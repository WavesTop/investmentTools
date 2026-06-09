#include "core/AdviceEngine.h"

QList<InvestmentAdvice> AdviceEngine::generate(const QList<FundImpact> &impacts) const
{
    QList<InvestmentAdvice> advices;
    for (const FundImpact &impact : impacts) {
        AdviceAction action = AdviceAction::Hold;
        if (impact.expectedMove >= 0.25) {
            action = AdviceAction::Increase;
        } else if (impact.expectedMove <= -0.25) {
            action = AdviceAction::Decrease;
        }

        QStringList reasons;
        reasons.push_back("预期相对变动指标：" + QString::number(impact.expectedMove, 'f', 2));
        reasons.push_back("置信度：" + QString::number(impact.confidence, 'f', 2));
        reasons.push_back("核心依据：\n" + impact.rationale);

        advices.push_back({
            impact.fundCategory,
            action,
            impact.confidence,
            reasons
        });
    }
    return advices;
}
