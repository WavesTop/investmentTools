#pragma once

#include <QString>
#include <QStringList>

enum class AdviceAction
{
    Increase,
    Hold,
    Decrease
};

struct InvestmentAdvice
{
    QString fundCategory;
    AdviceAction action = AdviceAction::Hold;
    double confidence = 0.0;
    QStringList reasons;
};
