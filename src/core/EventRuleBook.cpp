#include "core/EventRuleBook.h"

namespace {

bool containsAny(const QString &text, const QStringList &keywords)
{
    for (const QString &keyword : keywords) {
        if (text.contains(keyword, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool hasAllContext(const QString &text, const EventRule &rule)
{
    if (!containsAny(text, rule.requiredAny)) {
        return false;
    }
    return rule.contextAny.isEmpty() || containsAny(text, rule.contextAny);
}

} // namespace

EventRuleBook::EventRuleBook()
{
    m_rules = {
        {QStringLiteral("fed_rate_cut"),
         MacroEventType::MonetaryPolicy,
         MacroEventRegion::US,
         {QString::fromUtf8("美联储"), QStringLiteral("Fed"), QStringLiteral("FOMC")},
         {QString::fromUtf8("降息"), QString::fromUtf8("宽松"), QStringLiteral("rate cut")},
         QStringLiteral("FOMC / CPI / PCE / nonfarm payrolls"),
         0.86},
        {QStringLiteral("fed_meeting"),
         MacroEventType::MonetaryPolicy,
         MacroEventRegion::US,
         {QString::fromUtf8("美联储"), QStringLiteral("FOMC")},
         {QString::fromUtf8("会议"), QString::fromUtf8("议息"), QString::fromUtf8("主席讲话")},
         QStringLiteral("FOMC"),
         0.76},
        {QStringLiteral("us_inflation_jobs"),
         MacroEventType::InflationEmployment,
         MacroEventRegion::US,
         {QStringLiteral("CPI"), QStringLiteral("PCE"), QString::fromUtf8("非农"), QString::fromUtf8("就业")},
         {QString::fromUtf8("预期"), QString::fromUtf8("高于"), QString::fromUtf8("低于"), QString::fromUtf8("降息")},
         QStringLiteral("CPI / PCE / nonfarm payrolls"),
         0.78},
        {QStringLiteral("china_monetary"),
         MacroEventType::MonetaryPolicy,
         MacroEventRegion::China,
         {QString::fromUtf8("降准"), QStringLiteral("LPR"), QStringLiteral("MLF")},
         {},
         QStringLiteral("LPR / MLF / PBOC operation"),
         0.8},
        {QStringLiteral("commodity_price"),
         MacroEventType::CommoditySupplyDemand,
         MacroEventRegion::Global,
         {QString::fromUtf8("铜"), QString::fromUtf8("铝"), QString::fromUtf8("锂"), QString::fromUtf8("原油"), QString::fromUtf8("黄金")},
         {QString::fromUtf8("涨价"), QString::fromUtf8("上涨"), QString::fromUtf8("库存"), QString::fromUtf8("供给")},
         QString::fromUtf8("商品价格 / 库存 / 供需"),
         0.74},
        {QStringLiteral("semiconductor_policy"),
         MacroEventType::IndustrialPolicy,
         MacroEventRegion::Global,
         {QString::fromUtf8("半导体"), QString::fromUtf8("芯片"), QString::fromUtf8("出口管制")},
         {QString::fromUtf8("限制"), QString::fromUtf8("政策"), QString::fromUtf8("国产替代")},
         QString::fromUtf8("政策细则 / 企业订单 / 供应链反馈"),
         0.78}
    };
}

QList<EventRule> EventRuleBook::matchingRules(const QString &text) const
{
    QList<EventRule> matches;
    for (const EventRule &rule : m_rules) {
        if (hasAllContext(text, rule)) {
            matches.push_back(rule);
        }
    }
    bool hasFedRateCut = false;
    for (const EventRule &rule : matches) {
        if (rule.key == QStringLiteral("fed_rate_cut")) {
            hasFedRateCut = true;
            break;
        }
    }
    if (hasFedRateCut) {
        QList<EventRule> filtered;
        for (const EventRule &rule : matches) {
            if (rule.key != QStringLiteral("fed_meeting")) {
                filtered.push_back(rule);
            }
        }
        return filtered;
    }
    return matches;
}

MacroEventState EventRuleBook::resolveState(const QString &text) const
{
    if (text.contains(QString::fromUtf8("修正")) || text.contains(QString::fromUtf8("下修"))
        || text.contains(QString::fromUtf8("上修")) || text.contains(QString::fromUtf8("降温"))
        || text.contains(QString::fromUtf8("不及预期")) || text.contains(QString::fromUtf8("高于预期"))
        || text.contains(QString::fromUtf8("低于预期"))) {
        return MacroEventState::Revised;
    }

    if (text.contains(QString::fromUtf8("宣布")) || text.contains(QString::fromUtf8("落地"))
        || text.contains(QString::fromUtf8("正式")) || text.contains(QString::fromUtf8("决定"))) {
        return MacroEventState::Confirmed;
    }

    if (text.contains(QString::fromUtf8("预期")) || text.contains(QString::fromUtf8("预计"))
        || text.contains(QString::fromUtf8("可能")) || text.contains(QString::fromUtf8("押注"))
        || text.contains(QString::fromUtf8("升温")) || text.contains(QString::fromUtf8("有望"))) {
        return MacroEventState::Expected;
    }

    if (text.contains(QString::fromUtf8("将于")) || text.contains(QString::fromUtf8("召开"))
        || text.contains(QString::fromUtf8("会议")) || text.contains(QString::fromUtf8("日程"))) {
        return MacroEventState::Scheduled;
    }

    return MacroEventState::Expected;
}
