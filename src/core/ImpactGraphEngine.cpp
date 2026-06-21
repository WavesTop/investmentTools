#include "core/ImpactGraphEngine.h"

namespace {

bool allowed(const QString &sector, const QStringList &sectorPool)
{
    return sectorPool.isEmpty() || sectorPool.contains(sector);
}

void addImpact(QList<SectorEventImpact> &impacts,
               const MacroEvent &event,
               const QStringList &sectorPool,
               const QString &sector,
               EventImpactDirection direction,
               EventImpactRelation relation,
               double strength,
               double confidence,
               const QString &path,
               const QString &explanation,
               const QString &condition = QString())
{
    if (!allowed(sector, sectorPool)) return;

    SectorEventImpact impact;
    impact.eventId = event.id;
    impact.eventTitle = event.title;
    impact.sector = sector;
    impact.direction = direction;
    impact.relation = relation;
    impact.state = event.state;
    impact.strength = strength;
    impact.confidence = qMin(1.0, confidence * event.confidence);
    impact.path = path;
    impact.explanation = explanation;
    impact.condition = condition;
    impacts.push_back(impact);
}

bool containsAny(const QString &text, const QStringList &keywords)
{
    for (const QString &keyword : keywords) {
        if (text.contains(keyword, Qt::CaseInsensitive)) return true;
    }
    return false;
}

void addFedRateCut(QList<SectorEventImpact> &impacts, const MacroEvent &event, const QStringList &pool)
{
    addImpact(impacts, event, pool, QString::fromUtf8("黄金"), EventImpactDirection::Positive,
              EventImpactRelation::Direct, 0.86, 0.86,
              QString::fromUtf8("降息预期 -> 实际利率下行 -> 贵金属吸引力提升"),
              QString::fromUtf8("美元和实际利率压力缓解时，黄金通常最先受益。"),
              QString::fromUtf8("若降息来自衰退冲击，风险资产可能先承压。"));
    addImpact(impacts, event, pool, QString::fromUtf8("有色金属"), EventImpactDirection::Positive,
              EventImpactRelation::Direct, 0.78, 0.8,
              QString::fromUtf8("降息预期 -> 美元走弱 -> 商品定价改善"),
              QString::fromUtf8("美元走弱和流动性改善会抬升铜、铝等商品风险偏好。"),
              QString::fromUtf8("需要观察库存、需求和中国地产链是否配合。"));
    addImpact(impacts, event, pool, QString::fromUtf8("半导体"), EventImpactDirection::Positive,
              EventImpactRelation::Indirect, 0.68, 0.76,
              QString::fromUtf8("美债收益率下行 -> 成长股估值压力缓解 -> 科技弹性修复"),
              QString::fromUtf8("半导体受益于成长风格估值修复，但需要产业订单验证。"));
    addImpact(impacts, event, pool, QString::fromUtf8("创新药"), EventImpactDirection::Positive,
              EventImpactRelation::Indirect, 0.62, 0.72,
              QString::fromUtf8("折现率下行 -> 长久期资产估值修复 -> 创新药风险偏好改善"),
              QString::fromUtf8("创新药对折现率和风险偏好敏感，事件更偏估值催化。"));
    addImpact(impacts, event, pool, QString::fromUtf8("证券"), EventImpactDirection::Positive,
              EventImpactRelation::Indirect, 0.58, 0.7,
              QString::fromUtf8("全球流动性改善 -> 风险偏好回升 -> 成交活跃度预期改善"),
              QString::fromUtf8("券商更多受风险偏好和成交量预期影响，需观察 A 股量能。"));
}

} // namespace

QList<SectorEventImpact> ImpactGraphEngine::analyze(const MacroEvent &event,
                                                    const QStringList &sectorPool) const
{
    QList<SectorEventImpact> impacts;
    const QString text = event.title + QStringLiteral(" ") + event.normalizedKey;

    if (event.normalizedKey == QStringLiteral("fed_rate_cut")) {
        addFedRateCut(impacts, event, sectorPool);
    } else if (event.normalizedKey == QStringLiteral("china_monetary")) {
        addImpact(impacts, event, sectorPool, QString::fromUtf8("证券"), EventImpactDirection::Positive,
                  EventImpactRelation::Direct, 0.7, 0.78,
                  QString::fromUtf8("国内流动性宽松 -> 风险偏好改善 -> 券商成交预期"),
                  QString::fromUtf8("降准或 LPR 下调会先改善风险偏好和成交预期。"));
        addImpact(impacts, event, sectorPool, QString::fromUtf8("房地产"), EventImpactDirection::Positive,
                  EventImpactRelation::Direct, 0.64, 0.68,
                  QString::fromUtf8("融资成本下降 -> 地产链预期修复"),
                  QString::fromUtf8("地产链受益程度取决于销售和信用扩张是否跟上。"));
    } else if (event.normalizedKey == QStringLiteral("us_inflation_jobs")) {
        const bool hot = containsAny(text, {QString::fromUtf8("高于预期"), QString::fromUtf8("强于预期")});
        const EventImpactDirection direction = hot ? EventImpactDirection::Negative : EventImpactDirection::Positive;
        addImpact(impacts, event, sectorPool, QString::fromUtf8("半导体"), direction,
                  EventImpactRelation::Indirect, 0.58, 0.68,
                  QString::fromUtf8("美国通胀/就业数据 -> 降息预期变化 -> 成长股估值"),
                  QString::fromUtf8("通胀就业数据会通过美债利率和风险偏好影响成长板块。"));
    } else if (event.normalizedKey == QStringLiteral("commodity_price")) {
        addImpact(impacts, event, sectorPool, QString::fromUtf8("有色金属"), EventImpactDirection::Positive,
                  EventImpactRelation::Direct, 0.82, 0.78,
                  QString::fromUtf8("商品价格上涨 -> 资源品盈利弹性改善"),
                  QString::fromUtf8("有色板块直接受铜、铝、锂等价格和库存变化影响。"));
        addImpact(impacts, event, sectorPool, QString::fromUtf8("石油石化"), EventImpactDirection::Positive,
                  EventImpactRelation::Direct, 0.68, 0.7,
                  QString::fromUtf8("原油价格上涨 -> 上游盈利改善"),
                  QString::fromUtf8("原油价格上行利好上游，但会压制部分下游制造。"));
    } else if (event.normalizedKey == QStringLiteral("semiconductor_policy")) {
        addImpact(impacts, event, sectorPool, QString::fromUtf8("半导体"), EventImpactDirection::Mixed,
                  EventImpactRelation::Conditional, 0.72, 0.72,
                  QString::fromUtf8("出口限制/产业政策 -> 供应链扰动 + 国产替代加速"),
                  QString::fromUtf8("短期可能扰动供应链，中期可能强化国产替代逻辑。"));
    }

    return impacts;
}
