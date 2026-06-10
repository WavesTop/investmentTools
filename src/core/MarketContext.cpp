#include "core/MarketContext.h"
#include "core/HttpClient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDateTime>
#include <QTimeZone>
#include <QtMath>

namespace {

QVector<double> synthesizeWeekly(const QVector<double> &daily)
{
    QVector<double> weekly;
    for (int i = 4; i < daily.size(); i += 5) weekly.push_back(daily[i]);
    if (!daily.isEmpty() && daily.size() % 5 != 0) weekly.push_back(daily.last());
    return weekly;
}

QVector<double> synthesizeMonthly(const QVector<double> &daily)
{
    QVector<double> monthly;
    for (int i = 21; i < daily.size(); i += 22) monthly.push_back(daily[i]);
    if (!daily.isEmpty() && daily.size() % 22 != 0) monthly.push_back(daily.last());
    return monthly;
}

QVector<KBar> fetchTencentIndexDailyBars(const QString &qqSymbol, int limit = 130)
{
    const QString url = QString(
        "https://proxy.finance.qq.com/ifzqgtimg/appstock/app/fqkline/get?"
        "param=%1,day,,,%2,qfq").arg(qqSymbol).arg(limit);
    const auto r = HttpClient::get(url, 10000, 2);
    if (!r.ok || r.body.isEmpty()) {
        qDebug() << "[TencentKline] FAIL:" << qqSymbol << r.errorMessage;
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
    if (doc.isNull()) {
        qDebug() << "[TencentKline] JSON解析失败:" << qqSymbol << "body_start:" << r.body.left(200);
        return {};
    }
    const QJsonObject data = doc.object().value("data").toObject();
    const QJsonObject symData = data.value(qqSymbol).toObject();
    QJsonArray dayArr = symData.value("day").toArray();
    if (dayArr.isEmpty()) dayArr = symData.value("qfqday").toArray();
    if (dayArr.isEmpty()) {
        qDebug() << "[TencentKline] day数组为空:" << qqSymbol
                 << "data_keys:" << data.keys()
                 << "sym_keys:" << symData.keys();
        return {};
    }
    QVector<KBar> bars;
    bars.reserve(dayArr.size());
    for (const QJsonValue &v : dayArr) {
        const QJsonArray row = v.toArray();
        if (row.size() < 6) continue;
        KBar b;
        b.date = row[0].toString();
        b.open = row[1].toString().toDouble();
        b.close = row[2].toString().toDouble();
        b.high = row[3].toString().toDouble();
        b.low = row[4].toString().toDouble();
        b.volume = row[5].toString().toDouble();
        if (b.close > 0) bars.push_back(b);
    }
    for (int i = 1; i < bars.size(); ++i) {
        const double prev = bars[i-1].close;
        if (prev > 0) bars[i].changePct = (bars[i].close - prev) / prev * 100.0;
    }
    qDebug() << "[TencentKline] OK:" << qqSymbol << "bars:" << bars.size();
    return bars;
}

QVector<KBar> fetchSinaIndexDailyBars(const QString &sinaSymbol, int limit = 130)
{
    const QString url = QString(
        "https://money.finance.sina.com.cn/quotes_service/api/json_v2.php/"
        "CN_MarketData.getKLineData?symbol=%1&scale=240&ma=no&datalen=%2")
        .arg(sinaSymbol).arg(limit);
    const auto r = HttpClient::get(url, 10000, 1);
    if (!r.ok || r.body.isEmpty() || r.body.trimmed() == "null") {
        qDebug() << "[SinaIndexKline] FAIL:" << sinaSymbol << r.ok << r.body.size() << r.errorMessage;
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
    if (!doc.isArray()) {
        qDebug() << "[SinaIndexKline] 非数组:" << sinaSymbol << "body_start:" << r.body.left(200);
        return {};
    }
    QVector<KBar> bars;
    bars.reserve(doc.array().size());
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        KBar b;
        b.date = o.value("day").toString().left(10);
        b.open = o.value("open").toString().toDouble();
        b.high = o.value("high").toString().toDouble();
        b.low = o.value("low").toString().toDouble();
        b.close = o.value("close").toString().toDouble();
        b.volume = o.value("volume").toString().toDouble();
        if (b.close > 0) bars.push_back(b);
    }
    for (int i = 1; i < bars.size(); ++i) {
        const double prev = bars[i-1].close;
        if (prev > 0) bars[i].changePct = (bars[i].close - prev) / prev * 100.0;
    }
    qDebug() << "[SinaIndexKline] OK:" << sinaSymbol << "bars:" << bars.size();
    return bars;
}

QVector<KBar> fetchYahooDailyBars(const QString &symbol, const QString &range = "6mo")
{
    const QString url = QString(
        "https://query1.finance.yahoo.com/v8/finance/chart/%1?interval=1d&range=%2")
        .arg(symbol, range);
    const auto r = HttpClient::get(url, 8000, 1);
    if (!r.ok || r.body.isEmpty()) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
    const QJsonArray resultArr = doc.object().value("chart").toObject().value("result").toArray();
    if (resultArr.isEmpty()) return {};
    const QJsonObject result = resultArr[0].toObject();
    if (result.isEmpty()) return {};
    const QJsonArray ts = result.value("timestamp").toArray();
    const QJsonArray quoteArr = result.value("indicators").toObject().value("quote").toArray();
    if (quoteArr.isEmpty()) return {};
    const QJsonObject quote = quoteArr[0].toObject();
    const QJsonArray opens = quote.value("open").toArray();
    const QJsonArray highs = quote.value("high").toArray();
    const QJsonArray lows = quote.value("low").toArray();
    const QJsonArray closes = quote.value("close").toArray();
    const QJsonArray vols = quote.value("volume").toArray();
    const int n = qMin(ts.size(), qMin(opens.size(), qMin(highs.size(), qMin(lows.size(), closes.size()))));
    QVector<KBar> bars;
    bars.reserve(n);
    for (int i = 0; i < n; ++i) {
        if (closes[i].isNull()) continue;
        KBar b;
        const qint64 t = static_cast<qint64>(ts[i].toDouble());
        b.date = QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).date().toString("yyyy-MM-dd");
        b.open = opens[i].toDouble();
        b.high = highs[i].toDouble();
        b.low = lows[i].toDouble();
        b.close = closes[i].toDouble();
        b.volume = i < vols.size() ? vols[i].toDouble() : 0.0;
        if (b.close > 0) bars.push_back(b);
    }
    return bars;
}

void fillIndexSeries(IndexSnapshot &s)
{
    QVector<double> closes;
    closes.reserve(s.dailyBars.size());
    for (const KBar &b : s.dailyBars) closes.push_back(b.close);
    s.klineSeries = closes.size() > 90 ? closes.mid(closes.size() - 90) : closes;
    s.weekSeries = synthesizeWeekly(closes);
    s.monthSeries = synthesizeMonthly(closes);
}

} // namespace

