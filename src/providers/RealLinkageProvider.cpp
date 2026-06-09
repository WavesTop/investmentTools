#include "providers/RealLinkageProvider.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QVector>
#include <QtMath>

#include "core/HttpClient.h"

namespace {
struct LinkageTarget
{
    QString industryName;
    QString sinaFundSymbol;  // 新浪ETF代码
    QString sinaIndexSymbol; // 新浪对标指数/ETF代码
};

QList<LinkageTarget> buildTargets()
{
    return {
        { "半导体", "sh512480", "sh000985" },
        { "新能源", "sh516160", "sh000941" },
        { "医药", "sh512170", "sh000933" },
        { "消费", "sz159928", "sh000932" },
        { "军工", "sh512660", "sh399959" },
        { "银行", "sh512800", "sh000951" },
        { "光伏", "sh515790", "sh931151" },
    };
}

QMap<QString, double> fetchSinaCloseSeriesByDate(const QString &sinaSymbol)
{
    QMap<QString, double> series;
    const QString url = QString(
        "https://money.finance.sina.com.cn/quotes_service/api/json_v2.php/"
        "CN_MarketData.getKLineData?symbol=%1&scale=240&ma=no&datalen=250")
        .arg(sinaSymbol);
    const HttpClient::HttpResult result = HttpClient::get(url, 10000, 1);
    if (!result.ok || result.body.isEmpty() || result.body.trimmed() == "null") {
        return series;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(result.body.toUtf8());
    if (!doc.isArray()) return series;
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        const QString date = o.value("day").toString().left(10);
        const double close = o.value("close").toString().toDouble();
        if (!date.isEmpty() && close > 0.0) {
            series.insert(date, close);
        }
    }
    return series;
}

void toPairedReturns(
    const QMap<QString, double> &fundSeries,
    const QMap<QString, double> &indexSeries,
    QVector<double> *fundReturns,
    QVector<double> *indexReturns)
{
    if (!fundReturns || !indexReturns) {
        return;
    }
    fundReturns->clear();
    indexReturns->clear();
    if (fundSeries.size() < 2 || indexSeries.size() < 2) {
        return;
    }
    const QStringList dates = fundSeries.keys();
    for (int i = 1; i < dates.size(); ++i) {
        const QString prevDate = dates[i - 1];
        const QString currDate = dates[i];
        if (!indexSeries.contains(prevDate) || !indexSeries.contains(currDate)) {
            continue;
        }
        const double prevFund = fundSeries.value(prevDate);
        const double currFund = fundSeries.value(currDate);
        const double prevIndex = indexSeries.value(prevDate);
        const double currIndex = indexSeries.value(currDate);
        if (prevFund <= 0.0 || currFund <= 0.0 || prevIndex <= 0.0 || currIndex <= 0.0) {
            continue;
        }
        const double fundRet = (currFund - prevFund) / prevFund;
        const double indexRet = (currIndex - prevIndex) / prevIndex;
        if (qAbs(fundRet) > 0.2 || qAbs(indexRet) > 0.2) {
            continue;
        }
        fundReturns->push_back(fundRet);
        indexReturns->push_back(indexRet);
    }
}

double pearsonCorrelation(const QVector<double> &x, const QVector<double> &y)
{
    if (x.size() != y.size() || x.size() < 20) {
        return 0.0;
    }

    double sumX = 0.0;
    double sumY = 0.0;
    for (int i = 0; i < x.size(); ++i) {
        sumX += x[i];
        sumY += y[i];
    }
    const double meanX = sumX / static_cast<double>(x.size());
    const double meanY = sumY / static_cast<double>(y.size());

    double cov = 0.0;
    double varX = 0.0;
    double varY = 0.0;
    for (int i = 0; i < x.size(); ++i) {
        const double dx = x[i] - meanX;
        const double dy = y[i] - meanY;
        cov += dx * dy;
        varX += dx * dx;
        varY += dy * dy;
    }

    if (varX <= 1e-12 || varY <= 1e-12) {
        return 0.0;
    }
    return cov / qSqrt(varX * varY);
}

double estimateBeta(const QVector<double> &fundReturns, const QVector<double> &indexReturns)
{
    if (fundReturns.size() != indexReturns.size() || fundReturns.size() < 20) {
        return 0.0;
    }
    double meanFund = 0.0;
    double meanIndex = 0.0;
    for (int i = 0; i < fundReturns.size(); ++i) {
        meanFund += fundReturns[i];
        meanIndex += indexReturns[i];
    }
    meanFund /= static_cast<double>(fundReturns.size());
    meanIndex /= static_cast<double>(indexReturns.size());

    double cov = 0.0;
    double varIndex = 0.0;
    for (int i = 0; i < fundReturns.size(); ++i) {
        const double f = fundReturns[i] - meanFund;
        const double idx = indexReturns[i] - meanIndex;
        cov += f * idx;
        varIndex += idx * idx;
    }
    if (varIndex <= 1e-12) {
        return 0.0;
    }
    return cov / varIndex;
}

double linkageQuality(double corr, int sampleSize)
{
    const double corrQuality = qMin(1.0, qAbs(corr));
    const double sampleQuality = sampleSize >= 60 ? 0.95 : sampleSize >= 40 ? 0.8 : 0.65;
    return qMin(1.0, 0.4 * corrQuality + 0.6 * sampleQuality);
}
} // namespace

QList<RawInsight> RealLinkageProvider::fetchInsights()
{
    QList<RawInsight> insights;
    const QList<LinkageTarget> targets = buildTargets();

    for (const LinkageTarget &target : targets) {
        const QMap<QString, double> fundSeries = fetchSinaCloseSeriesByDate(target.sinaFundSymbol);
        const QMap<QString, double> indexSeries = fetchSinaCloseSeriesByDate(target.sinaIndexSymbol);
        if (fundSeries.size() < 30 || indexSeries.size() < 30) {
            continue;
        }

        QMap<QString, double> alignedFund;
        QMap<QString, double> alignedIndex;
        for (auto it = fundSeries.begin(); it != fundSeries.end(); ++it) {
            if (indexSeries.contains(it.key())) {
                alignedFund.insert(it.key(), it.value());
                alignedIndex.insert(it.key(), indexSeries.value(it.key()));
            }
        }
        if (alignedFund.size() < 30) {
            continue;
        }

        QVector<double> fundReturns;
        QVector<double> indexReturns;
        toPairedReturns(alignedFund, alignedIndex, &fundReturns, &indexReturns);
        const int sampleSize = fundReturns.size();
        if (sampleSize < 20) {
            continue;
        }

        const double corr = pearsonCorrelation(fundReturns, indexReturns);
        const double beta = estimateBeta(fundReturns, indexReturns);

        QString linkageText = "弱相关";
        if (corr >= 0.6) {
            linkageText = "显著正相关";
        } else if (corr >= 0.3) {
            linkageText = "中等正相关";
        } else if (corr <= -0.3) {
            linkageText = "负相关";
        }

        insights.push_back({
            providerName(),
            "https://finance.sina.com.cn",
            target.industryName,
            target.industryName + "基金与行业指数联动：" + linkageText,
            "ETF代码：" + target.sinaFundSymbol
                + "；相关系数：" + QString::number(corr, 'f', 3)
                + "；Beta：" + QString::number(beta, 'f', 3)
                + "；有效样本日：" + QString::number(sampleSize),
            QDateTime::currentDateTime(),
            linkageQuality(corr, sampleSize)
        });
    }

    return insights;
}

QString RealLinkageProvider::providerName() const
{
    return "EastmoneyLinkage";
}
