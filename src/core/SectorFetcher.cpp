#include "core/SectorFetcher.h"

#include <algorithm>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QSemaphore>
#include <QSet>
#include <QStringList>
#include <QRegularExpression>
#include <QThread>
#include <QThreadPool>
#include <QAtomicInt>
#include <QDate>
#include <QSettings>
#include <QtConcurrent>
#include <QtMath>

#include "core/HttpClient.h"

namespace {

bool isMeaningfulSector(const QString &name)
{
    if (name.isEmpty()) return false;

    // 精确匹配：交易所/市场分区 / 纯分类标签（不是投资主题）
    static const QSet<QString> exactBlacklist = {
        "涨停", "跌停", "连板",
        "创业板", "科创板", "北交所",
        "ST板块", "次新股",
        "综合行业", "综合类", "其他行业", "其它行业",
    };
    if (exactBlacklist.contains(name)) return false;

    // contains 过滤：价格档位 / 振幅统计（这些词极少出现在真实投资主题名中）
    static const QStringList containsNoise = {
        "百元股", "低价股", "高价股", "亏损股", "破净股",
        "高振幅", "低振幅", "昨日高振幅", "昨日低振幅",
        "含一字",
    };
    for (const QString &kw : containsNoise) {
        if (name.contains(kw)) return false;
    }

    // 报告期切片标签：年报/季报/中报 出现在概念名中基本是噪声
    static const QRegularExpression timeSlice(
        QString::fromUtf8("(年报|季报|中报)"));
    if (timeSlice.match(name).hasMatch()) return false;

    // 时间前缀：昨日/今日/近N日 等短线标签
    static const QRegularExpression timePrefix(
        QString::fromUtf8("^(昨日|今日|明日|连续|近\\d+日|本周|本月|上周|上月)"));
    if (timePrefix.match(name).hasMatch()) return false;

    // 纯交易技术噪声（精确词组，避免误伤相关投资主题）
    static const QRegularExpression tradingNoise(
        QString::fromUtf8("(一字涨停|炸板|反包|地天板|天地方|连续涨停|连续跌停)"));
    if (tradingNoise.match(name).hasMatch()) return false;

    // 含下划线通常是交易标签衍生项，如"昨日涨停_含一字"
    if (name.contains('_')) return false;

    return true;
}

QString normalizeSectorName(QString name)
{
    name = name.trimmed();
    name.remove(' ');
    name.remove(QChar(0x3000)); // 全角空格

    static const QList<QPair<QString, QString>> aliasMap = {
        {"券商", "证券"},
        {"有色", "有色金属"},
        {"新能源车", "新能源汽车"},
        {"半导体及元件", "半导体"},
        {"白酒概念", "白酒"},
        {"创新药概念", "创新药"},
        {"人工智能概念", "人工智能"},
        {"算力概念", "算力"}
    };
    for (const auto &it : aliasMap) {
        if (name == it.first) {
            name = it.second;
            break;
        }
    }

    static const QStringList removableSuffix = {
        "板块", "行业", "概念", "指数", "III", "II", "I", "Ⅲ", "Ⅱ", "Ⅰ"
    };
    for (const QString &s : removableSuffix) {
        if (name.endsWith(s) && name.size() > s.size() + 1) {
            name.chop(s.size());
            break;
        }
    }

    return name.trimmed();
}

int sectorPriorityScore(const QString &name)
{
    // 白名单优先层：优先展示标准行业与核心可投资主题
    static const QStringList highPriorityKeywords = {
        "半导体", "芯片", "人工智能", "算力", "数据中心", "通信", "软件", "云计算",
        "机器人", "自动驾驶", "汽车", "新能源车", "锂电", "光伏", "风电", "储能",
        "电网", "军工", "商业航天", "医药", "创新药", "医疗器械", "消费", "食品饮料",
        "白酒", "家电", "有色金属", "黄金", "煤炭", "石油", "化工", "钢铁", "建材",
        "银行", "保险", "证券", "房地产", "基础设施", "电力", "公用事业", "港口", "航运"
    };
    static const QStringList midPriorityKeywords = {
        "产业链", "新材料", "工业", "制造", "国企改革", "数字经济", "信创", "低空经济",
        "跨境电商", "消费电子", "旅游", "农业", "养殖", "环保", "稀土", "创新",
        "机构重仓", "北向资金", "外资重仓", "中字头", "央企", "京津冀", "长三角", "粤港澳"
    };

    for (const QString &kw : highPriorityKeywords) {
        if (name.contains(kw)) return 3;
    }
    for (const QString &kw : midPriorityKeywords) {
        if (name.contains(kw)) return 2;
    }
    return 1;
}

QString sectorTierLabel(int tier)
{
    if (tier >= 3) return "核心池";
    if (tier == 2) return "主题池";
    return "观察池";
}

double computeHotScore(double changePct, double turnoverRate, int upCount, int downCount)
{
    const double volatility = qAbs(changePct);
    const double activity = qMin(turnoverRate, 30.0) / 30.0;
    const double breadth = static_cast<double>(upCount + downCount);
    return volatility * 0.35 + activity * 100.0 * 0.35 + qMin(breadth, 200.0) / 200.0 * 30.0;
}

struct SinaFlowEntry {
    QString name;
    double netAmount;
    double inAmount;
    double outAmount;
};

QHash<QString, SinaFlowEntry> fetchSinaFlowBatch()
{
    QHash<QString, SinaFlowEntry> result;

    auto parsePage = [&](const QString &url) {
        auto tryParse = [&](const QString &body) -> QJsonDocument {
            QJsonDocument d = QJsonDocument::fromJson(body.toUtf8());
            if (d.isArray()) return d;
            return QJsonDocument();
        };
        const auto r = HttpClient::get(url, 8000, 1);
        if (!r.ok || r.body.isEmpty()) return false;
        QJsonDocument doc = tryParse(r.body);
        if (!doc.isArray()) {
            const auto r2 = HttpClient::getGbk(url, 8000, 1);
            if (r2.ok && !r2.body.isEmpty())
                doc = tryParse(r2.body);
        }
        if (!doc.isArray()) return false;
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject o = v.toObject();
            SinaFlowEntry e;
            e.name = o.value("name").toString();
            e.netAmount = o.value("netamount").toString().toDouble();
            e.inAmount = o.value("inamount").toString().toDouble();
            e.outAmount = o.value("outamount").toString().toDouble();
            if (!e.name.isEmpty() && !result.contains(e.name))
                result.insert(e.name, e);
        }
        return doc.array().size() >= 40;
    };

