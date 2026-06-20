#pragma once

#include "ui/AppTheme.h"

#include <QMap>
#include <QString>

struct AnalysisResult;

namespace InvestInsight::Ui {

struct DashboardRenderOptions {
    bool simpleMode = true;
    QMap<QString, double> portfolio;
};

class DashboardRenderer {
public:
    static QString render(const AnalysisResult &analysis,
                          const ThemeColors &theme,
                          const DashboardRenderOptions &options = {});
};

} // namespace InvestInsight::Ui
