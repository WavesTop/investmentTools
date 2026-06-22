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

void addCheckpoint(QList<MacroEventCheckpoint> &checkpoints,
                   const QString &name,
                   const QString &reason)
{
    for (const MacroEventCheckpoint &checkpoint : checkpoints) {
        if (checkpoint.name == name) return;
    }

    MacroEventCheckpoint checkpoint;
    checkpoint.name = name;
    checkpoint.reason = reason;
    checkpoints.push_back(checkpoint);
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
        {QStringLiteral("fed_hawkish_hike"),
         MacroEventType::MonetaryPolicy,
         MacroEventRegion::US,
         {QStringLiteral("Fed"), QStringLiteral("FOMC"), QString::fromUtf8("美联储")},
         {QString::fromUtf8("加息"), QString::fromUtf8("鹰派"), QString::fromUtf8("高利率"), QStringLiteral("hawkish")},
         QStringLiteral("FOMC / CPI / PCE / nonfarm payrolls"),
         0.82},
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
        {QStringLiteral("china_fiscal_stimulus"),
         MacroEventType::FiscalPolicy,
         MacroEventRegion::China,
         {QString::fromUtf8("专项债"), QString::fromUtf8("财政"), QString::fromUtf8("特别国债"),
          QString::fromUtf8("以旧换新")},
         {QString::fromUtf8("加速"), QString::fromUtf8("稳增长"), QString::fromUtf8("刺激"),
          QString::fromUtf8("发行"), QString::fromUtf8("补贴")},
         QString::fromUtf8("专项债发行 / 政策细则落地"),
         0.78},
        {QStringLiteral("semiconductor_export_control"),
         MacroEventType::GeopoliticsTrade,
         MacroEventRegion::Global,
         {QString::fromUtf8("半导体"), QString::fromUtf8("芯片"), QString::fromUtf8("出口限制"),
          QString::fromUtf8("出口管制")},
         {QString::fromUtf8("限制"), QString::fromUtf8("管制"), QString::fromUtf8("制裁"),
          QString::fromUtf8("供应链")},
         QString::fromUtf8("出口限制细则 / 供应链反馈"),
         0.8},
        {QStringLiteral("oil_supply_shock"),
         MacroEventType::CommoditySupplyDemand,
         MacroEventRegion::Global,
         {QString::fromUtf8("原油"), QStringLiteral("OPEC"), QString::fromUtf8("油价")},
         {QString::fromUtf8("减产"), QString::fromUtf8("供给"), QString::fromUtf8("供应"),
          QString::fromUtf8("地缘"), QString::fromUtf8("上涨")},
         QString::fromUtf8("OPEC / 库存数据 / 地缘冲突"),
         0.76},
        {QStringLiteral("china_market_institution"),
         MacroEventType::MarketInstitution,
         MacroEventRegion::China,
         {QString::fromUtf8("IPO"), QString::fromUtf8("印花税"), QString::fromUtf8("融资融券"),
          QString::fromUtf8("减持"), QString::fromUtf8("证监会")},
         {QString::fromUtf8("优化"), QString::fromUtf8("下调"), QString::fromUtf8("规则"),
          QString::fromUtf8("改革"), QString::fromUtf8("活跃资本市场")},
         QString::fromUtf8("证监会细则 / 交易所执行规则"),
         0.78},
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
    bool hasSemiconductorExportControl = false;
    for (const EventRule &rule : matches) {
        if (rule.key == QStringLiteral("fed_rate_cut")) {
            hasFedRateCut = true;
        }
        if (rule.key == QStringLiteral("semiconductor_export_control")) {
            hasSemiconductorExportControl = true;
        }
    }
    if (hasFedRateCut || hasSemiconductorExportControl) {
        QList<EventRule> filtered;
        for (const EventRule &rule : matches) {
            if (hasFedRateCut && rule.key == QStringLiteral("fed_meeting")) {
                continue;
            }
            if (hasSemiconductorExportControl && rule.key == QStringLiteral("semiconductor_policy")) {
                continue;
            }
            filtered.push_back(rule);
        }
        return filtered;
    }
    return matches;
}