    // fenlei=1 概念板块, fenlei=0 行业板块
    for (int fenlei : {1, 0}) {
        for (int page = 1; page <= 5; ++page) {
            const QString url = QString(
                "https://vip.stock.finance.sina.com.cn/quotes_service/api/json_v2.php/"
                "MoneyFlow.ssl_bkzj_bk?page=%1&num=40&sort=netamount&asc=0&fenlei=%2")
                .arg(page).arg(fenlei);
            if (!parsePage(url)) break;
        }
    }

    qDebug() << "[SectorFetcher] Sina资金流批量获取:" << result.size() << "条";
    return result;
}

QVector<KBar> fetchSinaBars(const QString &sinaSymbol, int datalen)
{
    QVector<KBar> bars;
    if (sinaSymbol.isEmpty()) return bars;
    const QString url = QString(
        "https://money.finance.sina.com.cn/quotes_service/api/json_v2.php/"
        "CN_MarketData.getKLineData?symbol=%1&scale=240&ma=no&datalen=%2")
        .arg(sinaSymbol).arg(datalen);
    const HttpClient::HttpResult r = HttpClient::get(url, 10000, 1);
    if (!r.ok || r.body.isEmpty() || r.body.trimmed() == "null") return bars;
    const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
    if (!doc.isArray()) return bars;
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject obj = v.toObject();
        KBar bar;
        bar.date   = obj.value("day").toString().left(10);
        bar.open   = obj.value("open").toString().toDouble();
        bar.high   = obj.value("high").toString().toDouble();
        bar.low    = obj.value("low").toString().toDouble();
        bar.close  = obj.value("close").toString().toDouble();
        bar.volume = obj.value("volume").toString().toDouble();
        if (bar.close > 0.0) bars.push_back(bar);
    }
    return bars;
}

QVector<double> closesFromBars(const QVector<KBar> &bars)
{
    QVector<double> closes;
    closes.reserve(bars.size());
    for (const KBar &b : bars) closes.push_back(b.close);
    return closes;
}

