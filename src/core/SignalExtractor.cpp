#include "core/SignalExtractor.h"

#include <QString>
#include <QtGlobal>

namespace {
SignalDirection inferDirection(const QString &text)
{
    static const QStringList posKeywords = {
        "回升", "改善", "活跃", "增强", "上涨", "大涨", "涨停", "突破",
        "创新高", "利好", "超预期", "增长", "扩张", "景气", "需求旺盛",
        "放量", "净流入", "加码", "刺激", "补贴", "扶持", "订单",
    };
    static const QStringList negKeywords = {
        "放缓", "承压", "退坡", "压力", "下跌", "大跌", "跌停", "下探",
        "创新低", "利空", "不及预期", "下滑", "萎缩", "衰退", "需求疲软",
        "缩量", "净流出", "减持", "制裁", "罚款", "暴雷", "违约",
    };

    int posHits = 0, negHits = 0;
    for (const QString &kw : posKeywords) {
        if (text.contains(kw)) ++posHits;
    }
    for (const QString &kw : negKeywords) {
        if (text.contains(kw)) ++negHits;
    }

    if (posHits > negHits) return SignalDirection::Positive;
    if (negHits > posHits) return SignalDirection::Negative;
    return SignalDirection::Neutral;
}

double inferStrength(SignalDirection direction, const QString &text, double dataQuality)
{
    const int lengthFactor = text.size() > 60 ? 2 : 1;
    const double quality = qBound(0.2, dataQuality, 1.0);
    switch (direction) {
    case SignalDirection::Positive:
        return 0.35 * lengthFactor * quality;
    case SignalDirection::Negative:
        return -0.35 * lengthFactor * quality;
    case SignalDirection::Neutral:
    default:
        return 0.05 * quality;
    }
}
} // namespace

QList<IndustrySignal> SignalExtractor::extract(const QList<RawInsight> &rawData) const
{
    QList<IndustrySignal> extractedSignals;
    for (const RawInsight &item : rawData) {
        const QString mergedText = item.headline + " " + item.detail;
        const SignalDirection direction = inferDirection(mergedText);
        extractedSignals.push_back({
            item.sourceName,
            item.sourceUrl,
            item.industry,
            item.headline,
            direction,
            inferStrength(direction, mergedText, item.dataQuality),
            item.dataQuality,
            item.detail,
            item.timestamp
        });
    }
    return extractedSignals;
}
