#include "ui/AppTheme.h"

#include <QApplication>
#include <QPalette>

namespace InvestInsight::Ui {

ThemeColors lightTheme()
{
    ThemeColors t;
    t.winBg = "#F5F6FA"; t.paneBg = "#FFFFFF"; t.paneBorder = "#E2E8F0";
    t.tabBg = "#EDF0F7"; t.tabSelectedBg = "#FFFFFF"; t.tabSelectedColor = "#4F46E5";
    t.btnBg = "#4F46E5"; t.btnHover = "#4338CA"; t.btnDisabled = "#CBD5E1";
    t.barBg = "#E2E8F0"; t.barChunk1 = "#4F46E5"; t.barChunk2 = "#818CF8"; t.barText = "#334155";
    t.statusColor = "#475569";

    t.bodyBg = "#FFFFFF"; t.bodyColor = "#1E293B"; t.headingColor = "#1E293B";
    t.metaColor = "#94A3B8"; t.dividerColor = "#F1F5F9";
    t.sectionTitleColor = "#1E293B"; t.sectionBorderColor = "#4F46E5";
    t.narrativeBg = "#F8FAFC"; t.narrativeColor = "#334155";
    t.tableHeaderBg = "#F8FAFC"; t.tableBorder = "#E2E8F0"; t.tableHoverBg = "#F1F5F9"; t.tableAltRowBg = "#FAFBFE";
    t.linkColor = "#4F46E5"; t.linkHoverColor = "#4338CA";
    t.formulaHeaderBg = "#F5F3FF"; t.formulaHighlightBg = "#F5F3FF";
    t.neutralColor = "#475569"; t.mutedColor = "#64748B"; t.subtleColor = "#94A3B8";
    t.cardBorder = "#E2E8F0";
    t.warningBg = "#FFFBEB"; t.warningBorder = "#F59E0B"; t.warningColor = "#B45309";
    t.disclaimerColor = "#CBD5E1"; t.newsItemBorder = "#F1F5F9";

    t.chartBg = "#FAFBFE"; t.chartGrid = "#F1F5F9"; t.chartAxisText = "#94A3B8"; t.chartTitleText = "#334155";
    return t;
}

ThemeColors darkTheme()
{
    ThemeColors t;
    t.winBg = "#0F172A"; t.paneBg = "#1E293B"; t.paneBorder = "#334155";
    t.tabBg = "#1E293B"; t.tabSelectedBg = "#1E293B"; t.tabSelectedColor = "#818CF8";
    t.btnBg = "#6366F1"; t.btnHover = "#4F46E5"; t.btnDisabled = "#475569";
    t.barBg = "#1E293B"; t.barChunk1 = "#6366F1"; t.barChunk2 = "#A5B4FC"; t.barText = "#E2E8F0";
    t.statusColor = "#94A3B8";

    t.bodyBg = "#0F172A"; t.bodyColor = "#E2E8F0"; t.headingColor = "#F1F5F9";
    t.metaColor = "#64748B"; t.dividerColor = "#1E293B";
    t.sectionTitleColor = "#F1F5F9"; t.sectionBorderColor = "#6366F1";
    t.narrativeBg = "#1E293B"; t.narrativeColor = "#CBD5E1";
    t.tableHeaderBg = "#1E293B"; t.tableBorder = "#334155"; t.tableHoverBg = "#273449"; t.tableAltRowBg = "#162032";
    t.linkColor = "#818CF8"; t.linkHoverColor = "#A5B4FC";
    t.formulaHeaderBg = "#1E1B3A"; t.formulaHighlightBg = "#1E1B3A";
    t.neutralColor = "#94A3B8"; t.mutedColor = "#64748B"; t.subtleColor = "#64748B";
    t.cardBorder = "#334155";
    t.warningBg = "#27200B"; t.warningBorder = "#D97706"; t.warningColor = "#FCD34D";
    t.disclaimerColor = "#475569"; t.newsItemBorder = "#1E293B";

    t.chartBg = "#0F172A"; t.chartGrid = "#1E293B"; t.chartAxisText = "#64748B"; t.chartTitleText = "#E2E8F0";
    return t;
}

bool detectDarkMode()
{
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

} // namespace InvestInsight::Ui
