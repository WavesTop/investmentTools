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

QString buildWidgetStyleSheet(const ThemeColors &t)
{
    QString s = QLatin1String(R"(
    QMainWindow { background: @winBg@; }
    QTabWidget::pane {
        border: 1px solid @border@; border-top: none;
        background: @pane@; border-radius: 0 0 10px 10px;
    }
    QTabBar { qproperty-drawBase: 0; background: transparent; }
    QTabBar::tab {
        padding: 10px 18px; font-size: 12px;
        background: transparent; color: @text@;
        border: none; border-bottom: 2px solid transparent;
        margin-right: 2px;
        min-width: 50px;
    }
    QTabBar::tab:hover {
        color: @accent@;
        border-bottom: 2px solid rgba(99,102,241,0.35);
    }
    QTabBar::tab:selected {
        color: @accent@; font-weight: 600;
        border-bottom: 2.5px solid @accent@;
    }
    QTabBar::close-button {
        image: url(nofile); border: none;
        subcontrol-position: right; padding: 2px;
        width: 12px; height: 12px;
    }
    QTabBar::close-button:hover { background: rgba(128,128,128,0.2); border-radius: 6px; }
    QTabBar::scroller { width: 0px; }
    QTabBar QToolButton { width: 0px; border: none; }

    QPushButton {
        background: @btn@; color: white; border: none; border-radius: 8px;
        padding: 9px 18px; font-size: 12px; font-weight: 600;
    }
    QPushButton:hover { background: @btnH@; }
    QPushButton:pressed { background: @btnH@; padding-top: 8px; }
    QPushButton:disabled { background: @btnD@; color: rgba(255,255,255,0.4); }
    QPushButton#refreshBtn {
        padding: 8px 28px; font-size: 13px; font-weight: 700;
        border-radius: 8px;
        background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 @btn@,stop:1 @chunk1@);
    }
    QPushButton#refreshBtn:hover {
        background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 @btnH@,stop:1 @btn@);
    }

    QLineEdit {
        border: 1px solid rgba(0,0,0,0.14); border-radius: 8px; padding: 8px 12px;
        font-size: 12px; background: @pane@; color: @text@;
        min-height: 18px;
        selection-background-color: rgba(99,102,241,0.18);
    }
    QLineEdit:focus { border-color: @btn@; }
    QLineEdit:disabled { background: @tabBg@; color: rgba(0,0,0,0.35); }

    QComboBox {
        border: 1px solid @border@; border-radius: 8px; padding: 7px 32px 7px 10px;
        font-size: 12px; background: @pane@; color: @text@;
        min-height: 28px;
    }
    QComboBox:hover { border-color: rgba(99,102,241,0.5); }
    QComboBox:focus { border-color: @btn@; }
    QComboBox::drop-down {
        subcontrol-origin: padding;
        subcontrol-position: top right;
        width: 28px;
        border: none;
        background: transparent;
    }
    QComboBox::down-arrow {
        width: 10px; height: 10px;
    }
    QComboBox QAbstractItemView {
        border: 1px solid @border@; border-radius: 8px;
        background: @pane@; color: @text@;
        selection-background-color: rgba(99,102,241,0.12);
        outline: none; padding: 4px;
        font-size: 12px;
    }
    QComboBox QAbstractItemView::item {
        padding: 6px 12px; min-height: 28px; border-radius: 4px;
        color: @text@; background: transparent;
    }
    QComboBox QAbstractItemView::item:hover {
        background: rgba(99,102,241,0.08); color: @text@;
    }
    QComboBox QAbstractItemView::item:selected {
        background: rgba(99,102,241,0.14); color: @text@;
    }

    QSpinBox {
        border: 1px solid @border@; border-radius: 8px; padding: 7px 10px;
        font-size: 12px; background: @pane@; color: @text@;
        min-height: 18px;
    }
    QSpinBox:focus { border-color: @btn@; }

    QCheckBox { font-size: 12px; spacing: 6px; padding: 4px 0px; }

    QTableWidget {
        border: 1px solid @border@; border-radius: 10px;
        gridline-color: transparent; background: @pane@; color: @text@;
        font-size: 12px;
        selection-background-color: rgba(99,102,241,0.06);
        alternate-background-color: rgba(0,0,0,0.015);
    }
    QTableWidget::item { padding: 6px 8px; border: none; }
    QTableWidget::item:selected { background: rgba(99,102,241,0.07); color: @text@; }
    QTableWidget::item:focus { border: none; outline: none; }
    QHeaderView::section {
        background: @pane@; color: @text@; border: none;
        border-bottom: 1.5px solid @border@; padding: 8px 10px;
        font-size: 11px; font-weight: 600;
    }

    QScrollBar:vertical {
        background: transparent; width: 5px; margin: 0;
    }
    QScrollBar::handle:vertical {
        background: rgba(128,128,128,0.22); min-height: 40px; border-radius: 2.5px;
    }
    QScrollBar::handle:vertical:hover { background: rgba(128,128,128,0.4); }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
    QScrollBar:horizontal {
        background: transparent; height: 5px; margin: 0;
    }
    QScrollBar::handle:horizontal {
        background: rgba(128,128,128,0.22); min-width: 40px; border-radius: 2.5px;
    }
    QScrollBar::handle:horizontal:hover { background: rgba(128,128,128,0.4); }
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
    QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }

    QGroupBox {
        border: 1px solid @border@; border-radius: 12px;
        margin-top: 36px; padding-top: 14px;
        font-weight: 600; font-size: 13px;
    }
    QGroupBox::title {
        subcontrol-origin: margin; subcontrol-position: top left;
        left: 16px; top: 4px; padding: 6px 14px;
        background: @pane@; color: @btn@; font-weight: 700;
    }

    QPushButton#secondaryBtn {
        background: transparent; color: @text@;
        border: 1px solid @border@; border-radius: 8px;
        font-weight: 500;
    }
    QPushButton#secondaryBtn:hover {
        background: rgba(99,102,241,0.06); border-color: rgba(99,102,241,0.3);
        color: @btn@;
    }
    QPushButton#dangerBtn {
        background: transparent; color: #DC2626;
        border: 1px solid rgba(239,68,68,0.25); border-radius: 8px;
    }
    QPushButton#dangerBtn:hover {
        background: rgba(239,68,68,0.06); border-color: #DC2626;
    }

    QProgressBar {
        border: none; border-radius: 2.5px; background: @barBg@;
        max-height: 5px;
    }
    QProgressBar::chunk {
        background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 @chunk1@,stop:1 @chunk2@);
        border-radius: 2.5px;
    }
    QLabel#statusLabel { color: @status@; font-size: 11px; font-weight: 500; }

    QToolButton {
        border: 1px solid @border@; border-radius: 6px; background: transparent;
        font-size: 12px; padding: 3px;
    }
    QToolButton:checked { background: rgba(99,102,241,0.08); border-color: rgba(99,102,241,0.3); }
    QToolButton:hover { background: rgba(128,128,128,0.06); }

    QTextBrowser {
        border: none;
        background: @pane@;
    }
    QScrollArea {
        border: none;
        background: transparent;
    }
    )");
    s.replace(QLatin1String("@winBg@"),  t.winBg);
    s.replace(QLatin1String("@border@"), t.paneBorder);
    s.replace(QLatin1String("@pane@"),   t.paneBg);
    s.replace(QLatin1String("@tabBg@"),  t.tabBg);
    s.replace(QLatin1String("@text@"),   t.bodyColor);
    s.replace(QLatin1String("@accent@"), t.tabSelectedColor);
    s.replace(QLatin1String("@btn@"),    t.btnBg);
    s.replace(QLatin1String("@btnH@"),   t.btnHover);
    s.replace(QLatin1String("@btnD@"),   t.btnDisabled);
    s.replace(QLatin1String("@barBg@"),  t.barBg);
    s.replace(QLatin1String("@chunk1@"), t.barChunk1);
    s.replace(QLatin1String("@chunk2@"), t.barChunk2);
    s.replace(QLatin1String("@status@"), t.statusColor);
    return s;
}

bool detectDarkMode()
{
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

} // namespace InvestInsight::Ui
