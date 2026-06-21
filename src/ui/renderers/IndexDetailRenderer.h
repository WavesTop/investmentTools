#pragma once

#include "ui/AppTheme.h"

#include <QString>

struct IndexSnapshot;

namespace InvestInsight::Ui {

struct IndexDetailRenderOptions {
    bool simpleMode = false;
    int chartWidth = 800;
    int chartHeight = 720;
};

class IndexDetailRenderer {
public:
    static QString render(const IndexSnapshot &index,
                          const ThemeColors &theme,
                          const IndexDetailRenderOptions &options = {});
};

} // namespace InvestInsight::Ui