MarketContext MarketContextFetcher::fetch() const
{
    MarketContext ctx;
    fetchIndices(ctx);
    fetchUSIndices(ctx);   // 美股（与A股分开，避免影响valid判断）
    fetchNorthbound(ctx);
    fetchMarketBreadth(ctx);
    computeRiskScore(ctx);
    ctx.valid = ctx.shanghai.lastClose > 0;
    qDebug() << "[MarketContext] valid:" << ctx.valid
             << "SH:" << ctx.shanghai.changePct << "% NB:" << ctx.northboundNetBuy << "亿"
             << "AD:" << ctx.advanceCount << "/" << ctx.declineCount
             << "risk:" << ctx.marketRiskScore << ctx.riskLevel;
    return ctx;
}

void MarketContextFetcher::fetchIndices(MarketContext &ctx) const
{
    // 使用腾讯 qt.gtimg.cn 获取实时行情, proxy.finance.qq.com / 新浪获取K线
    struct IdxDef {
        QString qqSymbol;   // 腾讯行情代码
        QString qqKline;    // 腾讯K线代码
        QString sinaSymbol; // 新浪代码 (K线回退)
        IndexSnapshot *snap;
        QString name;
    };
    QList<IdxDef> defs = {
        { "sh000001", "sh000001", "sh000001", &ctx.shanghai,  "上证指数" },
        { "sz399001", "sz399001", "sz399001", &ctx.shenzhen,  "深证成指" },
        { "sz399006", "sz399006", "sz399006", &ctx.chinext,   "创业板指" },
        { "sh000300", "sh000300", "sh000300", &ctx.csi300,    "沪深300" },
        { "sh000905", "sh000905", "sh000905", &ctx.csi500,    "中证500" },
    };

    // 批量获取实时行情 (腾讯 qt.gtimg.cn)
    QStringList qqCodes;
    for (const auto &d : defs) qqCodes.push_back(d.qqSymbol);
    const QString rtUrl = "http://qt.gtimg.cn/q=" + qqCodes.join(",");
    const auto rtResult = HttpClient::getGbk(rtUrl, 8000, 1);
    if (rtResult.ok && !rtResult.body.isEmpty()) {
        // 格式: v_sh000001="1~上证指数~000001~4107.51~4078.64~..."
        for (int i = 0; i < defs.size(); ++i) {
            const QString tag = "v_" + defs[i].qqSymbol + "=\"";
            const int pos = rtResult.body.indexOf(tag);
            if (pos < 0) continue;
            const int start = pos + tag.size();
            const int end = rtResult.body.indexOf("\"", start);
            if (end <= start) continue;
            const QString line = rtResult.body.mid(start, end - start);
            const QStringList parts = line.split('~');
            // parts[1]=名称, parts[3]=当前价, parts[4]=昨收, parts[5]=开盘, parts[31]=涨跌额, parts[32]=涨跌幅
            IndexSnapshot &s = *defs[i].snap;
            s.name = defs[i].name;
            s.code = defs[i].qqSymbol;
            if (parts.size() > 3) s.lastClose = parts[3].toDouble();
            if (parts.size() > 32) {
                s.changePct = parts[32].toDouble();
                s.changePctValid = true;
            }
            if (parts.size() > 37) s.amount = parts[37].toDouble();
        }
    }

    // 获取 K线（腾讯优先，新浪回退），仅用于历史序列，不覆盖实时行情
    for (const auto &d : defs) {
        IndexSnapshot &s = *d.snap;
        const double rtClose = s.lastClose;
        const double rtPct   = s.changePct;
        const bool   hasRt   = rtClose > 0;

        s.dailyBars = fetchTencentIndexDailyBars(d.qqKline, 130);
        if (s.dailyBars.isEmpty()) {
            s.dailyBars = fetchSinaIndexDailyBars(d.sinaSymbol, 130);
        }
        if (!s.dailyBars.isEmpty()) {
            fillIndexSeries(s);
            if (hasRt) {
                s.lastClose = rtClose;
                s.changePct = rtPct;
                // changePctValid 已在实时获取阶段设置
            } else {
                // 无实时行情，K线推算不标记valid（可能非当日数据）
                const QString today = QDate::currentDate().toString("yyyy-MM-dd");
                s.lastClose = s.dailyBars.last().close;
                if (s.dailyBars.size() >= 2) {
                    const double prev = s.dailyBars[s.dailyBars.size() - 2].close;
                    if (prev > 0) s.changePct = (s.lastClose - prev) / prev * 100.0;
                }
                s.changePctValid = (s.dailyBars.last().date == today);
            }
        }
        qDebug() << "[Index]" << s.name << s.lastClose << s.changePct << "% bars:" << s.dailyBars.size()
                 << (hasRt ? "实时" : "K线推算");
    }
}

