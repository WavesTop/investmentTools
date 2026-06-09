#include "providers/MockMacroProvider.h"

#include <QDateTime>

QList<RawInsight> MockMacroProvider::fetchInsights()
{
    const QDateTime now = QDateTime::currentDateTime();

    return {
        {
            "MockMacroProvider",
            "mock://local/semiconductor",
            "半导体",
            "全球芯片库存去化加速",
            "上游设备订单出现连续回升，部分晶圆代工厂产能利用率改善，行业景气度边际回暖。",
            now,
            0.35
        },
        {
            "MockMacroProvider",
            "mock://local/new-energy",
            "新能源",
            "储能项目并网节奏放缓",
            "受海外利率高位和部分地区补贴退坡影响，项目回款周期拉长，短期需求承压。",
            now,
            0.35
        },
        {
            "MockMacroProvider",
            "mock://local/healthcare",
            "医药",
            "创新药对外授权交易活跃",
            "多家创新药企业达成里程碑付款协议，研发现金流改善预期增强。",
            now,
            0.35
        },
        {
            "MockMacroProvider",
            "mock://local/consumer",
            "消费",
            "可选消费促销力度提升",
            "渠道库存压力仍在，但高频数据表现分化，线下客流恢复较慢。",
            now,
            0.35
        }
    };
}

QString MockMacroProvider::providerName() const
{
    return "MockMacroProvider";
}
