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

// 腾讯K线API——支持ETF/股票/指数，稳定性最高
QVector<KBar> fetchTencentEtfBars(const QString &qqSymbol, int limit = 250)
{
    if (qqSymbol.isEmpty()) return {};
    const QString url = QString(
        "https://proxy.finance.qq.com/ifzqgtimg/appstock/app/fqkline/get?"
        "param=%1,day,,,%2,qfq").arg(qqSymbol).arg(limit);
    const auto r = HttpClient::get(url, 10000, 1);
    if (!r.ok || r.body.isEmpty()) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
    if (doc.isNull()) return {};
    const QJsonObject data = doc.object().value("data").toObject();
    const QJsonObject symData = data.value(qqSymbol).toObject();
    QJsonArray dayArr = symData.value("day").toArray();
    if (dayArr.isEmpty()) dayArr = symData.value("qfqday").toArray();
    if (dayArr.isEmpty()) return {};
    QVector<KBar> bars;
    bars.reserve(dayArr.size());
    for (const QJsonValue &v : dayArr) {
        const QJsonArray row = v.toArray();
        if (row.size() < 6) continue;
        KBar b;
        b.date   = row[0].toString();
        b.open   = row[1].toString().toDouble();
        b.close  = row[2].toString().toDouble();
        b.high   = row[3].toString().toDouble();
        b.low    = row[4].toString().toDouble();
        b.volume = row[5].toString().toDouble();
        if (b.close > 0) bars.push_back(b);
    }
    for (int i = 1; i < bars.size(); ++i) {
        const double prev = bars[i-1].close;
        if (prev > 0) bars[i].changePct = (bars[i].close - prev) / prev * 100.0;
    }
    return bars;
}

// 同花顺板块K线映射（板块名 -> THS板块代码）
// prefix: "bk" = 二级行业/概念板块，"hs" = 一级行业
struct ThsMapping { QString keyword; QString thsCode; QString prefix; };
static const QList<ThsMapping> kThsCodeMap = {
    // 一级行业 (hs_1Bxxxx) — 用户通常对照的大类
    { "有色金属",   "1B0819", "hs" }, { "有色",       "1B0819", "hs" },
    // 二级行业 (bk_881xxx)
    { "半导体",     "881121", "bk" }, { "煤炭",       "881105", "bk" },
    { "钢铁",       "881112", "bk" }, { "建筑材料",   "881115", "bk" }, { "建材",       "881115", "bk" },
    { "电力设备",   "881120", "bk" }, { "电力",       "881145", "bk" },
    { "光学",       "881122", "bk" }, { "消费电子",   "881124", "bk" },
    { "汽车",       "881125", "bk" }, { "汽车零部件", "881126", "bk" },
    { "通信设备",   "881129", "bk" }, { "通信",       "881129", "bk" },
    { "计算机",     "881130", "bk" }, { "计算机应用", "881130", "bk" },
    { "白色家电",   "881131", "bk" }, { "家电",       "881131", "bk" },
    { "食品饮料",   "881133", "bk" }, { "食品",       "881134", "bk" },
    { "纺织服装",   "881135", "bk" }, { "纺织",       "881135", "bk" },
    { "中药",       "881141", "bk" }, { "生物",       "881142", "bk" },
    { "医疗器械",   "881144", "bk" }, { "银行",       "881155", "bk" },
    { "保险",       "881156", "bk" }, { "证券",       "881157", "bk" },
    { "房地产",     "881153", "bk" }, { "地产",       "881153", "bk" },
    { "旅游",       "881160", "bk" }, { "文化传媒",   "881164", "bk" }, { "传媒",       "881164", "bk" },
    { "军工",       "881166", "bk" }, { "工业金属",   "881168", "bk" },
    { "贵金属",     "881169", "bk" }, { "黄金",       "881169", "bk" },
    { "小金属",     "881170", "bk" }, { "稀土",       "881170", "bk" },
    { "白酒",       "881273", "bk" }, { "游戏",       "881275", "bk" },
    { "软件",       "881272", "bk" }, { "化学原料",   "881108", "bk" },
    { "化学制品",   "881109", "bk" }, { "化工",       "881108", "bk" },
    { "养殖",       "881102", "bk" }, { "农业",       "881101", "bk" },
    { "环保",       "881181", "bk" }, { "物流",       "881152", "bk" },
    { "建筑",       "881116", "bk" }, { "电子",       "881123", "bk" },
    { "医药",       "881140", "bk" }, { "创新药",     "886015", "bk" },
    // 概念板块 (884xxx)
    { "光伏",       "884302", "bk" }, { "锂电池",     "884309", "bk" },
    { "风电",       "884307", "bk" }, { "水泥",       "884060", "bk" },
    { "玻璃",       "884059", "bk" }, { "铝",         "884053", "bk" },
    { "铜",         "884054", "bk" },
    // 概念板块 (885xxx)
    { "5G",         "885556", "bk" }, { "军工概念",   "885700", "bk" },
    { "消费电子概念", "885800", "bk" }, { "芯片",     "885756", "bk" },
    { "信创",       "886013", "bk" }, { "猪肉",       "885573", "bk" },
    { "核电",       "885571", "bk" }, { "无人机",     "885564", "bk" },
    // 概念板块 (886xxx)
    { "人工智能",   "886019", "bk" }, { "ChatGPT",    "886031", "bk" },
    { "机器人",     "886069", "bk" }, { "人形机器人", "886069", "bk" },
    { "低空经济",   "886067", "bk" }, { "商业航天",   "886078", "bk" },
    { "算力",       "886050", "bk" }, { "液冷",       "886044", "bk" },
    { "固态电池",   "886032", "bk" }, { "DeepSeek",   "886100", "bk" },
    { "AI应用",     "886108", "bk" }, { "AI智能体",   "886099", "bk" },
    { "存储",       "886042", "bk" }, { "光刻",       "886054", "bk" },
    { "储能",       "884312", "bk" }, { "新能源",     "884150", "bk" },
    { "充电桩",     "886001", "bk" }, { "碳中和",     "884302", "bk" },
    { "数据中心",   "886044", "bk" },
};

