#include "core/FlowStructureAnalyzer.h"
#include <cmath>
#include <algorithm>

FlowStructure FlowStructureAnalyzer::analyze(const QVector<double> &flowSeries,
                                              double todayFlow, double todayChangePct)
{
    FlowStructure fs;
    const int n = flowSeries.size();

    if (n < 3) {
        fs.flowPattern = QString::fromUtf8("数据不足");
        return fs;
    }

    // Continuous inflow days (counting from most recent backwards)
    fs.continuousInflowDays = 0;
    for (int i = n - 1; i >= 0; --i) {
        if (flowSeries[i] > 0) fs.continuousInflowDays++;
        else break;
    }
    if (todayFlow > 0) fs.continuousInflowDays = qMax(fs.continuousInflowDays, 1.0);

    // Flow momentum: recent 5-day avg vs previous 5-day avg
    double recent5 = 0, prev5 = 0;
    int r5 = qMin(5, n);
    for (int i = n - r5; i < n; ++i) recent5 += flowSeries[i];
    recent5 /= r5;
    if (n > 5) {
        int p5 = qMin(5, n - r5);
        for (int i = n - r5 - p5; i < n - r5; ++i) prev5 += flowSeries[i];
        prev5 /= qMax(p5, 1);
    }
    double maxAbs = qMax(qAbs(recent5), qAbs(prev5));
    fs.flowMomentum = (maxAbs > 0.01) ? (recent5 / maxAbs * 100.0) : 0;
    fs.flowAcceleration = recent5 - prev5;

    // Speculative flow risk: single-day spike + high change = speculative
    double maxSingleDay = 0;
    double avgFlow = 0;
    for (int i = 0; i < n; ++i) {
        avgFlow += qAbs(flowSeries[i]);
        if (qAbs(flowSeries[i]) > qAbs(maxSingleDay)) maxSingleDay = flowSeries[i];
    }
    avgFlow /= n;
    double spikeRatio = (avgFlow > 0.01) ? qAbs(maxSingleDay) / avgFlow : 1.0;
    fs.speculativeFlowRisk = qBound(0.0, (spikeRatio - 2.0) * 20.0, 60.0);
    if (qAbs(todayChangePct) > 5.0 && qAbs(todayFlow) > avgFlow * 2.0)
        fs.speculativeFlowRisk += 25.0;
    fs.speculativeFlowRisk = qBound(0.0, fs.speculativeFlowRisk, 100.0);

    // Institutional score: steady moderate inflow over multiple days
    int moderateInflowDays = 0;
    for (int i = qMax(n - 10, 0); i < n; ++i) {
        if (flowSeries[i] > 0 && flowSeries[i] < avgFlow * 2.5)
            ++moderateInflowDays;
    }
    int window10 = qMin(10, n);
    fs.institutionalScore = double(moderateInflowDays) / window10 * 80.0;
    if (fs.continuousInflowDays >= 5) fs.institutionalScore += 15;
    fs.institutionalScore = qBound(0.0, fs.institutionalScore, 100.0);

    // Pattern classification
    if (fs.continuousInflowDays >= 5 && fs.speculativeFlowRisk < 30) {
        fs.flowPattern = QString::fromUtf8("机构吸筹");
    } else if (spikeRatio > 3.0 && todayChangePct > 3.0) {
        fs.flowPattern = QString::fromUtf8("情绪高潮");
    } else if (fs.continuousInflowDays == 0 && recent5 < 0) {
        fs.flowPattern = QString::fromUtf8("持续流出");
    } else if (fs.flowMomentum > 30) {
        fs.flowPattern = QString::fromUtf8("资金流入加速");
    } else if (fs.flowMomentum < -30) {
        fs.flowPattern = QString::fromUtf8("资金流出加速");
    } else {
        fs.flowPattern = QString::fromUtf8("中性");
    }

    return fs;
}