// 东方财富板块原生K线API —— 直接获取板块综合价格K线（非ETF代理）
QVector<KBar> fetchEmSectorDailyBars(const QString &emCode, int limit = 250)
{
    if (emCode.isEmpty()) return {};
    const QString url = QString(
        "https://push2his.eastmoney.com/api/qt/stock/kline/get?"
        "secid=90.%1&fields1=f1,f2,f3,f4,f5,f6"
        "&fields2=f51,f52,f53,f54,f55,f56,f57,f58,f59,f60,f61"
        "&klt=101&fqt=0&beg=0&end=20500101&lmt=%2")
        .arg(emCode).arg(limit);
    const auto r = HttpClient::get(url, 10000, 2);
    if (!r.ok || r.body.isEmpty()) {
        qDebug() << "[EmSectorKline] FAIL:" << emCode << r.errorMessage;
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
    const QJsonObject data = doc.object().value("data").toObject();
    if (data.isEmpty()) {
        qDebug() << "[EmSectorKline] data为空:" << emCode;
        return {};
    }
    const QJsonArray klines = data.value("klines").toArray();
    if (klines.isEmpty()) return {};
    QVector<KBar> bars;
    bars.reserve(klines.size());
    for (const QJsonValue &v : klines) {
        const QStringList parts = v.toString().split(',');
        // f51=日期 f52=开 f53=收 f54=高 f55=低 f56=成交量 f57=成交额 f58=振幅 f59=涨跌幅 f60=涨跌额 f61=换手率
        if (parts.size() < 9) continue;
        KBar b;
        b.date      = parts[0];
        b.open      = parts[1].toDouble();
        b.close     = parts[2].toDouble();
        b.high      = parts[3].toDouble();
        b.low       = parts[4].toDouble();
        b.volume    = parts[5].toDouble();
        b.amount    = parts[6].toDouble();
        b.changePct = parts[8].toDouble();
        if (parts.size() > 10) b.turnover = parts[10].toDouble();
        if (b.close > 0) bars.push_back(b);
    }
    qDebug() << "[EmSectorKline] OK:" << emCode << data.value("name").toString()
             << "bars:" << bars.size();
    return bars;
}

struct SinaEtfMapping { QString keyword; QString sinaSymbol; };
static const QList<SinaEtfMapping> kSinaEtfMap = {
    { "酿酒", "sz161725" }, { "白酒", "sz161725" },
    { "半导体", "sh512480" }, { "芯片", "sz159995" },
    { "光伏", "sh515790" }, { "新能源", "sh516160" }, { "锂电池", "sz159840" },
    { "医药", "sh512010" }, { "医疗", "sh512170" }, { "创新药", "sz159992" },
    { "中药", "sh560080" }, { "生物", "sh515120" },
    { "汽车", "sh516110" }, { "煤炭", "sh515220" }, { "钢铁", "sh515210" },
    { "有色", "sh512400" }, { "稀土", "sh516150" }, { "黄金", "sh518880" },
    { "石油", "sz162719" }, { "化工", "sh516020" },
    { "军工", "sh512660" }, { "银行", "sh512800" }, { "证券", "sh512880" },
    { "保险", "sh512070" }, { "金融", "sh512640" },
    { "房地产", "sh512200" }, { "地产", "sh512200" },
    { "建材", "sz159745" }, { "建筑", "sz159639" },
    { "电力", "sz159611" }, { "电子", "sz159997" }, { "家电", "sz159996" },
    { "食品", "sh515710" }, { "传媒", "sh512980" }, { "游戏", "sh516190" },
    { "旅游", "sz159766" }, { "消费", "sz159928" },
    { "交运", "sz159666" }, { "物流", "sz159666" }, { "交通", "sz159666" },
    { "机械", "sh516960" }, { "环保", "sh516570" }, { "通信", "sh515880" },
    { "计算机", "sh512720" }, { "软件", "sh512720" },
    { "人工智能", "sz159819" }, { "机器人", "sh562500" },
    { "储能", "sz159566" }, { "农业", "sz159825" },
    { "科技", "sh515000" }, { "互联网", "sh513050" },
    { "航空", "sz159819" }, { "港口", "sz159666" },
    { "电气", "sz159611" }, { "农林", "sz159825" },
    { "算力", "sz159530" }, { "数据中心", "sz159530" },
    { "风电", "sh516660" }, { "电网", "sz159611" },
    { "碳中和", "sz159790" }, { "自动驾驶", "sz159530" },
    { "云计算", "sh516510" }, { "信创", "sh562030" },
    { "数字经济", "sh562560" }, { "低空经济", "sz159730" },
    { "跨境电商", "sz159792" }, { "消费电子", "sz159732" },
    { "养殖", "sh516670" }, { "猪肉", "sh516670" },
    { "水泥", "sz159745" }, { "玻璃", "sz159514" },
    { "商业航天", "sh515060" }, { "航天", "sh515060" },
    { "纺织", "sz159928" }, { "服装", "sz159928" },
    { "电信", "sh515880" }, { "5G", "sh515050" },
    { "物联网", "sz159786" }, { "区块链", "sz159523" },
    { "网络安全", "sz159729" }, { "CRO", "sz159992" },
    { "中字头", "sh512270" }, { "央企", "sh512270" },
    { "一带一路", "sz159638" }, { "基建", "sz159638" },
    { "乡村振兴", "sz159825" }, { "养老", "sz159928" },
};

QString findSinaSymbol(const QString &sectorName)
{
    for (const SinaEtfMapping &m : kSinaEtfMap) {
        if (sectorName.contains(m.keyword)) return m.sinaSymbol;
    }
    return {};
}

QVector<double> synthesizeWeekly(const QVector<double> &daily)
{
    QVector<double> weekly;
    for (int i = 4; i < daily.size(); i += 5)
        weekly.push_back(daily[i]);
    if (!daily.isEmpty() && daily.size() % 5 != 0)
        weekly.push_back(daily.last());
    return weekly;
}

QVector<double> synthesizeMonthly(const QVector<double> &daily)
{
    QVector<double> monthly;
    for (int i = 21; i < daily.size(); i += 22)
        monthly.push_back(daily[i]);
    if (!daily.isEmpty() && daily.size() % 22 != 0)
        monthly.push_back(daily.last());
    return monthly;
}

} // namespace

