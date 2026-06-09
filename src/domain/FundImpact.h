#pragma once

#include <QString>

struct FundImpact
{
    QString fundCategory;
    double expectedMove = 0.0; // percentage-like relative indicator
    double confidence = 0.0;   // [0.0, 1.0]
    QString rationale;
};
