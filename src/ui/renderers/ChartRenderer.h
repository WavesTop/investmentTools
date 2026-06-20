#pragma once

#include "ui/AppTheme.h"

#include <QPixmap>

struct SectorSnapshot;

namespace InvestInsight::Ui {

class ChartRenderer {
public:
    static QPixmap buildTrendChart(const SectorSnapshot &snap,
                                   const ThemeColors &theme,
                                   int width,
                                   int minHeight);
};

} // namespace InvestInsight::Ui