struct ThsCodeResult { QString code; QString prefix; };
ThsCodeResult findThsCode(const QString &sectorName)
{
    ThsCodeResult best;
    int bestLen = 0;
    for (const ThsMapping &m : kThsCodeMap) {
        if (sectorName.contains(m.keyword) && m.keyword.length() > bestLen) {
            best = { m.thsCode, m.prefix };
            bestLen = m.keyword.length();
        }
    }
    return best;
}

// 同花顺K线API: d.10jqka.com.cn (JSONP格式)
// prefix: "bk" = 板块, "hs" = 一级行业指数
QVector<KBar> fetchThsKline(const QString &thsCode, const QString &prefix = "bk", int limit = 250)
{
    if (thsCode.isEmpty()) return {};
    const QString url = QString("https://d.10jqka.com.cn/v4/line/%1_%2/01/last.js").arg(prefix, thsCode);
    const auto r = HttpClient::get(url, 8000, 1);
    if (!r.ok || r.body.isEmpty()) return {};

    // JSONP -> JSON: quotebridge_v4_line_bk_XXXXXX_01_last({...})
    const int lp = r.body.indexOf('(');
    const int rp = r.body.lastIndexOf(')');
    if (lp < 0 || rp <= lp) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(r.body.mid(lp + 1, rp - lp - 1).toUtf8());
    if (doc.isNull()) return {};
    const QJsonObject root = doc.object();
    const QString dataStr = root.value("data").toString();
    if (dataStr.isEmpty()) return {};

    const QStringList records = dataStr.split(';');
    QVector<KBar> bars;
    bars.reserve(records.size());
    for (const QString &rec : records) {
        const QStringList parts = rec.split(',');
        if (parts.size() < 5) continue;
        KBar b;
        // THS格式: date,open,high,low,close,volume,amount,...
        const QString dateRaw = parts[0].trimmed();
        if (dateRaw.length() == 8) {
            b.date = dateRaw.left(4) + "-" + dateRaw.mid(4, 2) + "-" + dateRaw.mid(6, 2);
        } else {
            b.date = dateRaw;
        }
        b.open   = parts[1].toDouble();
        b.high   = parts[2].toDouble();
        b.low    = parts[3].toDouble();
        b.close  = parts[4].toDouble();
        if (parts.size() > 5) b.volume = parts[5].toDouble();
        if (parts.size() > 6) b.amount = parts[6].toDouble();
        if (b.close > 0) bars.push_back(b);
    }
    // 计算changePct
    for (int i = 1; i < bars.size(); ++i) {
        const double prev = bars[i-1].close;
        if (prev > 0) bars[i].changePct = (bars[i].close - prev) / prev * 100.0;
    }
    if (bars.size() > limit) bars = bars.mid(bars.size() - limit);
    qDebug() << "[ThsKline] OK:" << thsCode << "name:" << root.value("name").toString()
             << "bars:" << bars.size();
    return bars;
}