void MarketContextFetcher::fetchUSIndices(MarketContext &ctx) const
{
    // 使用 Yahoo Finance chart API 获取美股指数（含历史序列）
    struct USDef { QString symbol; IndexSnapshot *snap; QString name; };
    const QList<USDef> defs = {
        { "^IXIC", &ctx.nasdaq,    "纳斯达克" },
        { "^GSPC", &ctx.sp500,     "标普500"  },
        { "^DJI",  &ctx.dowjones,  "道琼斯"   },
    };

    for (const USDef &d : defs) {
        d.snap->name = d.name;
        d.snap->code = d.symbol;
        d.snap->dailyBars = fetchYahooDailyBars(d.symbol, "6mo");
        if (d.snap->dailyBars.isEmpty()) continue;
        d.snap->lastClose = d.snap->dailyBars.last().close;
        if (d.snap->dailyBars.size() >= 2) {
            const double prev = d.snap->dailyBars[d.snap->dailyBars.size() - 2].close;
            d.snap->changePct = prev > 0 ? (d.snap->lastClose - prev) / prev * 100.0 : 0.0;
        }
        d.snap->changePctValid = !d.snap->dailyBars.isEmpty();
        d.snap->volume = d.snap->dailyBars.last().volume / 1e8;
        fillIndexSeries(*d.snap);
        qDebug() << "[USIndex]" << d.name << d.snap->lastClose << d.snap->changePct << "%"
                 << "bars:" << d.snap->dailyBars.size()
                 << "lastDate:" << d.snap->dailyBars.last().date;
    }
}