// ========== 阶段 1: 仅拉取板块列表（极快） ==========
QList<SectorInfo> SectorFetcher::fetchSectorList() const
{
    QList<SectorInfo> sectors;
    QHash<QString, int> sectorNameToIndex;

    static const QStringList kTraceKeywords = {
        "半导体", "芯片", "光伏", "新能源", "医药", "白酒", "人工智能", "算力"
    };

    // 解析新浪 JS 变量格式的板块数据 (newSinaHy / newFLJK 都是同一格式)
    auto parseSinaVarData = [&](const QString &body, const QString &tag) -> int {
        int added = 0;
        const int eqPos = body.indexOf('{');
        const int endPos = body.lastIndexOf('}');
        if (eqPos < 0 || endPos <= eqPos) return 0;
        const QString text = body.mid(eqPos + 1, endPos - eqPos - 1);
        const QStringList entries = text.split("\",\"");
        for (const QString &entry : entries) {
            QString cleaned = entry;
            cleaned.remove('"');
            const QStringList parts = cleaned.split(",");
            // 格式: [0]代码 [1]名称 [2]成分股数 [3]均价 [4]均涨跌额 [5]涨跌幅%
            if (parts.size() < 6) continue;
            SectorInfo info;
            info.code = parts[0].trimmed();
            info.name = normalizeSectorName(parts[1].trimmed());
            if (info.name.isEmpty() || !isMeaningfulSector(info.name)) continue;
            info.changePct = parts[5].toDouble();
            info.upCount = parts[2].toInt();
            info.hotScore = computeHotScore(info.changePct, info.turnoverRate, info.upCount, 0);
            info.sourceTag = tag;
            info.sectorTier = sectorPriorityScore(info.name);
            info.sectorTierLabel = sectorTierLabel(info.sectorTier);
            info.sourceConsistencyScore = 70.0;

            const auto it = sectorNameToIndex.find(info.name);
            if (it == sectorNameToIndex.end()) {
                sectors.push_back(info);
                sectorNameToIndex.insert(info.name, sectors.size() - 1);
                ++added;
            } else {
                SectorInfo &old = sectors[it.value()];
                old.crossSourceValidated = true;
                old.altChangePct = info.changePct;
                old.hasAltChangePct = true;
                const double diff = qAbs(old.changePct - info.changePct);
                const double consistency = qBound(35.0, 100.0 - diff * 15.0, 98.0);
                old.sourceConsistencyScore = qMax(old.sourceConsistencyScore, consistency);
                if (old.eastmoneyCode.isEmpty() && info.hotScore > old.hotScore) {
                    old = info;
                }
            }
        }
        return added;
    };

    // ---- 源1: 新浪概念板块 (约175个，包含光伏/新能源/白酒/锂电池/机器人等) ----
    qDebug() << "[SectorFetcher] === 拉取新浪概念板块 (newFLJK) ===";
    {
        const HttpClient::HttpResult cr = HttpClient::getGbk(
            "http://money.finance.sina.com.cn/q/view/newFLJK.php?param=class", 12000, 2);
        if (cr.ok && !cr.body.isEmpty()) {
            const int added = parseSinaVarData(cr.body, QString::fromUtf8("新浪概念"));
            qDebug() << "[SectorFetcher] 新浪概念板块获取:" << added << "个";
        } else {
            qDebug() << "[SectorFetcher] 新浪概念板块请求失败:" << cr.errorMessage;
        }
    }

    // ---- 源2: 新浪行业板块 (约48个: 玻璃/传媒/电力/钢铁...) ----
    qDebug() << "[SectorFetcher] === 拉取新浪行业板块 (newSinaHy) ===";
    {
        const HttpClient::HttpResult sr = HttpClient::getGbk(
            "https://vip.stock.finance.sina.com.cn/q/view/newSinaHy.php", 10000, 1);
        if (sr.ok && !sr.body.isEmpty()) {
            const int added = parseSinaVarData(sr.body, QString::fromUtf8("新浪行业"));
            qDebug() << "[SectorFetcher] 新浪行业板块获取:" << added << "个";
        } else {
            qDebug() << "[SectorFetcher] 新浪行业板块请求失败:" << sr.errorMessage;
        }
    }

    // ---- 源3: 东方财富 push2 API (尝试, 可能因网络环境不可用) ----
    qDebug() << "[SectorFetcher] === 尝试东方财富 push2 API ===";
    {
        auto parseEmPage = [&](const QJsonArray &diff) -> int {
            int catCount = 0;
            for (const QJsonValue &v : diff) {
                const QJsonObject obj = v.toObject();
                const QString rawName = obj.value("f14").toString();
                const QString emCode = obj.value("f12").toString();
                const QString normalized = normalizeSectorName(rawName);
                if (normalized.isEmpty() || !isMeaningfulSector(normalized)) continue;
                SectorInfo info;
                info.eastmoneyCode = emCode;
                info.name = normalized;
                info.code = emCode;
                info.sourceTag = QString::fromUtf8("东方财富");
                info.changePct = obj.value("f3").toDouble();
                info.turnoverRate = obj.value("f8").toDouble();
                info.upCount = obj.value("f104").toInt();
                info.downCount = obj.value("f105").toInt();
                info.hotScore = computeHotScore(info.changePct, info.turnoverRate, info.upCount, info.downCount);
                info.sectorTier = sectorPriorityScore(info.name);
                info.sectorTierLabel = sectorTierLabel(info.sectorTier);
                info.sourceConsistencyScore = 85.0;
                const auto it = sectorNameToIndex.find(info.name);
                if (it == sectorNameToIndex.end()) {
                    sectors.push_back(info);
                    sectorNameToIndex.insert(info.name, sectors.size() - 1);
                    ++catCount;
                } else {
                    SectorInfo &old = sectors[it.value()];
                    old.eastmoneyCode = emCode;
                    old.crossSourceValidated = true;
                    old.altChangePct = info.changePct;
                    old.hasAltChangePct = true;
                    if (info.hotScore > old.hotScore) old = info;
                }
            }
            return catCount;
        };

        int emTotal = 0;
        for (const QString &fsParam : QStringList{"m:90+t:2", "m:90+t:3"}) {
            const QString emUrl = QString(
                "https://push2.eastmoney.com/api/qt/clist/get?"
                "pn=1&pz=2000&po=1&np=1&fltt=2&invt=2&fid=f3"
                "&fs=%1&fields=f2,f3,f4,f8,f12,f14,f104,f105").arg(fsParam);
            const HttpClient::HttpResult r = HttpClient::get(emUrl, 10000, 1);
            if (!r.ok || r.body.isEmpty()) {
                qDebug() << "[SectorFetcher] push2" << fsParam << "不可用:" << r.errorMessage;
                continue;
            }
            const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
            const QJsonArray diff = doc.object().value("data").toObject().value("diff").toArray();
            if (diff.isEmpty()) continue;
            const int added = parseEmPage(diff);
            emTotal += added;
            qDebug() << "[SectorFetcher] push2" << fsParam << "新增:" << added;
        }
        qDebug() << "[SectorFetcher] 东方财富 push2 总计新增:" << emTotal;
    }

    qDebug() << "[SectorFetcher] 多源合并后板块总数:" << sectors.size();

    // ---- 兜底：核心板块如果任何源都没拉到，用硬编码保障 ----
    // 注意: 这些代码来自东方财富push2 API (secid=90.BKxxxx)
    // 行业板块(t:3)和概念板块(t:2)使用不同的BK编码体系
    // 代码会随东方财富更新而变化，实际运行时以push2实时返回为准
    static const QList<QPair<QString, QString>> kEssentialSectors = {
        {"BK1036", "半导体"},
        {"BK1408", "机器人"},
        {"BK1131", "人工智能"},
        {"BK1155", "算力"},
        {"BK1011", "光伏"},
        {"BK0574", "锂电池"},
        {"BK0989", "储能"},
        {"BK1157", "数据中心"},
        {"BK0493", "军工"},
        {"BK0896", "白酒"},
        {"BK0465", "医药"},
        {"BK0478", "新能源"},
        {"BK0438", "消费"},
        {"BK0475", "银行"},
        {"BK0473", "证券"},
        {"BK0428", "房地产"},
        {"BK0474", "保险"},
        {"BK0437", "煤炭"},
        {"BK0478", "新能源汽车"},
        {"BK0447", "化工"},
        {"BK0448", "钢铁"},
        {"BK0466", "家电"},
    };
    int fallbackAdded = 0;
    for (const auto &pair : kEssentialSectors) {
        if (sectorNameToIndex.contains(pair.second)) continue;
        SectorInfo info;
        info.eastmoneyCode = pair.first;
        info.code = pair.first;
        info.name = pair.second;
        info.sourceTag = "兜底补充";
        info.sectorTier = sectorPriorityScore(info.name);
        info.sectorTierLabel = sectorTierLabel(info.sectorTier);
        info.sourceConsistencyScore = 50.0;
        sectors.push_back(info);
        sectorNameToIndex.insert(info.name, sectors.size() - 1);
        ++fallbackAdded;
        qDebug() << "[SectorFetcher] 兜底补充板块:" << info.name << info.eastmoneyCode;
    }
    if (fallbackAdded > 0)
        qDebug() << "[SectorFetcher] 兜底补充了" << fallbackAdded << "个核心板块";

    // 二次追踪验证
    for (const QString &kw : kTraceKeywords) {
        bool found = false;
        for (const SectorInfo &si : sectors) {
            if (si.name.contains(kw)) {
                found = true;
                qDebug() << "[TRACE-FINAL2] 找到:" << si.name << "代码:" << si.eastmoneyCode << "来源:" << si.sourceTag;
                break;
            }
        }
        if (!found)
            qDebug() << "[TRACE-FINAL2] !!! 即使兜底后仍未找到:" << kw;
    }

    std::sort(sectors.begin(), sectors.end(), [](const SectorInfo &a, const SectorInfo &b) {
        const int pa = a.sectorTier > 0 ? a.sectorTier : sectorPriorityScore(a.name);
        const int pb = b.sectorTier > 0 ? b.sectorTier : sectorPriorityScore(b.name);
        if (pa != pb) return pa > pb;
        if (qAbs(a.sourceConsistencyScore - b.sourceConsistencyScore) > 1e-6)
            return a.sourceConsistencyScore > b.sourceConsistencyScore;
        return a.hotScore > b.hotScore;
    });

    qDebug() << "[SectorFetcher] Sector list ready:" << sectors.size();
    return sectors;
}