struct ThsRealtimeResult {
    double changePct = 0.0;
    double preClose  = 0.0;
    double lastPrice = 0.0;
    QString date;
    bool valid = false;
};

static ThsRealtimeResult fetchThsRealtimeChangePct(const QString &thsCode, const QString &prefix = "bk")
{
    ThsRealtimeResult result;
    if (thsCode.isEmpty()) return result;
    const QString url = QString("https://d.10jqka.com.cn/v4/time/%1_%2/last.js").arg(prefix, thsCode);
    const auto r = HttpClient::get(url, 6000, 1);
    if (!r.ok || r.body.isEmpty()) return result;

    const int lp = r.body.indexOf('(');
    const int rp = r.body.lastIndexOf(')');
    if (lp < 0 || rp <= lp) return result;
    const QJsonDocument doc = QJsonDocument::fromJson(r.body.mid(lp + 1, rp - lp - 1).toUtf8());
    if (doc.isNull()) return result;

    const QJsonObject root = doc.object();
    const QJsonObject inner = root.begin().value().toObject();
    if (inner.isEmpty()) return result;

    result.preClose = inner.value("pre").toString().toDouble();
    result.date = inner.value("date").toString();
    const QString dataStr = inner.value("data").toString();
    if (result.preClose <= 0 || dataStr.isEmpty()) return result;

    const int lastSemi = dataStr.lastIndexOf(';');
    const QString lastPt = (lastSemi >= 0) ? dataStr.mid(lastSemi + 1) : dataStr;
    const QStringList fields = lastPt.split(',');
    if (fields.size() >= 2) {
        result.lastPrice = fields[1].toDouble();
        if (result.lastPrice > 0) {
            result.changePct = (result.lastPrice - result.preClose) / result.preClose * 100.0;
            result.valid = true;
        }
    }
    return result;
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
    // 酒类
    { "酿酒", "sz161725" }, { "白酒", "sz161725" },
    // 半导体&芯片
    { "半导体", "sh512480" }, { "芯片", "sz159995" },
    // 新能源
    { "光伏", "sh515790" }, { "新能源", "sh516160" }, { "锂电池", "sz159840" },
    { "风电", "sh516660" }, { "电网", "sz159611" }, { "碳中和", "sz159790" },
    { "储能", "sz159566" }, { "充电桩", "sz159515" },
    // 医药健康
    { "医药", "sh512010" }, { "医疗", "sh512170" }, { "创新药", "sz159992" },
    { "中药", "sh560080" }, { "生物", "sh515120" }, { "医疗器械", "sh516580" },
    { "CRO", "sz159992" },
    // 汽车&制造
    { "汽车", "sh516110" }, { "新能源汽车", "sh516110" },
    { "机械", "sh516960" }, { "电气", "sh516850" }, { "电气设备", "sh516850" },
    // 周期资源
    { "煤炭", "sh515220" }, { "钢铁", "sh515210" }, { "有色", "sh512400" },
    { "稀土", "sh516150" }, { "黄金", "sh518880" }, { "石油", "sz162719" },
    { "化工", "sh516020" }, { "建材", "sz159745" }, { "水泥", "sz159745" },
    { "玻璃", "sz159514" },
    // 金融
    { "军工", "sh512660" }, { "银行", "sh512800" }, { "证券", "sh512880" },
    { "保险", "sh512070" }, { "金融", "sh512640" },
    // 地产&基建
    { "房地产", "sh512200" }, { "地产", "sh512200" }, { "建筑", "sz159639" },
    { "基建", "sz159638" }, { "一带一路", "sz159638" },
    // 电力&电子
    { "电力", "sz159611" }, { "电子", "sz159997" }, { "家电", "sz159996" },
    // 消费
    { "食品", "sh515710" }, { "食品饮料", "sh515710" }, { "消费", "sz159928" },
    { "消费电子", "sz159732" }, { "旅游", "sz159766" },
    { "纺织", "sz159928" }, { "服装", "sz159928" }, { "养老", "sz159928" },
    // 传媒&娱乐
    { "传媒", "sh512980" }, { "游戏", "sh516190" },
    // 交通&物流
    { "交运", "sz159666" }, { "物流", "sz159666" }, { "交通", "sz159666" },
    { "航空", "sz159666" }, { "港口", "sz159666" },
    // 科技
    { "计算机", "sh512720" }, { "软件", "sh512720" }, { "计算机应用", "sh512720" },
    { "人工智能", "sz159819" }, { "机器人", "sh562500" },
    { "科技", "sh515000" }, { "互联网", "sh513050" },
    { "算力", "sz159530" }, { "数据中心", "sz159530" },
    { "自动驾驶", "sz159530" }, { "ChatGPT", "sz159819" },
    { "云计算", "sh516510" }, { "信创", "sh562030" },
    { "数字经济", "sh562560" }, { "低空经济", "sz159730" },
    // 电信&通信
    { "通信", "sh515880" }, { "电信", "sh515880" }, { "5G", "sh515050" },
    { "物联网", "sz159786" },
    // 安全&区块链
    { "区块链", "sz159523" }, { "网络安全", "sz159729" },
    // 农业
    { "农业", "sz159825" }, { "农林", "sz159825" }, { "乡村振兴", "sz159825" },
    { "养殖", "sh516670" }, { "猪肉", "sh516670" },
    // 军工&航天
    { "商业航天", "sh515060" }, { "航天", "sh515060" },
    // 央企&主题
    { "中字头", "sh512270" }, { "央企", "sh512270" },
    { "跨境电商", "sz159792" },
    // 环保
    { "环保", "sh516570" },
    // 其他概念板块
    { "稀缺资源", "sh512400" }, { "有色金属", "sh512400" },
    { "贵金属", "sh518880" }, { "大金融", "sh512640" },
    { "大消费", "sz159928" }, { "国防", "sh512660" },
    { "装备制造", "sh516960" }, { "工业母机", "sh516960" },
    { "新材料", "sh516020" }, { "化学", "sh516020" },
    { "能源", "sh516160" }, { "电池", "sz159840" },
    { "光学", "sz159997" }, { "显示", "sz159997" },
    { "车联网", "sz159530" }, { "智能驾驶", "sz159530" },
    { "无人驾驶", "sz159530" }, { "卫星", "sh515060" },
    { "导航", "sh515060" }, { "北斗", "sh515060" },
    { "华为", "sz159819" }, { "鸿蒙", "sz159819" },
    { "苹果", "sz159732" }, { "特斯拉", "sh516110" },
    { "元宇宙", "sh516190" }, { "虚拟现实", "sh516190" },
    { "VR", "sh516190" }, { "AR", "sh516190" },
    { "OLED", "sz159997" }, { "面板", "sz159997" },
    { "PCB", "sz159997" }, { "被动元件", "sz159997" },
    { "功率", "sh512480" }, { "第三代半导体", "sh512480" },
    { "EDA", "sh512480" }, { "封测", "sh512480" },
    { "光刻", "sh512480" },
    { "存储", "sz159530" }, { "内存", "sz159530" },
    { "固态电池", "sz159840" }, { "钠离子", "sz159840" },
    { "氢能", "sz159566" }, { "绿电", "sz159611" },
    { "核电", "sz159611" }, { "火电", "sz159611" },
    { "天然气", "sz162719" }, { "油气", "sz162719" },
    { "铜", "sh512400" }, { "铝", "sh512400" }, { "锌", "sh512400" },
    { "锂", "sh516150" }, { "钴", "sh516150" }, { "镍", "sh512400" },
    { "白银", "sh518880" },
    { "养猪", "sh516670" }, { "畜牧", "sh516670" },
    { "种业", "sz159825" }, { "转基因", "sz159825" },
    { "粮食", "sz159825" }, { "大豆", "sz159825" },
    { "教育", "sz159928" }, { "在线教育", "sz159928" },
    { "酒店", "sz159766" }, { "餐饮", "sz159766" },
    { "快递", "sz159666" }, { "航运", "sz159666" },
    { "铁路", "sz159666" }, { "公路", "sz159666" },
    { "地铁", "sz159666" }, { "城轨", "sz159666" },
    { "工程", "sz159639" }, { "装修", "sz159745" }, { "家装", "sz159745" },
    { "家居", "sz159996" }, { "小家电", "sz159996" },
    { "白色家电", "sz159996" }, { "厨电", "sz159996" },
    { "乳业", "sh515710" }, { "调味品", "sh515710" },
    { "预制菜", "sh515710" }, { "零食", "sh515710" },
    { "啤酒", "sh515710" }, { "饮料", "sh515710" },
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
            info.changePctValid = true;
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
                // 新浪内部合并：补充字段，但不做整行替换以保持changePct稳定
                if (old.eastmoneyCode.isEmpty() && info.eastmoneyCode.isEmpty()) {
                    if (old.upCount == 0 && info.upCount > 0) old.upCount = info.upCount;
                    if (!old.changePctValid && info.changePctValid) {
                        old.changePct = info.changePct;
                        old.changePctValid = true;
                    }
                }
            }
        }
        return added;
    };

    // ---- 源1: 新浪概念板块 (约175个，包含光伏/新能源/白酒/锂电池/机器人等) ----
    qDebug() << "[SectorFetcher] === 拉取新浪概念板块 (newFLJK) ===";
    {
        const HttpClient::HttpResult cr = HttpClient::getGbk(
            "https://money.finance.sina.com.cn/q/view/newFLJK.php?param=class", 12000, 2);
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
                info.changePctValid = true;
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
                    old.crossSourceValidated = true;
                    old.altChangePct = old.changePct;
                    old.hasAltChangePct = true;
                    // 东方财富数据更权威：采信其changePct和结构化字段
                    old.eastmoneyCode = emCode;
                    old.changePct = info.changePct;
                    old.changePctValid = true;
                    old.turnoverRate = info.turnoverRate;
                    old.upCount = info.upCount;
                    old.downCount = info.downCount;
                    old.hotScore = info.hotScore;
                    old.sourceTag = info.sourceTag;
                    old.sourceConsistencyScore = qMax(old.sourceConsistencyScore, info.sourceConsistencyScore);
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
        // 科技
        {"BK1036", "半导体"}, {"BK1131", "人工智能"}, {"BK1155", "算力"},
        {"BK1157", "数据中心"}, {"BK1408", "机器人"}, {"BK0720", "芯片"},
        {"BK0849", "云计算"}, {"BK0891", "信创"}, {"BK1009", "数字经济"},
        {"BK0930", "物联网"}, {"BK0929", "网络安全"}, {"BK1145", "ChatGPT"},
        {"BK0715", "计算机应用"}, {"BK0802", "5G"},
        // 新能源
        {"BK1011", "光伏"}, {"BK0574", "锂电池"}, {"BK0989", "储能"},
        {"BK0478", "新能源"}, {"BK0485", "新能源汽车"}, {"BK0814", "风电"},
        {"BK1097", "碳中和"}, {"BK0850", "充电桩"},
        // 消费&医药
        {"BK0896", "白酒"}, {"BK0438", "消费"}, {"BK0465", "医药"},
        {"BK0808", "创新药"}, {"BK0659", "中药"}, {"BK0826", "医疗器械"},
        {"BK0816", "食品饮料"}, {"BK0466", "家电"}, {"BK0482", "旅游"},
        {"BK0764", "消费电子"},
        // 金融
        {"BK0475", "银行"}, {"BK0473", "证券"}, {"BK0474", "保险"},
        {"BK0476", "金融"},
        // 周期
        {"BK0437", "煤炭"}, {"BK0448", "钢铁"}, {"BK0447", "化工"},
        {"BK0428", "房地产"}, {"BK0477", "有色金属"}, {"BK0484", "稀土"},
        {"BK0868", "黄金"}, {"BK0468", "石油"}, {"BK0460", "电力"},
        {"BK0446", "建材"}, {"BK0500", "水泥"},
        // 军工&航天
        {"BK0493", "军工"}, {"BK0953", "商业航天"}, {"BK1191", "低空经济"},
        // 传媒&互联网
        {"BK0456", "传媒"}, {"BK0832", "游戏"}, {"BK0796", "互联网"},
        // 交通&物流
        {"BK0479", "汽车"}, {"BK0494", "交运"}, {"BK0487", "电气设备"},
        // 农业
        {"BK0480", "农业"}, {"BK0843", "养殖"},
        // 其他
        {"BK0969", "区块链"}, {"BK0713", "通信"}, {"BK0459", "纺织服装"},
        {"BK0469", "机械"}, {"BK0455", "环保"}, {"BK0462", "建筑"},
        {"BK0483", "电子"}, {"BK1052", "中字头"},
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
    const int kMaxConcurrent = 3;
    QSemaphore semaphore(kMaxConcurrent);
    QAtomicInt requestCounter(0);

    struct FetchResult {
        int index;
        QVector<KBar> bars;
        QString source;
        double thsChangePct = 0.0;
        bool   thsChangePctValid = false;
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

            const int seq = requestCounter.fetchAndAddRelaxed(1);
            if (seq > 0 && seq % 3 == 0) {
                QThread::msleep(150);
            }

            FetchResult fr;
            fr.index = i;

            // === 最高优先: 同花顺板块K线（用于图表）+ 实时分时（用于changePct）===
            const ThsCodeResult thsResult = findThsCode(name);
            if (!thsResult.code.isEmpty()) {
                QVector<KBar> thsBars = fetchThsKline(thsResult.code, thsResult.prefix, kDailyBars);
                if (thsBars.size() >= 2) {
                    fr.bars = thsBars;
                    fr.source = QString::fromUtf8("同花顺板块");

                    const QString todayStr = QDate::currentDate().toString("yyyyMMdd");
                    const QString lastBarDate = thsBars.last().date.remove('-');
                    if (lastBarDate == todayStr) {
                        const double prevClose = thsBars[thsBars.size() - 2].close;
                        if (prevClose > 0) {
                            fr.thsChangePct = (thsBars.last().close - prevClose) / prevClose * 100.0;
                            fr.thsChangePctValid = true;
                        }
                    }
                }
                if (!fr.thsChangePctValid) {
                    const ThsRealtimeResult rt = fetchThsRealtimeChangePct(thsResult.code, thsResult.prefix);
                    if (rt.valid) {
                        fr.thsChangePct = rt.changePct;
                        fr.thsChangePctValid = true;
                    }
                }
            }

            // 回退1: 腾讯ETF代理（稳定性最高，几乎不限流）
            const QString etfSym = findSinaSymbol(name);
            if (fr.bars.isEmpty() && !etfSym.isEmpty()) {
                fr.bars = fetchTencentEtfBars(etfSym, kDailyBars);
                if (!fr.bars.isEmpty()) {
                    fr.source = QString::fromUtf8("腾讯ETF代理");
                }
            }

            // 回退2: 东方财富板块原生K线
            if (fr.bars.isEmpty() && !emCode.isEmpty()) {
                fr.bars = fetchEmSectorDailyBars(emCode, kDailyBars);
                if (!fr.bars.isEmpty()) {
                    fr.source = QString::fromUtf8("东方财富板块");
                }
            }

            // 回退3: 新浪ETF代理
            if (fr.bars.isEmpty() && !etfSym.isEmpty()) {
                fr.bars = fetchSinaBars(etfSym, kDailyBars);
                if (!fr.bars.isEmpty()) {
                    fr.source = QString::fromUtf8("新浪ETF代理");
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

    int klineThsOk = 0, klineEmOk = 0, klineTencentOk = 0, klineSinaOk = 0, flowOk = 0;
    int thsChangePctCorrected = 0;
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
        if (fr.source.contains(QString::fromUtf8("同花顺"))) ++klineThsOk;
        else if (fr.source.contains(QString::fromUtf8("东方财富"))) ++klineEmOk;
        else if (fr.source.contains(QString::fromUtf8("腾讯"))) ++klineTencentOk;
        else if (fr.source.contains(QString::fromUtf8("新浪"))) ++klineSinaOk;

        // 同花顺changePct最权威：当THS给出了有效的changePct时，用它覆盖
        if (fr.thsChangePctValid) {
            if (si.changePctValid && qAbs(si.changePct - fr.thsChangePct) > 0.5) {
                qDebug() << "[SectorFetcher] THS修正changePct:" << si.name
                         << "旧值:" << si.changePct << "% 新值:" << fr.thsChangePct << "%";
            }
            si.changePct = fr.thsChangePct;
            si.changePctValid = true;
            ++thsChangePctCorrected;
        }

        // K线仅在changePct仍缺失 且 K线含当日bar时才补充
        if (!si.changePctValid && si.dailyBars.size() >= 2) {
            const QString today = QDate::currentDate().toString("yyyy-MM-dd");
            const QString lastBarDate = si.dailyBars.last().date;
            if (lastBarDate == today) {
                si.changePct = si.dailyBars.last().changePct;
                si.changePctValid = true;
                qDebug() << "[SectorFetcher] K线补充changePct(当日bar):" << si.name << si.changePct << "%";
            } else {
                qDebug() << "[SectorFetcher] K线非当日，不补充changePct:" << si.name
                         << "(K线末日:" << lastBarDate << " 今日:" << today << ")";
            }
        }

        // changePct仍无效时，通过ETF代理的腾讯实时行情补充
        if (!si.changePctValid) {
            const QString etfSym = findSinaSymbol(si.name);
            if (!etfSym.isEmpty()) {
                const QString qqCode = etfSym;
                const QString rtUrl = "http://qt.gtimg.cn/q=" + qqCode;
                const auto rtResult = HttpClient::getGbk(rtUrl, 5000, 1);
                if (rtResult.ok && !rtResult.body.isEmpty()) {
                    const QStringList parts = rtResult.body.split('~');
                    if (parts.size() > 32) {
                        si.changePct = parts[32].toDouble();
                        si.changePctValid = true;
                        qDebug() << "[SectorFetcher] ETF实时补充changePct:" << si.name
                                 << "ETF=" << qqCode << si.changePct << "%";
                    }
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
                si.fundFlowSource = QString::fromUtf8("新浪资金流");
                ++flowOk;
                flowMatched = true;
                break;
            }
        }

        // 读取/合并历史实时资金流缓存
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

            si.fundFlowSeries.clear();
            for (const QJsonValue &v : histArr) {
                si.fundFlowSeries.push_back(v.toObject().value("v").toDouble());
            }
            if (!si.fundFlowSeries.isEmpty() && si.fundFlowSource.isEmpty())
                si.fundFlowSource = QString::fromUtf8("历史缓存");
        }

        // 用K线量价数据补齐资金流至60条（向前填充）
        {
            const int targetLen = qMin(si.dailyBars.size(), 60);
            const int existing  = si.fundFlowSeries.size();
            if (existing < targetLen && si.dailyBars.size() >= 2) {
                const int needPrepend = targetLen - existing;
                const int barStart = si.dailyBars.size() - targetLen;
                QVector<double> estimated;
                estimated.reserve(needPrepend);
                for (int vi = barStart; vi < barStart + needPrepend; ++vi) {
                    if (vi < 0 || vi >= si.dailyBars.size()) {
                        estimated.push_back(0.0);
                        continue;
                    }
                    const KBar &b = si.dailyBars[vi];
                    double sign = (b.close >= b.open) ? 1.0 : -1.0;
                    estimated.push_back(sign * b.volume / 1e8);
                }
                estimated.append(si.fundFlowSeries);
                si.fundFlowSeries = estimated;
                if (si.fundFlowSource.isEmpty())
                    si.fundFlowSource = QString::fromUtf8("量价估算");
                else if (!si.fundFlowSource.contains(QString::fromUtf8("量价补齐")))
                    si.fundFlowSource += QString::fromUtf8("+量价补齐");
            }
        }
    }

    const int totalKline = klineThsOk + klineEmOk + klineTencentOk + klineSinaOk;
    int flowFull = 0, flowPartial = 0, flowEmpty = 0;
    for (const auto &si : sectors) {
        if (si.fundFlowSeries.size() >= 60) ++flowFull;
        else if (!si.fundFlowSeries.isEmpty()) ++flowPartial;
        else ++flowEmpty;
    }
    qDebug() << "[SectorFetcher] MarketData: 同花顺K线" << klineThsOk
             << "东方财富K线" << klineEmOk
             << "腾讯K线" << klineTencentOk
             << "新浪K线" << klineSinaOk
             << "总" << totalKline << "/" << sectors.size()
             << "THS修正changePct" << thsChangePctCorrected
             << "资金流实时" << flowOk
             << "资金流≥60:" << flowFull
             << "部分:" << flowPartial
             << "缺失:" << flowEmpty;
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
        } else {
            // K线不足且无真实PE数据时，保持默认50（中性），不做伪估算
            si.valuationSource = "数据不足";
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
            // K线不足，拥挤度保持默认50（中性），不做伪估算
            si.avgTurnover5d = si.turnoverRate;
            si.crowdingIndex = 50.0;
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
