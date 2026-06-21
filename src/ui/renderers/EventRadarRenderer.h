#pragma once

#include "ui/AppTheme.h"

#include <QString>

struct AnalysisResult;

namespace InvestInsight::Ui {

struct EventRadarRenderOptions {
    bool simpleMode = false;
    int maxItems = 8;
};

class EventRadarRenderer {
public:
    static QString render(const AnalysisResult &analysis,
                          const ThemeColors &theme,
                          const EventRadarRenderOptions &options = {});
};

} // namespace InvestInsight::Ui