// ========== 阶段 2: 并行填充 K线 + 资金流 ==========
void SectorFetcher::fetchMarketData(QList<SectorInfo> &sectors) const
{
    if (sectors.isEmpty()) return;

    // 批量获取资金流数据 (新浪MoneyFlow, 按板块名匹配)
    const QHash<QString, SinaFlowEntry> flowMap = fetchSinaFlowBatch();

    const int kDailyBars = 250;
    const int kMaxConcurrent = 4;
    QSemaphore semaphore(kMaxConcurrent);
    QAtomicInt requestCounter(0);

    struct FetchResult {
        int index;
        QVector<KBar> bars;
        QString source;
    };

    QMutex resultMutex;
    QVector<FetchResult> results;
    results.reserve(sectors.size());

    QList<QFuture<void>> futures;
    futures.reserve(sectors.size());

    for (int i = 0; i < sectors.size(); ++i) {
        const QString name = sectors[i].name;
        const QString emCode = sectors[i].eastmoneyCode;
        futures.push_back(QtConcurrent::run([i, name, emCode, kDailyBars,
                                              &semaphore, &resultMutex, &results,
                                              &requestCounter]() {
            semaphore.acquire();

            // 防限流: 每4个请求后稍作等待
            const int seq = requestCounter.fetchAndAddRelaxed(1);
            if (seq > 0 && seq % 4 == 0) {
                QThread::msleep(300);
            }

            FetchResult fr;
            fr.index = i;

            // 优先: 东方财富板块原生K线（真实板块综合价格）
            if (!emCode.isEmpty()) {
                fr.bars = fetchEmSectorDailyBars(emCode, kDailyBars);
                if (!fr.bars.isEmpty()) {
                    fr.source = QString::fromUtf8("东方财富板块");
                }
            }

            // 回退: 新浪ETF代理（仅当东方财富无数据时）
            if (fr.bars.isEmpty()) {
                const QString sinaSym = findSinaSymbol(name);
                if (!sinaSym.isEmpty()) {
                    fr.bars = fetchSinaBars(sinaSym, kDailyBars);
                    if (!fr.bars.isEmpty()) {
                        fr.source = QString::fromUtf8("新浪ETF代理");
                    }
                }
            }

            {
                QMutexLocker lock(&resultMutex);
                results.push_back(fr);
            }
            semaphore.release();
        }));
    }

    for (QFuture<void> &f : futures)
        f.waitForFinished();

    int klineEmOk = 0, klineEtfOk = 0, flowOk = 0;
    for (const FetchResult &fr : results) {
        SectorInfo &si = sectors[fr.index];
        const QVector<double> allCloses = closesFromBars(fr.bars);
        si.dailyBars = fr.bars;
        si.klineSeries = allCloses.size() > 60 ? allCloses.mid(allCloses.size() - 60) : allCloses;
        si.weekSeries  = synthesizeWeekly(allCloses);
        si.monthSeries = synthesizeMonthly(allCloses);
        si.klineSource = fr.source;
        if (!si.dailyBars.isEmpty()) {
            si.lastDataDate = si.dailyBars.last().date;
        }
        if (fr.source.contains(QString::fromUtf8("东方财富"))) ++klineEmOk;
        else if (!fr.bars.isEmpty()) ++klineEtfOk;

        // 用K线涨跌幅校验/补充实时changePct
        if (si.dailyBars.size() >= 2) {
            const double klinePct = si.dailyBars.last().changePct;
            if (fr.source.contains(QString::fromUtf8("东方财富"))) {
                // 东方财富板块K线的changePct是板块真实涨跌幅，优先采信
                if (qAbs(si.changePct) < 1e-9 || qAbs(si.changePct - klinePct) > 1.0) {
                    qDebug() << "[SectorFetcher] K线校正changePct:" << si.name
                             << "原=" << si.changePct << "% K线=" << klinePct << "%";
                    si.changePct = klinePct;
                }
            } else if (qAbs(si.changePct) < 1e-9) {
                // ETF代理：仅在无实时数据时才用ETF涨跌幅补充
                const double prev = si.dailyBars[si.dailyBars.size() - 2].close;
                const double curr = si.dailyBars.last().close;
                if (prev > 0) {
                    si.changePct = (curr - prev) / prev * 100.0;
                    qDebug() << "[SectorFetcher] ETF补充changePct:" << si.name << si.changePct << "%";
                }
            }
        }

        // 匹配资金流 (多策略名称匹配: 全称/子串/去除后缀)
        bool flowMatched = false;
        auto tryMatchFlow = [&](const QString &key) -> bool {
            if (key.contains(si.name) || si.name.contains(key)) return true;
            QString stripped = key;
            stripped = stripped.remove(QStringLiteral("概念"));
            stripped = stripped.remove(QStringLiteral("板块"));
            stripped = stripped.remove(QStringLiteral("行业"));
            stripped = stripped.trimmed();
            if (!stripped.isEmpty() && (stripped.contains(si.name) || si.name.contains(stripped)))
                return true;
            return false;
        };
        for (auto it = flowMap.begin(); it != flowMap.end(); ++it) {
            if (tryMatchFlow(it.key())) {
                const SinaFlowEntry &fe = it.value();
                const double netFlow = fe.netAmount / 1e8;
                si.fundFlowSeries.push_back(netFlow);
                si.fundFlowSource = "新浪资金流";
                ++flowOk;
                flowMatched = true;
                qDebug() << "[SectorFetcher] 资金流匹配:" << si.name << "<->" << it.key()
                         << "净流入:" << netFlow << "亿";
                break;
            }
        }

        // 读取/合并历史资金流数据（存储在QSettings中）
        {
            QSettings s("InvestInsight", "InvestInsight");
            const QString flowKey = "cache/flow_history_" + si.name;
            QJsonDocument histDoc = QJsonDocument::fromJson(s.value(flowKey).toString().toUtf8());
            QJsonArray histArr = histDoc.isArray() ? histDoc.array() : QJsonArray();

            if (flowMatched && !si.fundFlowSeries.isEmpty()) {
                const QString todayStr = QDate::currentDate().toString("yyyy-MM-dd");
                bool found = false;
                for (int hi = 0; hi < histArr.size(); ++hi) {
                    if (histArr[hi].toObject().value("d").toString() == todayStr) {
                        QJsonObject upd;
                        upd["d"] = todayStr;
                        upd["v"] = si.fundFlowSeries.last();
                        histArr[hi] = upd;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    QJsonObject newPt;
                    newPt["d"] = todayStr;
                    newPt["v"] = si.fundFlowSeries.last();
                    histArr.push_back(newPt);
                }
                while (histArr.size() > 60) histArr.removeFirst();
                s.setValue(flowKey, QString::fromUtf8(QJsonDocument(histArr).toJson(QJsonDocument::Compact)));
            }

            // 用历史数据填充 fundFlowSeries
            si.fundFlowSeries.clear();
            for (const QJsonValue &v : histArr) {
                si.fundFlowSeries.push_back(v.toObject().value("v").toDouble());
            }
            if (!si.fundFlowSeries.isEmpty() && si.fundFlowSource.isEmpty())
                si.fundFlowSource = "历史缓存";
        }

        if (!flowMatched && si.dailyBars.size() >= 2) {
            double volNetFlow = 0;
            for (int vi = qMax(0, si.dailyBars.size() - 20); vi < si.dailyBars.size(); ++vi) {
                const KBar &b = si.dailyBars[vi];
                double sign = (b.close >= b.open) ? 1.0 : -1.0;
                double dayFlow = sign * b.volume / 1e8;
                if (si.fundFlowSeries.isEmpty() || si.fundFlowSeries.size() <= (vi - qMax(0, si.dailyBars.size() - 20)))
                    si.fundFlowSeries.push_back(dayFlow);
            }
            if (!si.fundFlowSeries.isEmpty()) {
                si.fundFlowSource = "量价估算";
                volNetFlow = si.fundFlowSeries.last();
            }
        }
    }

    qDebug() << "[SectorFetcher] MarketData: 东方财富K线" << klineEmOk
             << "ETF代理K线" << klineEtfOk
             << "总" << (klineEmOk + klineEtfOk) << "/" << sectors.size()
             << "资金流" << flowOk;
}

// ========== 阶段 2.5: 估值 + 拥挤度（基于K线数据计算，不依赖push2） ==========
void SectorFetcher::fetchValuationData(QList<SectorInfo> &sectors) const
{
    if (sectors.isEmpty()) return;

    int valOk = 0;
    for (SectorInfo &si : sectors) {
        // 估值分位（基于K线历史价格分位）
        if (si.dailyBars.size() >= 20) {
            QVector<double> historicalCloses;
            historicalCloses.reserve(si.dailyBars.size());
            for (const KBar &b : si.dailyBars)
                historicalCloses.push_back(b.close);

            const double currentPrice = historicalCloses.last();
            std::sort(historicalCloses.begin(), historicalCloses.end());
            int rank = 0;
            for (double v : historicalCloses) {
                if (v < currentPrice) ++rank;
                else break;
            }
            si.pePercentile = static_cast<double>(rank) / historicalCloses.size() * 100.0;
            si.pbPercentile = si.pePercentile;
            si.valuationSource = "历史价格分位";
            valOk++;
        } else if (si.changePct != 0) {
            si.pePercentile = 50.0 + si.changePct * 2.0;
            si.pePercentile = qBound(5.0, si.pePercentile, 95.0);
            si.pbPercentile = si.pePercentile;
            si.valuationSource = "涨跌幅估算";
            valOk++;
        }

        // 拥挤热度
        if (si.dailyBars.size() >= 20) {
            double histTurnSum = 0;
            int turnCnt = 0;
            for (const KBar &b : si.dailyBars) {
                if (b.turnover > 0) { histTurnSum += b.turnover; ++turnCnt; }
            }
            const double histAvgTurn = turnCnt > 0 ? histTurnSum / turnCnt : 1.0;

            double recent5Turn = 0;
            const int n = si.dailyBars.size();
            int r5cnt = 0;
            for (int k = n - qMin(5, n); k < n; ++k) {
                if (si.dailyBars[k].turnover > 0) {
                    recent5Turn += si.dailyBars[k].turnover;
                    ++r5cnt;
                }
            }
            recent5Turn = r5cnt > 0 ? recent5Turn / r5cnt : histAvgTurn;
            si.avgTurnover5d = recent5Turn;

            double turnoverRatio = histAvgTurn > 0.01 ? recent5Turn / histAvgTurn : 1.0;

            QVector<double> histVols;
            for (const KBar &b : si.dailyBars) {
                if (b.volume > 0) histVols.push_back(b.volume);
            }
            double volPercentile = 50.0;
            if (histVols.size() >= 10 && n >= 1) {
                double recentVol = si.dailyBars.last().volume;
                std::sort(histVols.begin(), histVols.end());
                int volRank = 0;
                for (double v : histVols) {
                    if (v < recentVol) ++volRank;
                    else break;
                }
                volPercentile = static_cast<double>(volRank) / histVols.size() * 100.0;
            }

            double momAbs = 0;
            if (n >= 6) {
                momAbs = qAbs((si.dailyBars[n - 1].close - si.dailyBars[n - 6].close)
                    / si.dailyBars[n - 6].close * 100.0);
            }
            double momFactor = qMin(momAbs / 10.0, 1.0);

            double crowding = qMin(turnoverRatio / 2.0, 1.0) * 40.0
                + volPercentile * 0.35
                + momFactor * 25.0;
            si.crowdingIndex = qBound(0.0, crowding, 100.0);
        } else {
            si.avgTurnover5d = si.turnoverRate;
            const double turnFactor = qMin(si.turnoverRate / 10.0, 1.0);
            const double volFactor = qMin(qAbs(si.changePct) / 5.0, 1.0);
            si.crowdingIndex = qBound(0.0, turnFactor * 50.0 + volFactor * 50.0, 100.0);
        }

        si.missingDataItems.clear();
        if (si.klineSeries.size() < 20) si.missingDataItems.push_back("日线数据不足");
        if (si.weekSeries.size() < 4) si.missingDataItems.push_back("周线数据不足");
        if (si.monthSeries.size() < 3) si.missingDataItems.push_back("月线数据不足");
        if (si.fundFlowSeries.isEmpty()) si.missingDataItems.push_back("资金流缺失");
        if (si.lastDataDate.isEmpty()) si.missingDataItems.push_back("最新行情日期缺失");
    }

    qDebug() << "[SectorFetcher] Valuation: ok" << valOk << "/" << sectors.size();
}

QList<SectorInfo> SectorFetcher::fetchAll() const
{
    QList<SectorInfo> sectors = fetchSectorList();
    fetchMarketData(sectors);
    fetchValuationData(sectors);
    return sectors;
}