void MarketContextFetcher::fetchNorthbound(MarketContext &ctx) const
{
    // 板块主力资金流合计（非真实北向资金，仅作市场情绪参考）
    const QString url =
        "https://vip.stock.finance.sina.com.cn/quotes_service/api/json_v2.php/"
        "MoneyFlow.ssl_bkzj_bk?page=1&num=20&sort=netamount&asc=0&fenlei=1";
    const auto r = HttpClient::get(url, 8000, 1);
    if (r.ok && !r.body.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
        if (doc.isArray()) {
            double totalNetFlow = 0;
            for (const QJsonValue &v : doc.array()) {
                const QJsonObject o = v.toObject();
                const double net = o.value("netamount").toString().toDouble();
                totalNetFlow += net;
            }
            ctx.northboundNetBuy = totalNetFlow / 1e8;
            ctx.northboundFlowValid = true;
            qDebug() << "[MarketContext] 板块资金流合计:" << ctx.northboundNetBuy << "亿（非北向资金）";
        }
    }

    if (ctx.northboundFlowValid) {
        ctx.northboundSeries.push_back(ctx.northboundNetBuy);
        ctx.northbound5dAvg = ctx.northboundNetBuy;
        ctx.northbound20dAvg = ctx.northboundNetBuy;
    }
}

void MarketContextFetcher::fetchMarketBreadth(MarketContext &ctx) const
{
    // 优先：东方财富实时涨跌家数（真实统计）
    const auto emUrl = QString(
        "https://push2.eastmoney.com/api/qt/ulist.np/get?fltt=2"
        "&fields=f1,f2,f3,f104,f105,f106&secids=1.000001,0.399001"
        "&ut=fa5fd1943c7b386f172d6893dbbd1");
    const auto emResult = HttpClient::get(emUrl, 8000, 2);
    if (emResult.ok && !emResult.body.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(emResult.body.toUtf8());
        const QJsonArray diff = doc.object().value("data").toObject().value("diff").toArray();
        if (!diff.isEmpty()) {
            int totalAdv = 0, totalDec = 0, totalFlat = 0;
            for (const QJsonValue &v : diff) {
                const QJsonObject o = v.toObject();
                totalAdv  += o.value("f104").toInt();
                totalDec  += o.value("f105").toInt();
                totalFlat += o.value("f106").toInt();
            }
            // 去重：上证+深证有交叉，取上证数据即可
            const QJsonObject sh = diff[0].toObject();
            ctx.advanceCount = sh.value("f104").toInt();
            ctx.declineCount = sh.value("f105").toInt();
            ctx.totalStocks = ctx.advanceCount + ctx.declineCount + sh.value("f106").toInt();
            ctx.breadthEstimated = false;
            qDebug() << "[MarketBreadth] 东方财富真实数据: adv=" << ctx.advanceCount
                     << "dec=" << ctx.declineCount << "total=" << ctx.totalStocks;
        }
    }

    // 回退：新浪获取A股总数 + 估算
    if (ctx.advanceCount == 0 && ctx.declineCount == 0) {
        const auto countResult = HttpClient::get(
            "https://vip.stock.finance.sina.com.cn/quotes_service/api/json_v2.php/"
            "Market_Center.getHQNodeStockCount?node=hs_a", 8000, 1);
        if (countResult.ok && !countResult.body.isEmpty()) {
            const QString trimmed = countResult.body.trimmed().replace("\"", "");
            ctx.totalStocks = trimmed.toInt();
        }
        if (ctx.totalStocks > 0 && ctx.shanghai.changePctValid) {
            const double shPct = ctx.shanghai.changePct;
            double advRatio = qBound(0.1, 0.5 + shPct / 10.0, 0.9);
            ctx.advanceCount = static_cast<int>(ctx.totalStocks * advRatio);
            ctx.declineCount = ctx.totalStocks - ctx.advanceCount;
        }
        ctx.breadthEstimated = true;
        qDebug() << "[MarketBreadth] 回退估算: adv=" << ctx.advanceCount
                 << "dec=" << ctx.declineCount << "(估算)";
    }

    if (ctx.advanceCount == 0 && ctx.declineCount == 0) {
        ctx.advanceCount = 2500;
        ctx.declineCount = 2500;
        ctx.breadthEstimated = true;
    }
    ctx.advanceDeclineRatio = ctx.declineCount > 0
        ? static_cast<double>(ctx.advanceCount) / ctx.declineCount : 1.0;

    // 涨跌停数：无可靠实时源时，不估算而是保持0
    if (ctx.breadthEstimated) {
        ctx.limitUpCount = 0;
        ctx.limitDownCount = 0;
    }

    qDebug() << "[MarketBreadth] total:" << ctx.totalStocks
             << "adv:" << ctx.advanceCount << "dec:" << ctx.declineCount
             << "limitUp:" << ctx.limitUpCount << "limitDown:" << ctx.limitDownCount
             << (ctx.breadthEstimated ? "(估算)" : "(真实)");
}

