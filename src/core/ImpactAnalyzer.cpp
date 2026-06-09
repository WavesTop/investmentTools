#include "core/ImpactAnalyzer.h"

#include <QHash>
#include <QStringList>
#include <QtGlobal>

namespace {
QString mapFundCategory(const QString &industry)
{
    if (industry == "半导体") {
        return "科技成长基金";
    }
    if (industry == "新能源") {
        return "新能源主题基金";
    }
    if (industry == "医药") {
        return "医药创新基金";
    }
    if (industry == "消费") {
        return "消费精选基金";
    }
    return "宽基指数基金";
}

double toConfidence(double absoluteSignal)
{
    const double base = absoluteSignal < 0.2 ? 0.35 : absoluteSignal < 0.5 ? 0.6 : 0.8;
    return base > 1.0 ? 1.0 : base;
}
} // namespace

QList<FundImpact> ImpactAnalyzer::analyze(const QList<IndustrySignal> &industrySignals) const
{
    struct Aggregate
    {
        double total = 0.0;
        int count = 0;
        double qualitySum = 0.0;
        QStringList rationales;
    };

    QHash<QString, Aggregate> grouped;
    for (const IndustrySignal &signal : industrySignals) {
        const QString category = mapFundCategory(signal.industry);
        Aggregate &bucket = grouped[category];
        bucket.total += signal.strength;
        bucket.count += 1;
        bucket.qualitySum += signal.dataQuality;
        bucket.rationales.push_back(
            signal.industry + "：" + signal.event
            + "；来源：" + signal.sourceName
            + "；链接：" + signal.sourceUrl
            + "；质量分：" + QString::number(signal.dataQuality, 'f', 2)
            + "；" + signal.reason
        );
    }

    QList<FundImpact> impacts;
    for (auto it = grouped.begin(); it != grouped.end(); ++it) {
        const QString category = it.key();
        const Aggregate &agg = it.value();
        const double avgMove = agg.count == 0 ? 0.0 : agg.total / static_cast<double>(agg.count);
        const double baseConfidence = toConfidence(qAbs(avgMove));
        const double avgQuality = agg.count == 0 ? 0.0 : agg.qualitySum / static_cast<double>(agg.count);
        const double confidence = qBound(0.2, baseConfidence * (0.6 + 0.4 * avgQuality), 0.98);
        impacts.push_back({
            category,
            avgMove,
            confidence,
            agg.rationales.join("\n")
        });
    }
    return impacts;
}
