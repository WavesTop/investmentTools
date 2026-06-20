#pragma once

#include <QString>

namespace InvestInsight::Ui {

struct ThemeColors {
    QString winBg, paneBg, paneBorder, tabBg, tabSelectedBg, tabSelectedColor;
    QString btnBg, btnHover, btnDisabled, barBg, barChunk1, barChunk2, barText;
    QString statusColor;

    QString bodyBg, bodyColor, headingColor, metaColor, dividerColor;
    QString sectionTitleColor, sectionBorderColor;
    QString narrativeBg, narrativeColor;
    QString tableHeaderBg, tableBorder, tableHoverBg, tableAltRowBg;
    QString linkColor, linkHoverColor;
    QString formulaHeaderBg, formulaHighlightBg;
    QString neutralColor, mutedColor, subtleColor;
    QString cardBorder;
    QString warningBg, warningBorder, warningColor;
    QString disclaimerColor;
    QString newsItemBorder;

    QString chartBg, chartGrid, chartAxisText, chartTitleText;
};

ThemeColors lightTheme();
ThemeColors darkTheme();
bool detectDarkMode();

} // namespace InvestInsight::Ui