void MarketContextFetcher::computeRiskScore(MarketContext &ctx) const
{
    double score = 50.0;

    // 1) 大盘涨跌贡献 ±15（仅在实时数据有效时参与）
    if (ctx.shanghai.changePctValid) {
        if (ctx.shanghai.changePct > 2.0) score += 12;
        else if (ctx.shanghai.changePct > 1.0) score += 8;
        else if (ctx.shanghai.changePct > 0.3) score += 4;
        else if (ctx.shanghai.changePct < -2.0) score -= 12;
        else if (ctx.shanghai.changePct < -1.0) score -= 8;
        else if (ctx.shanghai.changePct < -0.3) score -= 4;
    }

    // 2) 涨跌家数比贡献：真实数据±10，估算数据±3
    const int breadthWeight = ctx.breadthEstimated ? 1 : 3;
    if (ctx.advanceDeclineRatio > 3.0) score += 4 * breadthWeight;
    else if (ctx.advanceDeclineRatio > 2.0) score += 2 * breadthWeight;
    else if (ctx.advanceDeclineRatio > 1.2) score += 1 * breadthWeight;
    else if (ctx.advanceDeclineRatio < 0.33) score -= 4 * breadthWeight;
    else if (ctx.advanceDeclineRatio < 0.5) score -= 2 * breadthWeight;
    else if (ctx.advanceDeclineRatio < 0.8) score -= 1 * breadthWeight;

    // 3) 涨停/跌停贡献：仅真实数据参与
    if (!ctx.breadthEstimated) {
        if (ctx.limitUpCount > 60) score += 5;
        else if (ctx.limitUpCount > 30) score += 2;
        if (ctx.limitDownCount > 60) score -= 5;
        else if (ctx.limitDownCount > 30) score -= 2;
    }

    // 4) 板块资金流贡献 ±5
    if (ctx.northboundFlowValid) {
        if (ctx.northboundNetBuy > 80) score += 4;
        else if (ctx.northboundNetBuy > 30) score += 2;
        else if (ctx.northboundNetBuy > 0) score += 1;
        else if (ctx.northboundNetBuy < -80) score -= 4;
        else if (ctx.northboundNetBuy < -30) score -= 2;
        else if (ctx.northboundNetBuy < 0) score -= 1;
    }

    ctx.marketRiskScore = qBound(0.0, score, 100.0);

    if (ctx.marketRiskScore >= 85) ctx.riskLevel = "极度贪婪";
    else if (ctx.marketRiskScore >= 70) ctx.riskLevel = "贪婪";
    else if (ctx.marketRiskScore >= 58) ctx.riskLevel = "乐观";
    else if (ctx.marketRiskScore >= 42) ctx.riskLevel = "中性";
    else if (ctx.marketRiskScore >= 30) ctx.riskLevel = "谨慎";
    else if (ctx.marketRiskScore >= 15) ctx.riskLevel = "恐慌";
    else ctx.riskLevel = "极度恐慌";
}
