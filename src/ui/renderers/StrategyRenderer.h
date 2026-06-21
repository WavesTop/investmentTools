#pragma once

#include "ui/AppTheme.h"

#include <QString>

struct AnalysisResult;

namespace InvestInsight::Ui {

struct StrategyRenderOptions {
    bool simpleMode = true;
    QString portfolioBatchesJson;
};

class StrategyRenderer {
public:
    static QString render(const AnalysisResult &analysis,
                          const ThemeColors &theme,
                          const StrategyRenderOptions &options = {});
};

} // namespace InvestInsight::Ui
