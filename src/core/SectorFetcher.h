#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

struct KBar
{
    QString date;
    double open = 0, high = 0, low = 0, close = 0;
    double volume = 0;
    double amount = 0;
    double turnover = 0;
    double changePct = 0;
};

struct SectorInfo
{
    QString code;
    QString name;
    QString eastmoneyCode;
    double changePct = 0.0;
    double turnoverRate = 0.0;
    int upCount = 0;
    int downCount = 0;
    double hotScore = 0.0;
    QVector<KBar> dailyBars;
    QVector<double> klineSeries;
    QVector<double> weekSeries;
    QVector<double> monthSeries;
    QVector<double> fundFlowSeries;
    QString sourceTag;
    QString klineSource;
    QString fundFlowSource;
    QString valuationSource;
    QString lastDataDate;
    QStringList missingDataItems;
    int sectorTier = 1;                 // 3核心池 2主题池 1观察池
    QString sectorTierLabel;
    bool crossSourceValidated = false;  // 是否有跨源校验
    double sourceConsistencyScore = 70.0; // 跨源一致性分 [0,100]
    double altChangePct = 0.0;          // 备用源涨跌幅
    bool hasAltChangePct = false;

    // 估值与拥挤度（实时获取）
    double peRatio = 0.0;           // 市盈率
    double pbRatio = 0.0;           // 市净率
    double pePercentile = 50.0;     // PE历史分位 [0,100]：越低越被低估
    double pbPercentile = 50.0;     // PB历史分位
    double totalMarketCap = 0.0;    // 总市值（亿元）
    double avgTurnover5d = 0.0;     // 近5日平均换手率
    double crowdingIndex = 50.0;    // 拥挤度 [0,100]：越高交易越拥挤
    int stockCount = 0;             // 成分股总数
};

class SectorFetcher
{
public:
    QList<SectorInfo> fetchSectorList() const;
    void fetchMarketData(QList<SectorInfo> &sectors) const;
    void fetchValuationData(QList<SectorInfo> &sectors) const;
    QList<SectorInfo> fetchAll() const;
};
