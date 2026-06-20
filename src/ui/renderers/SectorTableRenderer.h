#pragma once

#include "ui/AppTheme.h"

#include <QString>

struct AnalysisResult;

namespace InvestInsight::Ui {

struct SectorTableRenderOptions {
    bool simpleMode = true;
    QString searchText;
    int trendIndex = 0;
    int actionIndex = 0;
    int sortIndex = 0;
};

class SectorTableRenderer {
public:
    static QString render(const AnalysisResult &analysis,
                          const ThemeColors &theme,
                          const SectorTableRenderOptions &options = {});
};

} // namespace InvestInsight::Ui
