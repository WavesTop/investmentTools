#include "core/ReportComposer.h"

#include <QDateTime>

namespace {
QString actionText(AdviceAction action)
{
    switch (action) {
    case AdviceAction::Increase:
        return "增加配置";
    case AdviceAction::Decrease:
        return "降低配置";
    case AdviceAction::Hold:
    default:
        return "继续持有";
    }
}
} // namespace

QString ReportComposer::compose(const QList<InvestmentAdvice> &advices) const
{
    QString report;
    report += "=== InvestInsight 行业-基金联动分析报告 ===\n";
    report += "生成时间：" + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + "\n\n";
    report += "指标说明：\n";
    report += "1) 预期相对变动指标：用于表示基金类别受当前行业信号影响的方向和强度，范围通常为 [-1.00, 1.00]。\n";
    report += "   - 大于 0：偏正向影响；小于 0：偏负向影响；接近 0：影响不明显。\n";
    report += "2) 置信度：用于表示本次建议可靠性，范围 [0.00, 1.00]。\n";
    report += "   - 越接近 1.00，说明数据质量、样本有效性和信号一致性越高。\n";
    report += "3) 建议动作阈值（当前版本）：预期相对变动指标 >= 0.25 为“增加配置”；<= -0.25 为“降低配置”；其余为“继续持有”。\n\n";

    if (advices.isEmpty()) {
        report += "未获取到可分析数据，请检查数据源配置。\n";
        return report;
    }

    report += "以下建议基于公开数据源（财经新闻RSS与行情接口）和规则引擎推断，仅供研究参考，不构成投资承诺。\n\n";
    for (const InvestmentAdvice &advice : advices) {
        report += "基金类别：" + advice.fundCategory + "\n";
        report += "建议动作：" + actionText(advice.action) + "\n";
        report += "置信度：" + QString::number(advice.confidence, 'f', 2) + "\n";
        report += "理由：\n";
        for (const QString &reason : advice.reasons) {
            report += "- " + reason + "\n";
        }
        report += "\n";
    }

    report += "风险提示：建议结合持仓周期、回撤承受能力与资产配置比例进行二次判断。\n";
    return report;
}
