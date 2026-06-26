#pragma once

#include "ui/AppTheme.h"

#include <QString>

struct SectorSnapshot;

namespace InvestInsight::Ui {

struct SectorDetailRenderOptions {
    bool aiAvailable = false;
    bool simpleMode = false;
    int chartWidth = 800;
    int chartHeight = 640;
};

class SectorDetailRenderer {
public:
    static QString render(const SectorSnapshot &sector,
                          const ThemeColors &theme,
                          const SectorDetailRenderOptions &options = {});
};

} // namespace InvestInsight::Ui
