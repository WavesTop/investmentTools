#include "core/PriceLevelAnalyzer.h"

#include <QCoreApplication>
#include <QDate>
#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (condition) return;
    QTextStream(stderr) << "FAIL: " << message << '\n';
    ++failures;
}

QVector<KBar> makeBars(double start, const QVector<double> &moves)
{
    QVector<KBar> bars;
    const QDate first(2026, 1, 2);
    double close = start;
    for (int i = 0; i < moves.size(); ++i) {
        const double prev = close;
        close += moves[i];
        KBar bar;
        bar.date = first.addDays(i).toString("yyyy-MM-dd");
        bar.open = prev;
        bar.close = close;
        bar.high = qMax(prev, close) + 1.2;
        bar.low = qMin(prev, close) - 1.0;
        bar.volume = 100000000 + i * 250000;
        bars << bar;
    }
    return bars;
}

SectorSnapshot makeSector(const QString &name)
{
    SectorSnapshot sector;
    sector.industry = name;
    sector.todayChangePctValid = true;
    sector.dataQualityScore = 86.0;
    sector.sourceConsistencyScore = 91.0;
    return sector;
}

void verifyPullbackEntryPlan()
{
    SectorSnapshot sector = makeSector(QString::fromUtf8("半导体"));
    sector.forecastScore = 0.34;
    sector.todayChangePct = -0.6;
    sector.fiveDayMomentum = 2.1;
    sector.twentyDayMomentum = 8.4;
    sector.tech.ma20 = 108.0;
    sector.tech.ma60 = 101.0;
    sector.tech.bollMid = 108.0;
    sector.tech.bollUpper = 119.0;
    sector.tech.bollLower = 97.0;
    sector.tech.maLongArrange = true;
    sector.tech.rsi6 = 58.0;
    sector.dailyBars = makeBars(100.0, {
        0.4, 0.6, 0.2, 0.8, -0.2, 0.5, 0.7, -0.1, 0.6, 0.8,
        -0.3, 0.4, 0.9, 0.5, -0.2, 0.7, 0.6, 0.3, 0.4, 0.6,
        -0.4, 0.5, 0.4, 0.7, 0.5, -0.2, 0.4, 0.6, 0.3, -0.5
    });

    const PriceLevelPlan plan = PriceLevelAnalyzer::analyze(sector);

    expect(plan.valid, "pullback plan is valid when k-line data exists");
    expect(plan.trendStateLabel.contains(QString::fromUtf8("上升")), "pullback plan detects uptrend");
    expect(plan.actionLabel.contains(QString::fromUtf8("回调")) || plan.actionLabel.contains(QString::fromUtf8("观察")),
           "pullback plan avoids direct chase language");
    expect(plan.entryZoneLow > 0.0 && plan.entryZoneHigh >= plan.entryZoneLow,
           "pullback plan has entry observation zone");
    expect(plan.stopLossLevel > 0.0 && plan.stopLossLevel < plan.entryZoneLow,
           "pullback plan puts invalidation below entry zone");
    expect(plan.takeProfitHigh > plan.entryZoneHigh,
           "pullback plan has upside exit zone");
    expect(plan.riskRewardRatio >= 1.5,
           "pullback plan keeps acceptable risk reward");
    expect(plan.invalidationReason.contains(QString::fromUtf8("跌破")),
           "pullback plan explains invalidation");
}

void verifyOverheatedNoChase()
{
    SectorSnapshot sector = makeSector(QString::fromUtf8("锂电池"));
    sector.forecastScore = 0.46;
    sector.todayChangePct = 5.4;
    sector.fiveDayMomentum = 12.5;
    sector.tech.ma20 = 103.0;
    sector.tech.ma60 = 96.0;
    sector.tech.bollMid = 104.0;
    sector.tech.bollUpper = 112.0;
    sector.tech.bollLower = 96.0;
    sector.tech.rsiOverbought = true;
    sector.tech.priceAboveUpper = true;
    sector.dailyBars = makeBars(96.0, {
        0.2, 0.4, 0.5, 0.2, 0.6, 0.8, 0.4, 0.6, 0.5, 0.7,
        0.8, 0.6, 0.7, 0.5, 0.9, 0.7, 0.8, 0.9, 1.0, 0.8,
        1.1, 0.9, 1.2, 1.0, 1.1, 1.3, 1.2, 1.4, 1.5, 1.8
    });

    const PriceLevelPlan plan = PriceLevelAnalyzer::analyze(sector);

    expect(plan.valid, "overheated plan is valid");
    expect(plan.actionLabel.contains(QString::fromUtf8("过热不追")),
           "overheated plan marks no-chase action");
    expect(plan.entryZoneHigh < plan.currentPrice,
           "overheated plan waits for pullback below current price");
    expect(plan.summary.contains(QString::fromUtf8("追高")),
           "overheated plan explains chase risk");
}

void verifyDowntrendRiskWarning()
{
    SectorSnapshot sector = makeSector(QString::fromUtf8("有色金属"));
    sector.forecastScore = -0.24;
    sector.todayChangePct = -2.8;
    sector.fiveDayMomentum = -6.3;
    sector.twentyDayMomentum = -11.0;
    sector.tech.ma20 = 91.0;
    sector.tech.ma60 = 98.0;
    sector.tech.bollMid = 92.0;
    sector.tech.bollUpper = 101.0;
    sector.tech.bollLower = 83.0;
    sector.tech.maShortArrange = true;
    sector.dailyBars = makeBars(112.0, {
        -0.4, -0.7, -0.3, -0.8, -0.5, -0.9, -0.6, -0.4, -0.8, -1.0,
        -0.5, -0.9, -0.7, -0.6, -1.0, -0.8, -0.6, -1.1, -0.9, -0.7,
        -1.0, -0.8, -1.1, -0.9, -0.7, -1.2, -1.0, -0.8, -1.1, -1.3
    });

    const PriceLevelPlan plan = PriceLevelAnalyzer::analyze(sector);

    expect(plan.valid, "downtrend plan is valid");
    expect(plan.trendStateLabel.contains(QString::fromUtf8("下降")),
           "downtrend plan detects falling trend");
    expect(plan.actionLabel.contains(QString::fromUtf8("风险")) || plan.actionLabel.contains(QString::fromUtf8("失效")),
           "downtrend plan marks warning or invalidation");
    expect(plan.riskRewardRatio < 1.5,
           "downtrend plan refuses positive risk reward");
    expect(plan.entryReason.contains(QString::fromUtf8("企稳")),
           "downtrend plan asks for stabilization before observation");
}

void verifyInsufficientData()
{
    SectorSnapshot sector = makeSector(QString::fromUtf8("新主题"));
    sector.forecastScore = 0.18;
    sector.dailyBars = makeBars(100.0, {0.2, -0.1, 0.3});

    const PriceLevelPlan plan = PriceLevelAnalyzer::analyze(sector);

    expect(!plan.valid, "insufficient data plan is not valid");
    expect(plan.actionLabel.contains(QString::fromUtf8("观察")),
           "insufficient data stays in observation");
    expect(plan.summary.contains(QString::fromUtf8("K 线不足")),
           "insufficient data explains missing k-line history");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    verifyPullbackEntryPlan();
    verifyOverheatedNoChase();
    verifyDowntrendRiskWarning();
    verifyInsufficientData();

    if (failures > 0) {
        QTextStream(stderr) << failures << " price level analyzer smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "PriceLevelAnalyzer smoke passed.\n";
    return 0;
}
