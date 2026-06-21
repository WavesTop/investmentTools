#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <QAbstractButton>
#include <QFutureWatcher>
#include <QMainWindow>
#include <QMap>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTextBrowser>
#include <QUrl>
#include <QWheelEvent>

#include "core/InsightOrchestrator.h"

// 持仓类型列代理：单元格显示纯文字，点击弹出 QComboBox 选择器
class TypeComboDelegate : public QStyledItemDelegate
{
public:
    explicit TypeComboDelegate(const QStringList &options, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_options(options) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &) const override;
private:
    QStringList m_options;
};

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTabWidget;
class QTimer;
class QVBoxLayout;

class ScrollableTabBar : public QTabBar
{
public:
    explicit ScrollableTabBar(QWidget *parent = nullptr) : QTabBar(parent)
    {
        setMouseTracking(true);
    }

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        const int delta = event->angleDelta().y();
        if (delta > 0 && currentIndex() > 0) {
            setCurrentIndex(currentIndex() - 1);
        } else if (delta < 0 && currentIndex() < count() - 1) {
            setCurrentIndex(currentIndex() + 1);
        }
        event->accept();
    }
};

class TabCloseButton : public QAbstractButton
{
public:
    explicit TabCloseButton(QWidget *parent = nullptr) : QAbstractButton(parent)
    {
        setFixedSize(16, 16);
        setCursor(Qt::ArrowCursor);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (underMouse()) {
            p.setBrush(QColor(128, 128, 128, 60));
            p.setPen(Qt::NoPen);
            p.drawEllipse(rect().adjusted(1, 1, -1, -1));
        }
        QPen pen(palette().color(QPalette::WindowText));
        pen.setWidthF(1.3);
        p.setPen(pen);
        const int m = 5;
        p.drawLine(m, m, width() - m, height() - m);
        p.drawLine(width() - m, m, m, height() - m);
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *) override { update(); }
#else
    void enterEvent(QEvent *) override { update(); }
#endif
    void leaveEvent(QEvent *) override { update(); }
};

class ScrollableTabWidget : public QTabWidget
{
public:
    explicit ScrollableTabWidget(QWidget *parent = nullptr) : QTabWidget(parent)
    {
        auto *bar = new ScrollableTabBar(this);
        setTabBar(bar);
    }

    int addClosableTab(QWidget *widget, const QString &label)
    {
        int idx = addTab(widget, label);
        auto *btn = new TabCloseButton(tabBar());
        connect(btn, &QAbstractButton::clicked, this, [this, widget]() {
            int i = indexOf(widget);
            if (i > 0) emit tabCloseRequested(i);
        });
        tabBar()->setTabButton(idx, QTabBar::RightSide, btn);
        return idx;
    }
};

class ClickableBrowser : public QTextBrowser
{
public:
    explicit ClickableBrowser(QWidget *parent = nullptr);
    std::function<void(int)> onTabJump;
    std::function<void(const QString &)> onIndexJump;

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void doSetSource(const QUrl &url, QTextDocument::ResourceType type) override;
#else
    void setSource(const QUrl &url) override;
#endif
};

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;
    void autoAnalyze();

private:
    void beginRefresh();
    void onRefreshFinished();
    void pollProgress();
    void buildUi();
    void buildSetupPage(QVBoxLayout *root);
    void buildMainPage(QVBoxLayout *root);
    void saveAndEnterMainPage();
    void loadSavedAIConfigToForm();
    QList<AIProvider> collectProvidersFromForm() const;
    QMap<QString, double> collectPortfolioFromTable() const;
    void loadPortfolioToTable();

    void renderOverview(const AnalysisResult &analysis);
    void openSectorTab(const QString &sectorName);
    void openIndexTab(const QString &indexKey);
    void onTabCloseRequested(int index);
    void openChatTab();
    void sendChatMessage();
    QString buildDataDashboardHtml(const AnalysisResult &analysis) const;
    QString buildEventRadarHtml(const AnalysisResult &analysis) const;
    QString buildSectorTableHtml(const AnalysisResult &analysis) const;
    QString buildStrategyHtml(const AnalysisResult &analysis) const;
    QString buildSectorHtml(const SectorSnapshot &sector, bool aiAvailable, bool simpleMode = false) const;
    QString buildIndexHtml(const IndexSnapshot &idx, bool aiAvailable, bool simpleMode = false) const;

    InsightOrchestrator m_orchestrator;
    QStackedWidget *m_pages = nullptr;
    QWidget *m_setupPage = nullptr;
    QWidget *m_mainPage = nullptr;
    // setup page AI key inputs (indexed to match kProviderDefs)
    QLineEdit *m_deepseekKeyEdit  = nullptr;
    QLineEdit *m_openaiKeyEdit    = nullptr;
    QLineEdit *m_qwenKeyEdit      = nullptr;
    QLineEdit *m_claudeKeyEdit    = nullptr;
    QLineEdit *m_geminiKeyEdit    = nullptr;
    QCheckBox *m_setupAiEnabled   = nullptr;
    QSpinBox  *m_topNSpin         = nullptr;
    QPushButton *m_enterButton    = nullptr;
    // portfolio table
    QTableWidget *m_portfolioTable = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_backToSetupButton = nullptr;
    QCheckBox *m_aiToggle = nullptr;
    QTabWidget *m_tabWidget = nullptr;
    QTabWidget *m_overviewSubTabs = nullptr;
    ClickableBrowser *m_overviewBrowser = nullptr;
    ClickableBrowser *m_dashboardBrowser = nullptr;
    ClickableBrowser *m_eventRadarBrowser = nullptr;
    ClickableBrowser *m_strategyBrowser = nullptr;
    QLineEdit  *m_overviewSearch  = nullptr;
    QComboBox  *m_trendFilter     = nullptr;
    QComboBox  *m_actionFilter    = nullptr;
    QComboBox  *m_sortBy          = nullptr;
    QComboBox  *m_viewMode        = nullptr;  // 0=简明, 1=专业
    QMap<QString, int> m_openSectorTabs;
    int m_chatTabIndex = -1;
    QTextBrowser *m_chatDisplay = nullptr;
    QLineEdit *m_chatInput = nullptr;
    QPushButton *m_chatSendBtn = nullptr;
    AnalysisResult m_lastResult;
    QLabel *m_statusLabel = nullptr;
    QProgressBar *m_loadingBar = nullptr;
    QTimer *m_autoRefreshTimer = nullptr;
    QFutureWatcher<AnalysisResult> *m_refreshWatcher = nullptr;
    QTimer *m_progressPollTimer = nullptr;
    bool m_isRefreshing = false;
    bool m_darkMode = false;
    std::atomic<int> m_currentProgress{0};
    QString m_currentStage;
    std::shared_ptr<std::function<void()>> m_reorderSoldFn;
};