MacroEventState EventRuleBook::resolveState(const QString &text) const
{
    if (text.contains(QString::fromUtf8("否认")) || text.contains(QString::fromUtf8("辟谣"))
        || text.contains(QString::fromUtf8("落空")) || text.contains(QString::fromUtf8("证伪"))
        || text.contains(QString::fromUtf8("不考虑")) || text.contains(QString::fromUtf8("无计划"))) {
        return MacroEventState::Invalidated;
    }

    if (text.contains(QString::fromUtf8("落地后")) || text.contains(QString::fromUtf8("会后"))
        || text.contains(QString::fromUtf8("公布后")) || text.contains(QString::fromUtf8("实施后"))
        || text.contains(QString::fromUtf8("兑现后")) || text.contains(QString::fromUtf8("交易结果"))) {
        return MacroEventState::Occurred;
    }

    if (text.contains(QString::fromUtf8("传闻")) || text.contains(QString::fromUtf8("传出"))
        || text.contains(QString::fromUtf8("据传")) || text.contains(QString::fromUtf8("未证实"))
        || text.contains(QString::fromUtf8("消息称"))) {
        return MacroEventState::Rumor;
    }

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

QList<MacroEventCheckpoint> EventRuleBook::resolveCheckpoints(const QString &text,
                                                              MacroEventType type,
                                                              MacroEventRegion region) const
{
    QList<MacroEventCheckpoint> checkpoints;

    const bool isFedRelated = region == MacroEventRegion::US
        && (type == MacroEventType::MonetaryPolicy || type == MacroEventType::InflationEmployment
            || containsAny(text, {QStringLiteral("Fed"), QStringLiteral("FOMC"), QString::fromUtf8("美联储")}));
    if (isFedRelated) {
        addCheckpoint(checkpoints, QStringLiteral("FOMC"), QStringLiteral("rate decision and dot plot"));
        addCheckpoint(checkpoints, QStringLiteral("CPI"), QStringLiteral("inflation confirmation"));
        addCheckpoint(checkpoints, QStringLiteral("PCE"), QStringLiteral("Fed preferred inflation data"));
        addCheckpoint(checkpoints, QString::fromUtf8("非农"), QStringLiteral("employment confirmation"));
    }

    const bool isChinaMonetary = region == MacroEventRegion::China
        && (type == MacroEventType::MonetaryPolicy
            || containsAny(text, {QStringLiteral("LPR"), QStringLiteral("MLF"), QString::fromUtf8("降准"),
                                  QString::fromUtf8("央行")}));
    if (isChinaMonetary) {
        addCheckpoint(checkpoints, QStringLiteral("LPR"), QStringLiteral("loan prime rate quote"));
        addCheckpoint(checkpoints, QStringLiteral("MLF"), QStringLiteral("medium-term lending facility operation"));
        addCheckpoint(checkpoints, QString::fromUtf8("央行公开市场操作"), QStringLiteral("liquidity operation"));
    }

    if (type == MacroEventType::FiscalPolicy
        || containsAny(text, {QString::fromUtf8("专项债"), QString::fromUtf8("财政"),
                              QString::fromUtf8("特别国债")})) {
        addCheckpoint(checkpoints, QString::fromUtf8("专项债发行"), QStringLiteral("fiscal funding pace"));
        addCheckpoint(checkpoints, QString::fromUtf8("政策细则落地"), QStringLiteral("implementation details"));
    }

    if (type == MacroEventType::IndustrialPolicy) {
        addCheckpoint(checkpoints, QString::fromUtf8("政策细则"), QStringLiteral("industry policy details"));
        addCheckpoint(checkpoints, QString::fromUtf8("企业订单"), QStringLiteral("company order validation"));
    }

    if (type == MacroEventType::CommoditySupplyDemand) {
        addCheckpoint(checkpoints, QString::fromUtf8("库存数据"), QStringLiteral("inventory confirmation"));
        addCheckpoint(checkpoints, QString::fromUtf8("供需数据"), QStringLiteral("supply and demand confirmation"));
    }

    if (type == MacroEventType::GeopoliticsTrade) {
        addCheckpoint(checkpoints, QString::fromUtf8("出口限制细则"), QStringLiteral("restriction details"));
        addCheckpoint(checkpoints, QString::fromUtf8("关税或制裁清单"), QStringLiteral("tariff or sanction list"));
    }

    if (type == MacroEventType::MarketInstitution) {
        addCheckpoint(checkpoints, QString::fromUtf8("证监会细则"), QStringLiteral("market rule details"));
        addCheckpoint(checkpoints, QString::fromUtf8("交易所执行规则"), QStringLiteral("exchange implementation rules"));
    }

    return checkpoints;
}
