#pragma once

#include <QString>
#include <QVector>
#include "core/SectorFetcher.h"

struct IndexSnapshot
{
    QString name;
    QString code;
    double lastClose = 0;
    double changePct = 0;
    double volume = 0;       // 亿元
    double amount = 0;       // 亿元
    QVector<KBar> dailyBars;
    QVector<double> klineSeries;
    QVector<double> weekSeries;
    QVector<double> monthSeries;
};

struct MarketContext
{
    // A股大盘指数
    IndexSnapshot shanghai;   // 上证指数
    IndexSnapshot shenzhen;   // 深证成指
    IndexSnapshot chinext;    // 创业板指
    IndexSnapshot csi300;     // 沪深300
    IndexSnapshot csi500;     // 中证500

    // 美股主要指数
    IndexSnapshot nasdaq;     // 纳斯达克综合
    IndexSnapshot sp500;      // 标普500
    IndexSnapshot dowjones;   // 道琼斯工业

    // 北向资金（沪股通+深股通）
    double northboundNetBuy = 0;     // 今日净买入（亿元）
    double northbound5dAvg = 0;      // 近5日均值
    double northbound20dAvg = 0;     // 近20日均值
    bool northboundFlowValid = false;

    // 市场广度
    int totalStocks = 0;
    int advanceCount = 0;    // 上涨家数
    int declineCount = 0;    // 下跌家数
    int limitUpCount = 0;    // 涨停家数
    int limitDownCount = 0;  // 跌停家数
    double advanceDeclineRatio = 1.0;

    // 市场风险评分 [0, 100]：0=极度恐慌, 50=中性, 100=极度贪婪
    double marketRiskScore = 50.0;
    QString riskLevel;       // 极度恐慌/恐慌/谨慎/中性/乐观/贪婪/极度贪婪
    QVector<double> northboundSeries; // 近20日北向净流入序列

    bool valid = false;
};

class MarketContextFetcher
{
public:
    MarketContext fetch() const;

private:
    void fetchIndices(MarketContext &ctx) const;
    void fetchUSIndices(MarketContext &ctx) const;
    void fetchNorthbound(MarketContext &ctx) const;
    void fetchMarketBreadth(MarketContext &ctx) const;
    void computeRiskScore(MarketContext &ctx) const;
};
