#include "ui/MainWindow.h"
#include "ui/AppTheme.h"
#include "ui/renderers/DashboardRenderer.h"
#include "ui/renderers/SectorTableRenderer.h"
#include "ui/renderers/StrategyRenderer.h"
#include "ui/renderers/SectorDetailRenderer.h"
#include "ui/renderers/IndexDetailRenderer.h"
#include "ui/renderers/EventRadarRenderer.h"

#include <algorithm>

// ─── TypeComboDelegate 实现 ──────────────────────────────────────────────────
#include <QComboBox>

QWidget *TypeComboDelegate::createEditor(QWidget *parent,
    const QStyleOptionViewItem &, const QModelIndex &) const
{
    auto *combo = new QComboBox(parent);
    combo->addItems(m_options);
    combo->setFrame(false);
    return combo;
}
void TypeComboDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto *combo = qobject_cast<QComboBox *>(editor);
    if (!combo) return;
    const QString val = index.data(Qt::EditRole).toString();
    const int idx = combo->findText(val);
    if (idx >= 0) combo->setCurrentIndex(idx);
}
void TypeComboDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
    const QModelIndex &index) const
{
    auto *combo = qobject_cast<QComboBox *>(editor);
    if (combo) model->setData(index, combo->currentText(), Qt::EditRole);
}
void TypeComboDelegate::updateEditorGeometry(QWidget *editor,
    const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}
// ─────────────────────────────────────────────────────────────────────────────
#include <QCheckBox>
#include <QButtonGroup>
#include <QComboBox>
#include <QCompleter>
#include <QDesktopServices>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QDateTime>
#include <QDateEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>
#include <QProxyStyle>

class PaddedTabStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;
    QSize sizeFromContents(ContentsType type, const QStyleOption *opt,
                           const QSize &sz, const QWidget *w) const override {
        QSize s = QProxyStyle::sizeFromContents(type, opt, sz, w);
        if (type == CT_TabBarTab)
            s.rwidth() += 28;
        return s;
    }
};

using InvestInsight::Ui::ThemeColors;
namespace UiTheme = InvestInsight::Ui;

// --------------- ClickableBrowser ---------------

ClickableBrowser::ClickableBrowser(QWidget *parent) : QTextBrowser(parent) {}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ClickableBrowser::doSetSource(const QUrl &url, QTextDocument::ResourceType /*type*/)
#else
void ClickableBrowser::setSource(const QUrl &url)
#endif
{
    const QString s = url.toString();
    if (s.startsWith("jumpi-")) {
        const QString key = s.mid(6);
        if (!key.isEmpty() && onIndexJump) onIndexJump(key);
        return;
    }
    if (s.startsWith("jump-")) {
        bool ok = false;
        const int idx = s.mid(5).toInt(&ok);
        if (ok && onTabJump) onTabJump(idx);
        return;
    }
    if (url.scheme() == "http" || url.scheme() == "https") {
        QDesktopServices::openUrl(url);
        return;
    }
}

static ThemeColors s_themeStorage;
static const ThemeColors *s_theme = &s_themeStorage;

namespace {
QString pct(double v)
{
    return (v >= 0 ? "+" : "") + QString::number(v, 'f', 2) + "%";
}

QString num(double v, int d = 2) { return QString::number(v, 'f', d); }
} // namespace

// --------------- Core logic ---------------

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_darkMode = UiTheme::detectDarkMode();
    s_themeStorage = m_darkMode ? UiTheme::darkTheme() : UiTheme::lightTheme();
    s_theme = &s_themeStorage;
    buildUi();
}

void MainWindow::autoAnalyze()
{
    QTimer::singleShot(200, this, [this]() {
        saveAndEnterMainPage();
        QTimer::singleShot(500, this, [this]() {
            beginRefresh();
        });
    });
}

void MainWindow::beginRefresh()
{
    if (m_isRefreshing) return;
    m_isRefreshing = true;
    m_refreshButton->setEnabled(false);
    m_aiToggle->setEnabled(false);
    m_tabWidget->setEnabled(false);
    if (m_backToSetupButton) m_backToSetupButton->setEnabled(false);
    m_currentProgress = 0;
    m_loadingBar->setVisible(true);
    m_loadingBar->setRange(0, 100);
    m_loadingBar->setValue(0);
    m_loadingBar->setFormat("%p%");
    m_statusLabel->setText("正在拉取数据并分析...");
    m_progressPollTimer->start(200);

    m_refreshWatcher->setFuture(QtConcurrent::run([this]() {
        return m_orchestrator.runAnalysis([this](int pct, const QString &stage) {
            m_currentProgress.store(pct);
            m_currentStage = stage;
        });
    }));
}

void MainWindow::pollProgress()
{
    const int p = m_currentProgress.load();
    m_loadingBar->setValue(p);
    m_loadingBar->setFormat(QString("%1%").arg(p));
    m_statusLabel->setText(m_currentStage);
}

void MainWindow::onRefreshFinished()
{
    const AnalysisResult result = m_refreshWatcher->result();
    m_lastResult = result;

    while (m_tabWidget->count() > 1) {
        QWidget *w = m_tabWidget->widget(1);
        m_tabWidget->removeTab(1);
        w->deleteLater();
    }
    m_openSectorTabs.clear();

    // 缓存板块名到 QSettings，供持仓下拉框使用
    // 合并：本次API返回的板块 + 用户持仓中已有的板块名，防止因API波动导致持仓板块从下拉框消失
    QSet<QString> sectorNameSet;
    QStringList sectorNameList;
    for (const SectorSnapshot &sec : result.sectors) {
        if (!sectorNameSet.contains(sec.industry)) {
            sectorNameSet.insert(sec.industry);
            sectorNameList.push_back(sec.industry);
        }
    }
    QSettings cacheSettings("InvestInsight", "InvestInsight");
    const QStringList oldCached = cacheSettings.value("cache/sector_names").toStringList();
    for (const QString &old : oldCached) {
        if (!sectorNameSet.contains(old)) {
            sectorNameSet.insert(old);
            sectorNameList.push_back(old);
        }
    }
    const QJsonDocument portfolioDoc = QJsonDocument::fromJson(
        cacheSettings.value("portfolio/batches_json").toString().toUtf8());
    if (portfolioDoc.isArray()) {
        for (const QJsonValue &v : portfolioDoc.array()) {
            const QString sec = v.toObject().value("sector").toString();
            if (!sec.isEmpty() && !sectorNameSet.contains(sec)) {
                sectorNameSet.insert(sec);
                sectorNameList.push_back(sec);
            }
        }
    }
    cacheSettings.setValue("cache/sector_names", sectorNameList);

    renderOverview(result);
    m_tabWidget->setEnabled(true);
    m_refreshButton->setEnabled(true);
    m_aiToggle->setEnabled(m_orchestrator.isAIAvailable());
    if (m_backToSetupButton) m_backToSetupButton->setEnabled(true);
    m_loadingBar->setValue(100);
    m_loadingBar->setVisible(false);
    m_progressPollTimer->stop();
    QString statusText = "✓ " + QDateTime::currentDateTime().toString("hh:mm")
        + " · " + num(result.sectors.size(), 0) + " 板块";
    if (result.aiAvailable) {
        const int aiCnt = m_orchestrator.aiProviderCount();
        statusText += aiCnt > 1
            ? QString(" · AI×%1").arg(aiCnt)
            : " · AI";
        if (!result.aiErrors.isEmpty()) {
            statusText += "(!err)";
        }
    } else if (!result.aiErrors.isEmpty()) {
        statusText += " — AI 调用失败：" + result.aiErrors.join("；");
    } else {
        statusText += " — 规则引擎（可在配置页填写 AI Key 启用深度分析）";
    }
    m_statusLabel->setText(statusText);
    QTimer::singleShot(3000, this, [this]() { if (!m_isRefreshing) m_loadingBar->setVisible(false); });
    m_isRefreshing = false;
}

void MainWindow::renderOverview(const AnalysisResult &analysis)
{
    if (m_dashboardBrowser)
        m_dashboardBrowser->setHtml(buildDataDashboardHtml(analysis));
    if (m_eventRadarBrowser)
        m_eventRadarBrowser->setHtml(buildEventRadarHtml(analysis));
    if (m_overviewBrowser)
        m_overviewBrowser->setHtml(buildSectorTableHtml(analysis));
    if (m_strategyBrowser)
        m_strategyBrowser->setHtml(buildStrategyHtml(analysis));
}

void MainWindow::openSectorTab(const QString &sectorName)
{
    if (m_openSectorTabs.contains(sectorName)) {
        m_tabWidget->setCurrentIndex(m_openSectorTabs.value(sectorName));
        if (m_workspaceTitle) m_workspaceTitle->setText(sectorName + QString::fromUtf8("详情"));
        if (m_workspaceMeta) m_workspaceMeta->setText(QString::fromUtf8("行情、事件、路径、技术面和证据新闻"));
        return;
    }

    for (const SectorSnapshot &s : m_lastResult.sectors) {
        if (s.industry == sectorName) {
            auto *browser = new QTextBrowser();
            browser->setOpenExternalLinks(true);
            const bool isSimple = m_viewMode && m_viewMode->currentIndex() == 0;
            browser->setHtml(buildSectorHtml(s, m_lastResult.aiAvailable, isSimple));
            auto *stw = static_cast<ScrollableTabWidget *>(m_tabWidget);
            const int idx = stw->addClosableTab(browser, s.industry + " " + pct(s.todayChangePct));
            m_openSectorTabs.insert(sectorName, idx);
            m_tabWidget->setCurrentIndex(idx);
            if (m_workspaceTitle) m_workspaceTitle->setText(s.industry + QString::fromUtf8("详情"));
            if (m_workspaceMeta) m_workspaceMeta->setText(QString::fromUtf8("行情、事件、路径、技术面和证据新闻"));
            return;
        }
    }
}

void MainWindow::openIndexTab(const QString &indexKey)
{
    const QString tabKey = "index:" + indexKey;
    if (m_openSectorTabs.contains(tabKey)) {
        m_tabWidget->setCurrentIndex(m_openSectorTabs.value(tabKey));
        if (m_workspaceTitle) m_workspaceTitle->setText(indexKey + QString::fromUtf8(" 指数详情"));
        if (m_workspaceMeta) m_workspaceMeta->setText(QString::fromUtf8("指数趋势、市场风控和数据质量"));
        return;
    }

    const MarketContext &mctx = m_lastResult.marketCtx;
    auto pick = [&](const QString &k) -> const IndexSnapshot * {
        if (k == "SH") return &mctx.shanghai;
        if (k == "SZ") return &mctx.shenzhen;
        if (k == "CY") return &mctx.chinext;
        if (k == "CSI300") return &mctx.csi300;
        if (k == "CSI500") return &mctx.csi500;
        if (k == "NASDAQ") return &mctx.nasdaq;
        if (k == "SP500") return &mctx.sp500;
        if (k == "DJI") return &mctx.dowjones;
        return nullptr;
    };
    const IndexSnapshot *idx = pick(indexKey);
    if (!idx || idx->name.isEmpty()) return;

    auto *browser = new QTextBrowser();
    browser->setOpenExternalLinks(true);
    const bool isSimple = m_viewMode && m_viewMode->currentIndex() == 0;
    browser->setHtml(buildIndexHtml(*idx, m_lastResult.aiAvailable, isSimple));
    auto *stw = static_cast<ScrollableTabWidget *>(m_tabWidget);
    const int tabIdx = stw->addClosableTab(browser, idx->name + " " + pct(idx->changePct));
    m_openSectorTabs.insert(tabKey, tabIdx);
    m_tabWidget->setCurrentIndex(tabIdx);
    if (m_workspaceTitle) m_workspaceTitle->setText(idx->name + QString::fromUtf8("详情"));
    if (m_workspaceMeta) m_workspaceMeta->setText(QString::fromUtf8("指数趋势、市场风控和数据质量"));
}

void MainWindow::onTabCloseRequested(int index)
{
    if (index <= 0) return;

    if (index == m_chatTabIndex) {
        m_tabWidget->removeTab(index);
        m_chatTabIndex = -1;
        m_chatDisplay = nullptr;
        m_chatInput = nullptr;
        m_chatSendBtn = nullptr;
        QMap<QString, int> updated;
        for (auto it = m_openSectorTabs.constBegin(); it != m_openSectorTabs.constEnd(); ++it)
            updated.insert(it.key(), it.value() > index ? it.value() - 1 : it.value());
        m_openSectorTabs = updated;
        return;
    }

    QWidget *w = m_tabWidget->widget(index);
    const QString closedName = m_openSectorTabs.key(index);

    m_tabWidget->removeTab(index);
    w->deleteLater();
    m_openSectorTabs.remove(closedName);

    QMap<QString, int> updated;
    for (auto it = m_openSectorTabs.constBegin(); it != m_openSectorTabs.constEnd(); ++it) {
        updated.insert(it.key(), it.value() > index ? it.value() - 1 : it.value());
    }
    m_openSectorTabs = updated;
    if (m_chatTabIndex > index) --m_chatTabIndex;
}

// ========== 子Tab1: 数据总览 ==========
QString MainWindow::buildDataDashboardHtml(const AnalysisResult &analysis) const
{
    UiTheme::DashboardRenderOptions options;
    options.simpleMode = m_viewMode && m_viewMode->currentIndex() == 0;
    options.portfolio = m_orchestrator.portfolio();
    return UiTheme::DashboardRenderer::render(analysis, *s_theme, options);
}

QString MainWindow::buildEventRadarHtml(const AnalysisResult &analysis) const
{
    UiTheme::EventRadarRenderOptions options;
    options.simpleMode = m_viewMode && m_viewMode->currentIndex() == 0;
    return UiTheme::EventRadarRenderer::render(analysis, *s_theme, options);
}

// ========== 子Tab2: 板块&指数信息（搜索/过滤 + 混合表 + 审计摘要）==========
QString MainWindow::buildSectorTableHtml(const AnalysisResult &analysis) const
{
    UiTheme::SectorTableRenderOptions options;
    options.simpleMode = m_viewMode && m_viewMode->currentIndex() == 0;
    options.searchText = m_overviewSearch ? m_overviewSearch->text().trimmed() : QString();
    options.trendIndex = m_trendFilter ? m_trendFilter->currentIndex() : 0;
    options.actionIndex = m_actionFilter ? m_actionFilter->currentIndex() : 0;
    options.sortIndex = m_sortBy ? m_sortBy->currentIndex() : 0;
    return UiTheme::SectorTableRenderer::render(analysis, *s_theme, options);
}

// ========== 子Tab3: 投资策略 ==========
QString MainWindow::buildStrategyHtml(const AnalysisResult &analysis) const
{
    UiTheme::StrategyRenderOptions options;
    options.simpleMode = m_viewMode && m_viewMode->currentIndex() == 0;
    QSettings settings("InvestInsight", "InvestInsight");
    const QString batchKey = settings.contains("portfolio/batches_json")
        ? "portfolio/batches_json" : "portfolio/entries_json";
    options.portfolioBatchesJson = settings.value(batchKey).toString();
    return UiTheme::StrategyRenderer::render(analysis, *s_theme, options);
}

QString MainWindow::buildSectorHtml(const SectorSnapshot &s, bool aiAvailable, bool simpleMode) const
{
    UiTheme::SectorDetailRenderOptions options;
    options.aiAvailable = aiAvailable;
    options.simpleMode = simpleMode;
    return UiTheme::SectorDetailRenderer::render(s, *s_theme, options);
}

QString MainWindow::buildIndexHtml(const IndexSnapshot &idx, bool aiAvailable, bool simpleMode) const
{
    Q_UNUSED(aiAvailable);
    UiTheme::IndexDetailRenderOptions options;
    options.simpleMode = simpleMode;
    return UiTheme::IndexDetailRenderer::render(idx, *s_theme, options);
}
void MainWindow::buildUi()
{
    setWindowTitle("InvestInsight");
    resize(1200, 860);
    setMinimumSize(960, 680);
    setStyleSheet(UiTheme::buildWidgetStyleSheet(*s_theme));

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_pages = new QStackedWidget(central);
    m_mainPage = new QWidget(m_pages);
    m_pages->addWidget(m_mainPage);
    root->addWidget(m_pages);
    setCentralWidget(central);

    auto *mainLayout = new QVBoxLayout(m_mainPage);
    buildMainPage(mainLayout);

    m_refreshWatcher = new QFutureWatcher<AnalysisResult>(this);
    m_progressPollTimer = new QTimer(this);
    connect(m_progressPollTimer, &QTimer::timeout, this, &MainWindow::pollProgress);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::beginRefresh);
    connect(m_refreshWatcher, &QFutureWatcher<AnalysisResult>::finished, this, &MainWindow::onRefreshFinished);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    connect(m_aiToggle, &QCheckBox::toggled, this, [this](bool checked) {
        m_orchestrator.setAIEnabled(checked);
        m_statusLabel->setText(checked ? "AI 分析已启用" : "AI 分析已关闭");
    });

    loadSavedAIConfigToForm();
    m_pages->setCurrentWidget(m_mainPage);
}

static QHBoxLayout *makeKeyRow(const QString &placeholder, QLineEdit **editOut, QWidget *parent)
{
    auto *row = new QHBoxLayout();
    row->setSpacing(6);
    auto *edit = new QLineEdit(parent);
    edit->setEchoMode(QLineEdit::Password);
    edit->setPlaceholderText(placeholder);
    edit->setMinimumWidth(280);

    auto *eyeBtn = new QToolButton(parent);
    eyeBtn->setCheckable(true);
    eyeBtn->setText("\360\237\221\201");
    eyeBtn->setFixedSize(32, 32);
    eyeBtn->setToolTip("\346\230\276\347\244\272/\351\232\220\350\227\217 Key");
    // uses global QToolButton style
    QObject::connect(eyeBtn, &QToolButton::toggled, edit, [edit](bool on) {
        edit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });

    row->addWidget(edit, 1);
    row->addWidget(eyeBtn);
    if (editOut) *editOut = edit;
    return row;
}

void MainWindow::buildSetupPage(QVBoxLayout *root)
{
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *content = new QWidget(m_setupPage);
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(24, 24, 24, 24);
    cl->setSpacing(18);

    auto *configGrid = new QWidget(content);
    auto *configGridLayout = new QGridLayout(configGrid);
    configGridLayout->setContentsMargins(0, 0, 0, 0);
    configGridLayout->setHorizontalSpacing(18);
    configGridLayout->setVerticalSpacing(18);

    // ──────────────── AI 接入配置 ────────────────
    auto *aiTabWidget = new QWidget(content);
    auto *aiTabLayout = new QVBoxLayout(aiTabWidget);
    aiTabLayout->setContentsMargins(0, 0, 0, 0);
    aiTabLayout->setSpacing(12);

    auto *aiTitle = new QLabel(QString::fromUtf8("AI 接入配置"), aiTabWidget);
    aiTitle->setObjectName("cardTitle");
    aiTabLayout->addWidget(aiTitle);

    auto *aiCard = new QGroupBox(QString::fromUtf8("Key 仅保存在本机，不写入代码"), aiTabWidget);
    auto *af = new QFormLayout(aiCard);
    af->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    af->setSpacing(10);
    af->setContentsMargins(14, 20, 14, 14);

    struct ProvDef { const char *label; const char *hint; QLineEdit **ptr; };
    const QList<ProvDef> defs = {
        {"DeepSeek:",             "sk-...  (deepseek-chat)",                  &m_deepseekKeyEdit},
        {"OpenAI:",               "sk-proj-...  (GPT-4o)",                    &m_openaiKeyEdit},
        {"\351\200\232\344\271\211\345\215\203\351\227\256 Qwen:", "sk-...  (qwen-plus)",                     &m_qwenKeyEdit},
        {"Claude:",               "sk-ant-...  (claude-3-5-haiku-20241022)", &m_claudeKeyEdit},
        {"Gemini:",               "AIza...  (gemini-1.5-flash)",              &m_geminiKeyEdit},
    };
    for (const auto &d : defs) {
        auto *keyRow = makeKeyRow(d.hint, d.ptr, aiCard);
        af->addRow(d.label, keyRow);
    }

    auto *aiSwRow = new QHBoxLayout();
    m_setupAiEnabled = new QCheckBox(QString::fromUtf8("启用 AI 智能分析"), aiCard);
    m_setupAiEnabled->setChecked(true);
    auto *topNLabel = new QLabel(QString::fromUtf8("深度分析板块数 TopN:"), aiCard);
    m_topNSpin = new QSpinBox(aiCard);
    m_topNSpin->setRange(1, 200);
    m_topNSpin->setValue(20);
    m_topNSpin->setFixedWidth(70);
    aiSwRow->addWidget(m_setupAiEnabled);
    aiSwRow->addStretch(1);
    aiSwRow->addWidget(topNLabel);
    aiSwRow->addWidget(m_topNSpin);
    af->addRow("", aiSwRow);

    aiTabLayout->addWidget(aiCard);
    aiTabLayout->addStretch(1);

    // ──────────────── 我的持仓 ────────────────
    auto *pfTabWidget = new QWidget(content);
    auto *pfTabLayout = new QVBoxLayout(pfTabWidget);
    pfTabLayout->setContentsMargins(0, 0, 0, 0);
    pfTabLayout->setSpacing(10);

    auto *pfTitle = new QLabel(QString::fromUtf8("我的持仓"), pfTabWidget);
    pfTitle->setObjectName("cardTitle");
    pfTabLayout->addWidget(pfTitle);

    auto *pfCard = new QGroupBox(QString::fromUtf8("可选 · 用于个性化操作建议"), pfTabWidget);
    auto *pfl = new QVBoxLayout(pfCard);
    pfl->setSpacing(8);
    pfl->setContentsMargins(14, 20, 14, 14);

    auto *pfDesc = new QLabel(
        QString::fromUtf8("填写持仓板块、买入批次及金额后，分析结果将生成针对您持仓的操作建议。"
                          "\n支持多批次买入记录，金额单位为元（¥）。"),
        pfCard);
    pfDesc->setWordWrap(true);
    {
        QFont f = pfDesc->font();
        f.setPointSizeF(f.pointSizeF() * 0.9);
        pfDesc->setFont(f);
        QPalette pal = pfDesc->palette();
        pal.setColor(QPalette::WindowText, QColor(s_theme->mutedColor));
        pfDesc->setPalette(pal);
    }
    pfl->addWidget(pfDesc);

    // 表格：[✓][板块名称][类型][买入日期][首次收益日][投入金额（元）][已卖出]
    m_portfolioTable = new QTableWidget(0, 7, pfCard);
    m_portfolioTable->setHorizontalHeaderLabels({
        "", QString::fromUtf8("板块名称"), QString::fromUtf8("类型"),
        QString::fromUtf8("买入日期"), QString::fromUtf8("首次收益日"),
        QString::fromUtf8("投入金额（元）"),
        QString::fromUtf8("已卖出")});
    auto *ph = m_portfolioTable->horizontalHeader();
    ph->setSectionResizeMode(0, QHeaderView::Fixed);
    ph->setSectionResizeMode(1, QHeaderView::Stretch);
    ph->setSectionResizeMode(2, QHeaderView::Fixed);
    ph->setSectionResizeMode(3, QHeaderView::Fixed);
    ph->setSectionResizeMode(4, QHeaderView::Fixed);
    ph->setSectionResizeMode(5, QHeaderView::Fixed);
    ph->setSectionResizeMode(6, QHeaderView::Fixed);
    m_portfolioTable->setColumnWidth(0, 44);
    m_portfolioTable->setColumnWidth(2, 130);
    m_portfolioTable->setColumnWidth(3, 148);
    m_portfolioTable->setColumnWidth(4, 148);
    m_portfolioTable->setColumnWidth(5, 130);
    m_portfolioTable->setColumnWidth(6, 60);
    m_portfolioTable->verticalHeader()->setVisible(false);
    m_portfolioTable->verticalHeader()->setDefaultSectionSize(36);
    m_portfolioTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_portfolioTable->setEditTriggers(
        QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed
        | QAbstractItemView::AnyKeyPressed);
    m_portfolioTable->setFocusPolicy(Qt::ClickFocus);
    m_portfolioTable->setShowGrid(false);
    m_portfolioTable->setMinimumHeight(200);
    pfl->addWidget(m_portfolioTable, 1);

    auto *pfBtnRow = new QHBoxLayout();
    auto *addRowBtn = new QPushButton(QString::fromUtf8("+ 添加批次"), pfCard);
    addRowBtn->setObjectName("secondaryBtn");
    auto *removeRowBtn = new QPushButton(QString::fromUtf8("× 删除勾选行"), pfCard);
    removeRowBtn->setObjectName("dangerBtn");
    pfBtnRow->addWidget(addRowBtn);
    pfBtnRow->addWidget(removeRowBtn);
    pfBtnRow->addStretch(1);
    pfl->addLayout(pfBtnRow);

    // 构建一行持仓数据的通用辅助
    // 类型枚举：股票 / ETF / 基金
    // 计算首次收益日：基金 T+1 工作日，股票/ETF 当日
    auto calcFirstEarnDate = [](const QString &buyDateStr, const QString &type) -> QString {
        QDate d = QDate::fromString(buyDateStr, "yyyy-MM-dd");
        if (!d.isValid()) return buyDateStr;
        if (type == QString::fromUtf8("基金")) {
            d = d.addDays(1);
            while (d.dayOfWeek() > 5) d = d.addDays(1); // 跳过周末
        }
        return d.toString("yyyy-MM-dd");
    };

    auto makePortfolioRow = [this, calcFirstEarnDate](int r, const QString &sector, const QString &holdType,
                                   const QString &date, double amount, bool sold = false) {
        // col 0: 删除勾选框
        auto *checkCell = new QWidget(m_portfolioTable);
        checkCell->setAutoFillBackground(false);
        auto *checkLay = new QHBoxLayout(checkCell);
        checkLay->setContentsMargins(0, 0, 0, 0);
        checkLay->setAlignment(Qt::AlignCenter);
        auto *cb = new QCheckBox(checkCell);
        cb->setChecked(false);
        cb->setFocusPolicy(Qt::NoFocus);
        checkLay->addWidget(cb);
        m_portfolioTable->setCellWidget(r, 0, checkCell);

        // col 1: 板块下拉（支持输入实时过滤，MatchContains）
        auto *combo = new QComboBox(m_portfolioTable);
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->setFocusPolicy(Qt::ClickFocus);
        combo->lineEdit()->setPlaceholderText(QString::fromUtf8("输入关键字筛选板块..."));
        QSettings s("InvestInsight", "InvestInsight");
        const QStringList sectors = s.value("cache/sector_names").toStringList();
        combo->addItems(sectors);
        // 输入过滤：仅显示包含输入文字的选项（MatchContains + 不区分大小写）
        auto *completer = new QCompleter(sectors, combo);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        combo->setCompleter(completer);
        if (sector.isEmpty()) combo->setCurrentIndex(-1);
        else combo->setCurrentText(sector);
        m_portfolioTable->setCellWidget(r, 1, combo);

        // col 2: 持仓类型 — 与板块名称列完全相同的 editable cellWidget 方式
        const QStringList typeOpts = {QString::fromUtf8("股票"),
                                      QString::fromUtf8("ETF"),
                                      QString::fromUtf8("基金")};
        auto *typeCombo = new QComboBox(m_portfolioTable);
        typeCombo->addItems(typeOpts);
        typeCombo->setEditable(true);
        typeCombo->setInsertPolicy(QComboBox::NoInsert);
        typeCombo->setFocusPolicy(Qt::ClickFocus);
        // 限制只能从固定选项中选，不允许随意输入
        auto *typeCompleter = new QCompleter(typeOpts, typeCombo);
        typeCompleter->setFilterMode(Qt::MatchContains);
        typeCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        typeCompleter->setCompletionMode(QCompleter::PopupCompletion);
        typeCombo->setCompleter(typeCompleter);
        const QString typeVal = holdType.isEmpty() ? QString::fromUtf8("基金") : holdType;
        typeCombo->setCurrentText(typeVal);
        // 去掉内嵌 QLineEdit 从全局 stylesheet 继承的 12px padding，释放可用空间
        typeCombo->lineEdit()->setTextMargins(0, 0, 0, 0);
        m_portfolioTable->setCellWidget(r, 2, typeCombo);

        // col 3: 买入日期（QDateEdit：可弹日历+可手输）
        const QDate buyDate = QDate::fromString(date, "yyyy-MM-dd").isValid()
            ? QDate::fromString(date, "yyyy-MM-dd") : QDate::currentDate();
        auto *buyDateEdit = new QDateEdit(m_portfolioTable);
        buyDateEdit->setDisplayFormat("yyyy-MM-dd");
        buyDateEdit->setCalendarPopup(true);
        buyDateEdit->setDate(buyDate);
        buyDateEdit->setMinimumWidth(136);
        m_portfolioTable->setCellWidget(r, 3, buyDateEdit);
        if (auto *le = buyDateEdit->findChild<QLineEdit *>()) {
            le->setTextMargins(0, 0, 0, 0);
            QObject::connect(le, &QLineEdit::editingFinished, buyDateEdit, [buyDateEdit, le]() {
                QString s = le->text().trimmed();
                if (s.isEmpty()) { le->setText(buyDateEdit->date().toString("yyyy-MM-dd")); return; }
                s.replace('/', '-');
                s.replace('.', '-');
                s.replace(QString::fromUtf8("年"), "-");
                s.replace(QString::fromUtf8("月"), "-");
                s.replace(QString::fromUtf8("日"), "");
                s.remove(' ');
                s.remove(QChar(0x3000));
                while (s.contains("--")) s.replace("--", "-");
                const QStringList p = s.split('-', Qt::SkipEmptyParts);
                QDate d;
                if (p.size() == 3) d = QDate(p[0].toInt(), p[1].toInt(), p[2].toInt());
                if (!d.isValid()) d = QDate::fromString(s, "yyyy-MM-dd");
                if (d.isValid()) buyDateEdit->setDate(d);
                le->setText(buyDateEdit->date().toString("yyyy-MM-dd"));
            });
        }

        // col 4: 首次收益日（默认自动计算，可手动改）
        const QDate autoEarnDate = QDate::fromString(
            calcFirstEarnDate(buyDate.toString("yyyy-MM-dd"), typeVal), "yyyy-MM-dd");
        auto *earnDateEdit = new QDateEdit(m_portfolioTable);
        earnDateEdit->setDisplayFormat("yyyy-MM-dd");
        earnDateEdit->setCalendarPopup(true);
        earnDateEdit->setDate(autoEarnDate.isValid() ? autoEarnDate : buyDate);
        earnDateEdit->setMinimumWidth(136);
        earnDateEdit->setProperty("manualOverride", false);
        m_portfolioTable->setCellWidget(r, 4, earnDateEdit);
        if (auto *le = earnDateEdit->findChild<QLineEdit *>()) {
            le->setTextMargins(0, 0, 0, 0);
            QObject::connect(le, &QLineEdit::editingFinished, earnDateEdit, [earnDateEdit, le]() {
                QString s = le->text().trimmed();
                if (s.isEmpty()) { le->setText(earnDateEdit->date().toString("yyyy-MM-dd")); return; }
                s.replace('/', '-');
                s.replace('.', '-');
                s.replace(QString::fromUtf8("年"), "-");
                s.replace(QString::fromUtf8("月"), "-");
                s.replace(QString::fromUtf8("日"), "");
                s.remove(' ');
                s.remove(QChar(0x3000));
                while (s.contains("--")) s.replace("--", "-");
                const QStringList p = s.split('-', Qt::SkipEmptyParts);
                QDate d;
                if (p.size() == 3) d = QDate(p[0].toInt(), p[1].toInt(), p[2].toInt());
                if (!d.isValid()) d = QDate::fromString(s, "yyyy-MM-dd");
                if (d.isValid()) earnDateEdit->setDate(d);
                le->setText(earnDateEdit->date().toString("yyyy-MM-dd"));
            });
        }

        // 当类型或买入日期变化时自动更新首次收益日
        QObject::connect(typeCombo, &QComboBox::currentTextChanged,
            m_portfolioTable, [this, r, calcFirstEarnDate](const QString &newType) {
                auto *bd = qobject_cast<QDateEdit *>(m_portfolioTable->cellWidget(r, 3));
                auto *ed = qobject_cast<QDateEdit *>(m_portfolioTable->cellWidget(r, 4));
                if (!bd || !ed) return;
                if (ed->property("manualOverride").toBool()) return; // 手动覆盖后不自动改写
                const QString autoDate = calcFirstEarnDate(bd->date().toString("yyyy-MM-dd"), newType);
                const QDate d = QDate::fromString(autoDate, "yyyy-MM-dd");
                if (!d.isValid()) return;
                const QSignalBlocker blocker(ed);
                ed->setDate(d);
                ed->setProperty("manualOverride", false);
            });

        // 买入日期变化：若未手动覆盖首次收益日，则自动刷新
        QObject::connect(buyDateEdit, &QDateEdit::dateChanged,
            m_portfolioTable, [calcFirstEarnDate, typeCombo, earnDateEdit](const QDate &newBuyDate) {
                if (!earnDateEdit || !typeCombo) return;
                if (earnDateEdit->property("manualOverride").toBool()) return;
                const QString autoDate = calcFirstEarnDate(newBuyDate.toString("yyyy-MM-dd"), typeCombo->currentText());
                const QDate d = QDate::fromString(autoDate, "yyyy-MM-dd");
                if (!d.isValid()) return;
                const QSignalBlocker blocker(earnDateEdit);
                earnDateEdit->setDate(d);
                earnDateEdit->setProperty("manualOverride", false);
            });

        // 首次收益日变化：标记是否为手动覆盖（与自动值不同即手动）
        QObject::connect(earnDateEdit, &QDateEdit::dateChanged,
            m_portfolioTable, [calcFirstEarnDate, typeCombo, buyDateEdit, earnDateEdit](const QDate &newEarnDate) {
                if (!earnDateEdit || !typeCombo || !buyDateEdit) return;
                const QString autoDate = calcFirstEarnDate(buyDateEdit->date().toString("yyyy-MM-dd"), typeCombo->currentText());
                const QDate autoD = QDate::fromString(autoDate, "yyyy-MM-dd");
                if (!autoD.isValid()) return;
                earnDateEdit->setProperty("manualOverride", newEarnDate != autoD);
            });

        // col 5: 金额（元），右对齐
        auto *amtItem = new QTableWidgetItem(
            amount > 0 ? QString::number(amount, 'f', 0) : QString("0"));
        amtItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_portfolioTable->setItem(r, 5, amtItem);

        // col 6: 已卖出 checkbox
        auto *soldCell = new QWidget(m_portfolioTable);
        soldCell->setAutoFillBackground(false);
        auto *soldLay = new QHBoxLayout(soldCell);
        soldLay->setContentsMargins(0, 0, 0, 0);
        soldLay->setAlignment(Qt::AlignCenter);
        auto *soldCb = new QCheckBox(soldCell);
        soldCb->setChecked(sold);
        soldCb->setFocusPolicy(Qt::NoFocus);
        soldLay->addWidget(soldCb);
        m_portfolioTable->setCellWidget(r, 6, soldCell);
    };

    // 已卖出行自动移到最下面：当 sold checkbox 被勾选时重排表格
    m_reorderSoldFn = std::make_shared<std::function<void()>>();
    auto reorderSold = m_reorderSoldFn;
    *reorderSold = [this, makePortfolioRow, reorderSold]() {
        struct RowData { QString sector, holdType, date, firstEarnDate; double amount; bool sold; bool earnManual; };
        QList<RowData> rows;
        for (int i = 0; i < m_portfolioTable->rowCount(); ++i) {
            RowData rd{};
            if (auto *c = qobject_cast<QComboBox*>(m_portfolioTable->cellWidget(i, 1)))
                rd.sector = c->currentText();
            if (auto *c = qobject_cast<QComboBox*>(m_portfolioTable->cellWidget(i, 2)))
                rd.holdType = c->currentText();
            if (auto *c = qobject_cast<QDateEdit*>(m_portfolioTable->cellWidget(i, 3)))
                rd.date = c->date().toString("yyyy-MM-dd");
            if (auto *c = qobject_cast<QDateEdit*>(m_portfolioTable->cellWidget(i, 4))) {
                rd.firstEarnDate = c->date().toString("yyyy-MM-dd");
                rd.earnManual = c->property("manualOverride").toBool();
            }
            rd.amount = m_portfolioTable->item(i, 5) ? m_portfolioTable->item(i, 5)->text().toDouble() : 0;
            rd.sold = false;
            if (auto *w = m_portfolioTable->cellWidget(i, 6)) {
                if (auto *cb = w->findChild<QCheckBox*>()) rd.sold = cb->isChecked();
            }
            rows.append(rd);
        }
        std::stable_sort(rows.begin(), rows.end(), [](const RowData &a, const RowData &b) {
            return !a.sold && b.sold;
        });
        m_portfolioTable->setRowCount(0);
        for (int i = 0; i < rows.size(); ++i) {
            const RowData &rd = rows[i];
            m_portfolioTable->insertRow(i);
            makePortfolioRow(i, rd.sector, rd.holdType, rd.date, rd.amount, rd.sold);
            if (auto *w = m_portfolioTable->cellWidget(i, 6)) {
                if (auto *cb = w->findChild<QCheckBox*>()) {
                    QObject::connect(cb, &QCheckBox::toggled, m_portfolioTable,
                        [reorderSold](bool) { QTimer::singleShot(0, [reorderSold]() { (*reorderSold)(); }); });
                }
            }
        }
    };

    auto connectSoldCb = [this, reorderSold](int row) {
        if (auto *w = m_portfolioTable->cellWidget(row, 6)) {
            if (auto *cb = w->findChild<QCheckBox*>()) {
                QObject::connect(cb, &QCheckBox::toggled, m_portfolioTable,
                    [reorderSold](bool) { QTimer::singleShot(0, [reorderSold]() { (*reorderSold)(); }); });
            }
        }
    };

    connect(addRowBtn, &QPushButton::clicked, this, [this, makePortfolioRow, connectSoldCb]() {
        const int r = m_portfolioTable->rowCount();
        m_portfolioTable->insertRow(r);
        makePortfolioRow(r, QString(), QString::fromUtf8("基金"), QString(), 0.0, false);
        connectSoldCb(r);
    });
    connect(removeRowBtn, &QPushButton::clicked, this, [this]() {
        for (int i = m_portfolioTable->rowCount() - 1; i >= 0; --i) {
            if (auto *w = m_portfolioTable->cellWidget(i, 0)) {
                auto *cb = w->findChild<QCheckBox *>();
                if (cb && cb->isChecked())
                    m_portfolioTable->removeRow(i);
            }
        }
    });

    pfTabLayout->addWidget(pfCard, 1);

    m_enterButton = new QPushButton(QString::fromUtf8("保存配置"), content);
    m_enterButton->setFixedHeight(48);
    m_enterButton->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #4338CA,stop:1 #6366F1);color:#fff;border:none;border-radius:12px;"
        "font-size:15px;font-weight:700;letter-spacing:0.5px;}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #4F46E5,stop:1 #818CF8);}"
        "QPushButton:pressed{background:#3730A3;}");
    connect(m_enterButton, &QPushButton::clicked, this, &MainWindow::saveAndEnterMainPage);

    auto *opsGrid = new QWidget(content);
    auto *opsLayout = new QHBoxLayout(opsGrid);
    opsLayout->setContentsMargins(0, 0, 0, 0);
    opsLayout->setSpacing(18);

    auto makeConfigCard = [content](const QString &title, const QStringList &rows) {
        auto *card = new QFrame(content);
        card->setObjectName("configCard");
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(26, 22, 26, 22);
        layout->setSpacing(12);
        auto *titleLabel = new QLabel(title, card);
        titleLabel->setObjectName("cardTitle");
        layout->addWidget(titleLabel);
        for (const QString &row : rows) {
            auto *label = new QLabel(row, card);
            label->setObjectName("cardMeta");
            label->setWordWrap(true);
            layout->addWidget(label);
        }
        layout->addStretch(1);
        return card;
    };
    opsLayout->addWidget(makeConfigCard(QString::fromUtf8("后台刷新与提醒"), {
        QString::fromUtf8("盘中增量扫描：当前由手动刷新触发，后续可扩展为 15 分钟定时扫描。"),
        QString::fromUtf8("关注板块优先：半导体、有色金属、锂电池等持仓或关注板块优先展示。"),
        QString::fromUtf8("提醒阈值：事件催化 > 0.65 或过热风险 > 0.70 时进入观察队列。")
    }));
    opsLayout->addWidget(makeConfigCard(QString::fromUtf8("数据源健康"), {
        QString::fromUtf8("同花顺实时行情：用于今日涨幅最高优先级口径。"),
        QString::fromUtf8("同花顺 K 线 / 东方财富板块 / 新浪资金流：用于图表、资金和兜底验证。"),
        QString::fromUtf8("多源财经新闻：用于事件雷达、影响路径和 AI 复盘。")
    }));

    configGridLayout->addWidget(aiTabWidget, 0, 0);
    configGridLayout->addWidget(pfTabWidget, 0, 1);
    configGridLayout->setColumnStretch(0, 1);
    configGridLayout->setColumnStretch(1, 1);

    cl->addWidget(configGrid, 1);
    cl->addWidget(opsGrid);
    cl->addSpacing(12);
    cl->addWidget(m_enterButton);

    root->addWidget(content, 1);
}

void MainWindow::buildMainPage(QVBoxLayout *mainLayout)
{
    auto *shell = new QFrame(m_mainPage);
    shell->setObjectName("mainShell");
    auto *shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto *sideNav = new QFrame(shell);
    sideNav->setObjectName("sideNav");
    sideNav->setFixedWidth(236);
    auto *sideLayout = new QVBoxLayout(sideNav);
    sideLayout->setContentsMargins(18, 36, 18, 24);
    sideLayout->setSpacing(8);

    auto *brandTitle = new QLabel("InvestInsight", sideNav);
    brandTitle->setObjectName("brandTitle");
    auto *brandSub = new QLabel(QString::fromUtf8("新闻驱动的板块洞察"), sideNav);
    brandSub->setObjectName("brandSubtitle");
    sideLayout->addWidget(brandTitle);
    sideLayout->addWidget(brandSub);
    sideLayout->addSpacing(18);

    auto *navGroup = new QButtonGroup(sideNav);
    navGroup->setExclusive(true);
    auto makeNavButton = [sideNav, navGroup, sideLayout](const QString &text, int id) {
        auto *button = new QPushButton(text, sideNav);
        button->setObjectName("navButton");
        button->setCheckable(true);
        button->setMinimumHeight(46);
        navGroup->addButton(button, id);
        sideLayout->addWidget(button);
        return button;
    };
    QPushButton *navOverview = makeNavButton(QString::fromUtf8("总览"), 0);
    QPushButton *navEvents = makeNavButton(QString::fromUtf8("事件雷达"), 1);
    QPushButton *navSectors = makeNavButton(QString::fromUtf8("板块机会"), 2);
    QPushButton *navStrategy = makeNavButton(QString::fromUtf8("策略跟踪"), 3);
    QPushButton *navChat = makeNavButton(QString::fromUtf8("AI 助手"), 4);
    QPushButton *navConfig = makeNavButton(QString::fromUtf8("配置"), 5);
    navOverview->setChecked(true);

    sideLayout->addStretch(1);
    auto *sideStatus = new QFrame(sideNav);
    sideStatus->setObjectName("sideStatusCard");
    auto *sideStatusLayout = new QVBoxLayout(sideStatus);
    sideStatusLayout->setContentsMargins(18, 14, 18, 14);
    sideStatusLayout->setSpacing(6);
    auto *dataState = new QLabel(QString::fromUtf8("数据状态"), sideStatus);
    dataState->setStyleSheet("color:#E2E8F0;font-size:15px;font-weight:800;");
    auto *quoteState = new QLabel(QString::fromUtf8("● 行情等待刷新"), sideStatus);
    quoteState->setStyleSheet("color:#86EFAC;font-size:12px;");
    auto *newsState = new QLabel(QString::fromUtf8("● 新闻等待扫描"), sideStatus);
    newsState->setStyleSheet("color:#FDE68A;font-size:12px;");
    sideStatusLayout->addWidget(dataState);
    sideStatusLayout->addWidget(quoteState);
    sideStatusLayout->addWidget(newsState);
    sideLayout->addWidget(sideStatus);

    auto *workspace = new QFrame(shell);
    workspace->setObjectName("workspacePane");
    auto *workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);

    auto *topBarW = new QFrame(workspace);
    topBarW->setObjectName("topStatusBar");
    auto *topBar = new QHBoxLayout(topBarW);
    topBar->setContentsMargins(30, 18, 30, 16);
    topBar->setSpacing(12);
    m_workspaceTitle = new QLabel(QString::fromUtf8("综合总览"), topBarW);
    m_workspaceTitle->setObjectName("workspaceTitle");
    m_workspaceMeta = new QLabel(QString::fromUtf8("等待分析 · AI 与规则引擎就绪"), topBarW);
    m_workspaceMeta->setObjectName("statusLabel");

    m_statusLabel = new QLabel("就绪", m_mainPage);
    m_statusLabel->setObjectName("statusLabel");
    m_loadingBar = new QProgressBar(m_mainPage);
    m_loadingBar->setFixedHeight(5);
    m_loadingBar->setVisible(false);
    m_loadingBar->setTextVisible(false);

    topBar->addWidget(m_workspaceTitle);
    topBar->addWidget(m_workspaceMeta);
    topBar->addStretch(1);
    topBar->addWidget(m_statusLabel);

    m_tabWidget = new ScrollableTabWidget(workspace);
    m_tabWidget->setUsesScrollButtons(true);
    m_tabWidget->setElideMode(Qt::ElideNone);
    m_tabWidget->setTabsClosable(false);
    m_tabWidget->tabBar()->setExpanding(false);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->tabBar()->setStyle(new PaddedTabStyle(m_tabWidget->tabBar()->style()));
    m_tabWidget->tabBar()->hide();

    // ---- 总览工作台：4个子Tab（总览 / 事件雷达 / 板块机会 / 策略跟踪）----
    auto *overviewContainer = new QWidget(workspace);
    auto *overviewContainerLayout = new QVBoxLayout(overviewContainer);
    overviewContainerLayout->setContentsMargins(0, 0, 0, 0);
    overviewContainerLayout->setSpacing(0);

    m_overviewSubTabs = new QTabWidget(overviewContainer);
    m_overviewSubTabs->setDocumentMode(true);
    m_overviewSubTabs->setElideMode(Qt::ElideNone);
    m_overviewSubTabs->tabBar()->setExpanding(false);
    m_overviewSubTabs->tabBar()->setStyle(new PaddedTabStyle(m_overviewSubTabs->tabBar()->style()));

    // -- 子页 1: 总览 --
    auto *dashboardPage = new QWidget(m_overviewSubTabs);
    auto *dashboardLayout = new QVBoxLayout(dashboardPage);
    dashboardLayout->setContentsMargins(22, 18, 22, 22);
    dashboardLayout->setSpacing(14);

    auto *analysisCard = new QFrame(dashboardPage);
    analysisCard->setObjectName("configCard");
    auto *analysisLayout = new QHBoxLayout(analysisCard);
    analysisLayout->setContentsMargins(26, 18, 26, 18);
    analysisLayout->setSpacing(18);
    auto *analysisText = new QWidget(analysisCard);
    auto *analysisTextLayout = new QVBoxLayout(analysisText);
    analysisTextLayout->setContentsMargins(0, 0, 0, 0);
    analysisTextLayout->setSpacing(4);
    auto *analysisTitle = new QLabel(QString::fromUtf8("分析控制"), analysisText);
    analysisTitle->setObjectName("cardTitle");
    auto *analysisMeta = new QLabel(QString::fromUtf8("点击后刷新行情、新闻、事件雷达和策略跟踪。"), analysisText);
    analysisMeta->setObjectName("cardMeta");
    analysisTextLayout->addWidget(analysisTitle);
    analysisTextLayout->addWidget(analysisMeta);

    m_refreshButton = new QPushButton(QString::fromUtf8("▶  开始分析"), analysisCard);
    m_refreshButton->setObjectName("refreshBtn");
    m_refreshButton->setMinimumWidth(142);
    m_aiToggle = new QCheckBox(QString::fromUtf8("AI 深度分析"), analysisCard);
    m_aiToggle->setChecked(m_orchestrator.isAIEnabled() && m_orchestrator.isAIAvailable());
    m_aiToggle->setEnabled(m_orchestrator.isAIAvailable());
    m_aiToggle->setToolTip(m_orchestrator.isAIAvailable()
        ? QString::fromUtf8("启用/关闭 AI 智能分析")
        : QString::fromUtf8("未配置 AI API Key"));
    analysisLayout->addWidget(analysisText, 1);
    analysisLayout->addWidget(m_refreshButton);
    analysisLayout->addWidget(m_aiToggle);

    m_dashboardBrowser = new ClickableBrowser(dashboardPage);
    m_dashboardBrowser->onTabJump = [this](int idx) {
        if (idx <= 0 || idx > static_cast<int>(m_lastResult.sectors.size())) return;
        const QString &name = m_lastResult.sectors[idx - 1].industry;
        openSectorTab(name);
    };
    m_dashboardBrowser->onIndexJump = [this](const QString &key) {
        openIndexTab(key);
    };
    dashboardLayout->addWidget(analysisCard);
    dashboardLayout->addWidget(m_dashboardBrowser, 1);
    m_overviewSubTabs->addTab(dashboardPage, "总览");

    // -- 子Tab 2: 事件雷达 --
    m_eventRadarBrowser = new ClickableBrowser(m_overviewSubTabs);
    m_eventRadarBrowser->onTabJump = [this](int idx) {
        if (idx <= 0 || idx > static_cast<int>(m_lastResult.sectors.size())) return;
        const QString &name = m_lastResult.sectors[idx - 1].industry;
        openSectorTab(name);
    };
    m_eventRadarBrowser->onIndexJump = [this](const QString &key) {
        openIndexTab(key);
    };
    m_overviewSubTabs->addTab(m_eventRadarBrowser, "事件雷达");

    // -- 子Tab 3: 板块机会（含过滤栏）--
    auto *sectorTab = new QWidget(m_overviewSubTabs);
    auto *sectorLayout = new QVBoxLayout(sectorTab);
    sectorLayout->setContentsMargins(0, 6, 0, 0);
    sectorLayout->setSpacing(4);

    auto *filterBar = new QWidget(sectorTab);
    auto *filterRow = new QHBoxLayout(filterBar);
    filterRow->setContentsMargins(10, 6, 10, 8);
    filterRow->setSpacing(8);

    m_overviewSearch = new QLineEdit(filterBar);
    m_overviewSearch->setPlaceholderText("搜索板块、指数或机会...");
    m_overviewSearch->setMinimumWidth(140);
    m_overviewSearch->setMaximumWidth(200);
    m_overviewSearch->setClearButtonEnabled(true);

    m_trendFilter = new QComboBox(filterBar);
    m_trendFilter->addItems({"趋势：全部", "强势看多", "偏多", "横盘震荡", "方向不明", "偏空", "强势看空"});
    m_trendFilter->setMinimumWidth(110);

    m_actionFilter = new QComboBox(filterBar);
    m_actionFilter->addItems({"建议：全部", "增配", "持有", "减配"});
    m_actionFilter->setMinimumWidth(90);

    m_sortBy = new QComboBox(filterBar);
    m_sortBy->addItems({"排序：热度", "预测评分↓", "预测评分↑", "今日涨跌↓", "今日涨跌↑", "置信度↓", "数据质量↓", "一致性↓"});
    m_sortBy->setMinimumWidth(108);

    m_viewMode = new QComboBox(filterBar);
    m_viewMode->addItems({"简明模式", "专业模式"});
    m_viewMode->setMinimumWidth(90);
    m_viewMode->setToolTip("简明模式适合快速查看关键信号；专业模式展示完整投研数据");

    auto *resetBtn = new QPushButton("重置", filterBar);
    resetBtn->setObjectName("secondaryBtn");
    resetBtn->setFixedWidth(60);
    resetBtn->setFixedHeight(30);

    filterRow->addWidget(m_viewMode);
    filterRow->addWidget(m_overviewSearch);
    filterRow->addWidget(m_trendFilter);
    filterRow->addWidget(m_actionFilter);
    filterRow->addWidget(m_sortBy);
    filterRow->addWidget(resetBtn);
    filterRow->addStretch(1);

    m_overviewBrowser = new ClickableBrowser(sectorTab);
    m_overviewBrowser->onTabJump = [this](int idx) {
        if (idx <= 0 || idx > static_cast<int>(m_lastResult.sectors.size())) return;
        const QString &name = m_lastResult.sectors[idx - 1].industry;
        openSectorTab(name);
    };
    m_overviewBrowser->onIndexJump = [this](const QString &key) {
        openIndexTab(key);
    };

    sectorLayout->addWidget(filterBar);
    sectorLayout->addWidget(m_overviewBrowser, 1);
    m_overviewSubTabs->addTab(sectorTab, "板块机会");

    // -- 子Tab 4: 策略跟踪 --
    m_strategyBrowser = new ClickableBrowser(m_overviewSubTabs);
    m_strategyBrowser->onTabJump = [this](int idx) {
        if (idx <= 0 || idx > static_cast<int>(m_lastResult.sectors.size())) return;
        const QString &name = m_lastResult.sectors[idx - 1].industry;
        openSectorTab(name);
    };
    m_strategyBrowser->onIndexJump = [this](const QString &key) {
        openIndexTab(key);
    };
    m_overviewSubTabs->addTab(m_strategyBrowser, "策略跟踪");

    overviewContainerLayout->addWidget(m_overviewSubTabs, 1);
    m_tabWidget->addTab(overviewContainer, "总览工作台");
    m_overviewSubTabs->tabBar()->hide();

    m_setupPage = new QWidget(m_tabWidget);
    auto *configLayout = new QVBoxLayout(m_setupPage);
    buildSetupPage(configLayout);
    m_configTabIndex = m_tabWidget->addTab(m_setupPage, QString::fromUtf8("配置"));

    // 过滤控件全部触发重新渲染（仅影响板块&指数信息子Tab）
    auto rerender = [this]() {
        if (!m_lastResult.sectors.isEmpty() || m_lastResult.marketCtx.valid) renderOverview(m_lastResult);
    };
    connect(m_overviewSearch, &QLineEdit::textChanged, this, rerender);
    connect(m_trendFilter,  qOverload<int>(&QComboBox::currentIndexChanged), this, rerender);
    connect(m_actionFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, rerender);
    connect(m_sortBy,       qOverload<int>(&QComboBox::currentIndexChanged), this, rerender);
    connect(m_viewMode,     qOverload<int>(&QComboBox::currentIndexChanged), this, rerender);
    connect(resetBtn, &QPushButton::clicked, this, [this, rerender]() {
        m_overviewSearch->clear();
        m_trendFilter->setCurrentIndex(0);
        m_actionFilter->setCurrentIndex(0);
        m_sortBy->setCurrentIndex(0);
        rerender();
    });
    auto setWorkspaceHeader = [this](const QString &title, const QString &meta) {
        if (m_workspaceTitle) m_workspaceTitle->setText(title);
        if (m_workspaceMeta) m_workspaceMeta->setText(meta);
    };
    auto setOverviewHeader = [setWorkspaceHeader](int index) {
        const QStringList titles = {
            QString::fromUtf8("综合总览"),
            QString::fromUtf8("事件雷达"),
            QString::fromUtf8("板块机会"),
            QString::fromUtf8("策略跟踪")
        };
        const QStringList metas = {
            QString::fromUtf8("市场温度、关键事件、机会风险和分析控制"),
            QString::fromUtf8("事件状态、时间节点、传导路径和影响板块"),
            QString::fromUtf8("板块评分、事件催化、趋势状态和风险排序"),
            QString::fromUtf8("建议动作、持仓影响、信号表现和后续检查点")
        };
        if (index >= 0 && index < titles.size()) {
            setWorkspaceHeader(titles[index], metas[index]);
        }
    };
    connect(navGroup, qOverload<int>(&QButtonGroup::idClicked), this, [this, setOverviewHeader, setWorkspaceHeader](int id) {
        if (id >= 0 && id <= 3 && m_overviewSubTabs) {
            m_tabWidget->setCurrentIndex(0);
            m_overviewSubTabs->setCurrentIndex(id);
            setOverviewHeader(id);
        } else if (id == 4) {
            openChatTab();
        } else if (id == 5) {
            loadSavedAIConfigToForm();
            if (m_configTabIndex >= 0) {
                m_tabWidget->setCurrentIndex(m_configTabIndex);
            }
            setWorkspaceHeader(QString::fromUtf8("配置"),
                QString::fromUtf8("AI 接入、持仓、数据刷新和提醒设置"));
        }
    });
    const QList<QPushButton *> navButtons = {navOverview, navEvents, navSectors, navStrategy};
    connect(m_overviewSubTabs, &QTabWidget::currentChanged, this, [navButtons, setOverviewHeader](int index) {
        if (index >= 0 && index < navButtons.size()) {
            navButtons[index]->setChecked(true);
            setOverviewHeader(index);
        }
    });

    workspaceLayout->addWidget(topBarW);
    workspaceLayout->addWidget(m_loadingBar);
    workspaceLayout->addWidget(m_tabWidget, 1);

    shellLayout->addWidget(sideNav);
    shellLayout->addWidget(workspace, 1);

    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(shell, 1);
}

QList<AIProvider> MainWindow::collectProvidersFromForm() const
{
    struct ProvSpec { QString name, apiUrl, model; QLineEdit *edit; };
    const QList<ProvSpec> specs = {
        {"DeepSeek", "https://api.deepseek.com/chat/completions",
         "deepseek-chat", m_deepseekKeyEdit},
        {"GPT-4o", "https://api.openai.com/v1/chat/completions",
         "gpt-4o", m_openaiKeyEdit},
        {"Qwen", "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions",
         "qwen-plus", m_qwenKeyEdit},
        {"Claude", "https://api.anthropic.com/v1/messages",
         "claude-3-5-haiku-20241022", m_claudeKeyEdit},
        {"Gemini", "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions",
         "gemini-1.5-flash", m_geminiKeyEdit},
    };
    QList<AIProvider> providers;
    for (const ProvSpec &s : specs) {
        const QString key = s.edit->text().trimmed();
        if (key.isEmpty()) continue;
        AIProvider p;
        p.name   = s.name;
        p.apiUrl = s.apiUrl;
        p.apiKey = key;
        p.model  = s.model;
        providers.push_back(p);
    }
    return providers;
}

QMap<QString, double> MainWindow::collectPortfolioFromTable() const
{
    QMap<QString, double> portfolio;
    for (int r = 0; r < m_portfolioTable->rowCount(); ++r) {
        // 跳过已卖出的
        if (auto *w = m_portfolioTable->cellWidget(r, 6)) {
            if (auto *scb = w->findChild<QCheckBox *>()) {
                if (scb->isChecked()) continue;
            }
        }
        QString name;
        if (auto *combo = qobject_cast<QComboBox *>(m_portfolioTable->cellWidget(r, 1)))
            name = combo->currentText().trimmed();
        if (name.isEmpty()) continue;
        const QTableWidgetItem *ai = m_portfolioTable->item(r, 5);
        const double amt = ai ? ai->text().toDouble() : 0.0;
        portfolio[name] += amt;
    }
    return portfolio;
}

void MainWindow::loadSavedAIConfigToForm()
{
    QSettings settings("InvestInsight", "InvestInsight");
    const QString providersJson = settings.value("ai/providers_json").toString();
    const QJsonDocument doc = QJsonDocument::fromJson(providersJson.toUtf8());
    if (doc.isArray()) {
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject o = v.toObject();
            const QString name = o.value("name").toString().toLower();
            const QString key  = o.value("api_key").toString();
            if      (name.contains("deepseek")) m_deepseekKeyEdit->setText(key);
            else if (name.contains("gpt") || name.contains("openai")) m_openaiKeyEdit->setText(key);
            else if (name.contains("qwen"))   m_qwenKeyEdit->setText(key);
            else if (name.contains("claude")) m_claudeKeyEdit->setText(key);
            else if (name.contains("gemini")) m_geminiKeyEdit->setText(key);
        }
    }
    m_setupAiEnabled->setChecked(settings.value("ai/enabled", true).toBool());
    m_topNSpin->setValue(settings.value("ai/deep_analysis_top_n", 20).toInt());
    loadPortfolioToTable();
}

void MainWindow::loadPortfolioToTable()
{
    QSettings s("InvestInsight", "InvestInsight");
    const QStringList cachedSectors = s.value("cache/sector_names").toStringList();
    // 兼容旧的 portfolio/entries_json 和新的 portfolio/batches_json
    QString jsonKey = s.contains("portfolio/batches_json")
        ? "portfolio/batches_json" : "portfolio/entries_json";
    const QJsonDocument doc = QJsonDocument::fromJson(
        s.value(jsonKey).toString().toUtf8());
    m_portfolioTable->setRowCount(0);
    if (!doc.isArray()) return;

    struct LoadedRow { QString sector; double amount; QString holdType; QString date; QString firstEarnDate; bool firstEarnDateManual; bool sold; };
    QList<LoadedRow> loadedRows;
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        const QString sector   = o.value("sector").toString();
        if (sector.isEmpty()) continue;
        LoadedRow lr;
        lr.sector = sector;
        lr.amount = o.value("amount").toDouble();
        lr.holdType = o.value("holdType").toString(QString::fromUtf8("基金"));
        lr.date = o.value("date").toString();
        lr.firstEarnDate = o.value("firstEarnDate").toString();
        lr.firstEarnDateManual = o.value("firstEarnDateManual").toBool(false);
        lr.sold = o.value("sold").toBool(false);
        loadedRows.append(lr);
    }
    // 排序：未卖出的在前，已卖出的在后
    std::stable_sort(loadedRows.begin(), loadedRows.end(), [](const LoadedRow &a, const LoadedRow &b) {
        return !a.sold && b.sold;
    });

    for (const LoadedRow &lr : loadedRows) {
        const QString &sector = lr.sector;
        const double amount = lr.amount;
        const QString &holdType = lr.holdType;
        const QString &date = lr.date;
        const int r = m_portfolioTable->rowCount();
        m_portfolioTable->insertRow(r);

        // col 0: 勾选框
        auto *checkCell = new QWidget(m_portfolioTable);
        checkCell->setAutoFillBackground(false);
        auto *checkLay = new QHBoxLayout(checkCell);
        checkLay->setContentsMargins(0, 0, 0, 0);
        checkLay->setAlignment(Qt::AlignCenter);
        auto *cb = new QCheckBox(checkCell);
        cb->setChecked(false);
        cb->setFocusPolicy(Qt::NoFocus);
        checkLay->addWidget(cb);
        m_portfolioTable->setCellWidget(r, 0, checkCell);

        // col 1: 板块下拉（支持输入实时过滤）
        auto *combo = new QComboBox(m_portfolioTable);
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->setFocusPolicy(Qt::ClickFocus);
        combo->lineEdit()->setPlaceholderText(QString::fromUtf8("输入关键字筛选板块..."));
        combo->addItems(cachedSectors);
        auto *completer2 = new QCompleter(cachedSectors, combo);
        completer2->setFilterMode(Qt::MatchContains);
        completer2->setCaseSensitivity(Qt::CaseInsensitive);
        completer2->setCompletionMode(QCompleter::PopupCompletion);
        combo->setCompleter(completer2);
        combo->setCurrentText(sector);
        m_portfolioTable->setCellWidget(r, 1, combo);

        // col 2: 持仓类型（与板块名称列相同的 editable cellWidget 方式）
        const QStringList typeOpts2 = {QString::fromUtf8("股票"),
                                       QString::fromUtf8("ETF"),
                                       QString::fromUtf8("基金")};
        auto *typeCombo2 = new QComboBox(m_portfolioTable);
        typeCombo2->addItems(typeOpts2);
        typeCombo2->setEditable(true);
        typeCombo2->setInsertPolicy(QComboBox::NoInsert);
        typeCombo2->setFocusPolicy(Qt::ClickFocus);
        auto *typeCompleter2 = new QCompleter(typeOpts2, typeCombo2);
        typeCompleter2->setFilterMode(Qt::MatchContains);
        typeCompleter2->setCaseSensitivity(Qt::CaseInsensitive);
        typeCompleter2->setCompletionMode(QCompleter::PopupCompletion);
        typeCombo2->setCompleter(typeCompleter2);
        const QString typeVal2 = holdType.isEmpty() ? QString::fromUtf8("基金") : holdType;
        typeCombo2->setCurrentText(typeVal2);
        typeCombo2->lineEdit()->setTextMargins(0, 0, 0, 0);
        m_portfolioTable->setCellWidget(r, 2, typeCombo2);

        // col 3: 买入日期（QDateEdit：可弹日历+可手输）
        const QDate buyDate2 = QDate::fromString(date, "yyyy-MM-dd").isValid()
            ? QDate::fromString(date, "yyyy-MM-dd") : QDate::currentDate();
        auto *buyDateEdit2 = new QDateEdit(m_portfolioTable);
        buyDateEdit2->setDisplayFormat("yyyy-MM-dd");
        buyDateEdit2->setCalendarPopup(true);
        buyDateEdit2->setDate(buyDate2);
        buyDateEdit2->setMinimumWidth(136);
        m_portfolioTable->setCellWidget(r, 3, buyDateEdit2);
        if (auto *le = buyDateEdit2->findChild<QLineEdit *>()) {
            le->setTextMargins(0, 0, 0, 0);
            QObject::connect(le, &QLineEdit::editingFinished, buyDateEdit2, [buyDateEdit2, le]() {
                QString s = le->text().trimmed();
                if (s.isEmpty()) { le->setText(buyDateEdit2->date().toString("yyyy-MM-dd")); return; }
                s.replace('/', '-');
                s.replace('.', '-');
                s.replace(QString::fromUtf8("年"), "-");
                s.replace(QString::fromUtf8("月"), "-");
                s.replace(QString::fromUtf8("日"), "");
                s.remove(' ');
                s.remove(QChar(0x3000));
                while (s.contains("--")) s.replace("--", "-");
                const QStringList p = s.split('-', Qt::SkipEmptyParts);
                QDate d;
                if (p.size() == 3) d = QDate(p[0].toInt(), p[1].toInt(), p[2].toInt());
                if (!d.isValid()) d = QDate::fromString(s, "yyyy-MM-dd");
                if (d.isValid()) buyDateEdit2->setDate(d);
                le->setText(buyDateEdit2->date().toString("yyyy-MM-dd"));
            });
        }

        // col 4: 首次收益日（默认自动计算，可手动覆盖）
        auto calcDate = [](const QString &bd, const QString &tp) -> QString {
            QDate d = QDate::fromString(bd, "yyyy-MM-dd");
            if (!d.isValid()) return bd;
            if (tp == QString::fromUtf8("基金")) {
                d = d.addDays(1);
                while (d.dayOfWeek() > 5) d = d.addDays(1);
            }
            return d.toString("yyyy-MM-dd");
        };
        const QString autoEarnDate2 = calcDate(buyDate2.toString("yyyy-MM-dd"), typeVal2);
        const QString savedEarnDate2 = lr.firstEarnDate;
        const QDate autoEarnD2 = QDate::fromString(autoEarnDate2, "yyyy-MM-dd");
        QDate earnD2 = QDate::fromString(savedEarnDate2, "yyyy-MM-dd");
        if (!earnD2.isValid()) earnD2 = autoEarnD2;
        auto *earnDateEdit2 = new QDateEdit(m_portfolioTable);
        earnDateEdit2->setDisplayFormat("yyyy-MM-dd");
        earnDateEdit2->setCalendarPopup(true);
        earnDateEdit2->setDate(earnD2.isValid() ? earnD2 : buyDate2);
        earnDateEdit2->setMinimumWidth(136);
        bool savedManual = lr.firstEarnDateManual;
        if (!savedEarnDate2.isEmpty() && autoEarnD2.isValid() && earnD2 != autoEarnD2) savedManual = true;
        earnDateEdit2->setProperty("manualOverride", savedManual);
        m_portfolioTable->setCellWidget(r, 4, earnDateEdit2);
        if (auto *le = earnDateEdit2->findChild<QLineEdit *>()) {
            le->setTextMargins(0, 0, 0, 0);
            QObject::connect(le, &QLineEdit::editingFinished, earnDateEdit2, [earnDateEdit2, le]() {
                QString s = le->text().trimmed();
                if (s.isEmpty()) { le->setText(earnDateEdit2->date().toString("yyyy-MM-dd")); return; }
                s.replace('/', '-');
                s.replace('.', '-');
                s.replace(QString::fromUtf8("年"), "-");
                s.replace(QString::fromUtf8("月"), "-");
                s.replace(QString::fromUtf8("日"), "");
                s.remove(' ');
                s.remove(QChar(0x3000));
                while (s.contains("--")) s.replace("--", "-");
                const QStringList p = s.split('-', Qt::SkipEmptyParts);
                QDate d;
                if (p.size() == 3) d = QDate(p[0].toInt(), p[1].toInt(), p[2].toInt());
                if (!d.isValid()) d = QDate::fromString(s, "yyyy-MM-dd");
                if (d.isValid()) earnDateEdit2->setDate(d);
                le->setText(earnDateEdit2->date().toString("yyyy-MM-dd"));
            });
        }
        QObject::connect(typeCombo2, &QComboBox::currentTextChanged,
            m_portfolioTable, [this, r, calcDate](const QString &newType) {
                auto *bd = qobject_cast<QDateEdit *>(m_portfolioTable->cellWidget(r, 3));
                auto *ed = qobject_cast<QDateEdit *>(m_portfolioTable->cellWidget(r, 4));
                if (!bd || !ed) return;
                if (ed->property("manualOverride").toBool()) return;
                const QString autoDate = calcDate(bd->date().toString("yyyy-MM-dd"), newType);
                const QDate d = QDate::fromString(autoDate, "yyyy-MM-dd");
                if (!d.isValid()) return;
                const QSignalBlocker blocker(ed);
                ed->setDate(d);
                ed->setProperty("manualOverride", false);
            });

        QObject::connect(buyDateEdit2, &QDateEdit::dateChanged,
            m_portfolioTable, [calcDate, typeCombo2, earnDateEdit2](const QDate &newBuyDate) {
                if (!typeCombo2 || !earnDateEdit2) return;
                if (earnDateEdit2->property("manualOverride").toBool()) return;
                const QString autoDate = calcDate(newBuyDate.toString("yyyy-MM-dd"), typeCombo2->currentText());
                const QDate d = QDate::fromString(autoDate, "yyyy-MM-dd");
                if (!d.isValid()) return;
                const QSignalBlocker blocker(earnDateEdit2);
                earnDateEdit2->setDate(d);
                earnDateEdit2->setProperty("manualOverride", false);
            });

        QObject::connect(earnDateEdit2, &QDateEdit::dateChanged,
            m_portfolioTable, [calcDate, typeCombo2, buyDateEdit2, earnDateEdit2](const QDate &newEarnDate) {
                if (!typeCombo2 || !buyDateEdit2 || !earnDateEdit2) return;
                const QString autoDate = calcDate(buyDateEdit2->date().toString("yyyy-MM-dd"), typeCombo2->currentText());
                const QDate autoD = QDate::fromString(autoDate, "yyyy-MM-dd");
                if (!autoD.isValid()) return;
                earnDateEdit2->setProperty("manualOverride", newEarnDate != autoD);
            });

        // col 5: 金额（元）
        auto *amtItem2 = new QTableWidgetItem(
            amount > 0 ? QString::number(amount, 'f', 0) : QString("0"));
        amtItem2->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_portfolioTable->setItem(r, 5, amtItem2);

        // col 6: 已卖出
        auto *soldCell2 = new QWidget(m_portfolioTable);
        soldCell2->setAutoFillBackground(false);
        auto *soldLay2 = new QHBoxLayout(soldCell2);
        soldLay2->setContentsMargins(0, 0, 0, 0);
        soldLay2->setAlignment(Qt::AlignCenter);
        auto *soldCb2 = new QCheckBox(soldCell2);
        soldCb2->setChecked(lr.sold);
        soldCb2->setFocusPolicy(Qt::NoFocus);
        soldLay2->addWidget(soldCb2);
        m_portfolioTable->setCellWidget(r, 6, soldCell2);

        if (m_reorderSoldFn) {
            auto fn = m_reorderSoldFn;
            QObject::connect(soldCb2, &QCheckBox::toggled, m_portfolioTable,
                [fn](bool) { QTimer::singleShot(0, [fn]() { if (*fn) (*fn)(); }); });
        }
    }
}

void MainWindow::saveAndEnterMainPage()
{
    const QList<AIProvider> providers = collectProvidersFromForm();
    m_orchestrator.configureAI(providers, m_setupAiEnabled->isChecked(), m_topNSpin->value());

    QSettings settings("InvestInsight", "InvestInsight");
    const QStringList cachedSectors = settings.value("cache/sector_names").toStringList();
    auto normSector = [](QString s) -> QString {
        s = s.trimmed();
        s = s.simplified();
        s.remove(' ');
        s.remove(QChar(0x3000));
        return s.toLower();
    };
    QMap<QString, QString> normalizedToCanonical;
    for (const QString &sec : cachedSectors) {
        const QString key = normSector(sec);
        if (!key.isEmpty() && !normalizedToCanonical.contains(key))
            normalizedToCanonical[key] = sec;
    }
    const bool canValidateSector = !normalizedToCanonical.isEmpty();

    // 保存所有批次（含类型、日期、首次收益日、备注、金额）
    QJsonArray batchArr;
    QStringList invalidSectorRows;
    for (int r = 0; r < m_portfolioTable->rowCount(); ++r) {
        QString sector;
        QComboBox *sectorCombo = qobject_cast<QComboBox *>(m_portfolioTable->cellWidget(r, 1));
        if (sectorCombo)
            sector = sectorCombo->currentText().trimmed();
        if (sector.isEmpty()) continue;
        const QString normalizedInput = normSector(sector);
        if (canValidateSector) {
            const QString canonical = normalizedToCanonical.value(normalizedInput);
            if (canonical.isEmpty()) {
                invalidSectorRows << QString::fromUtf8("第%1行：%2").arg(r + 1).arg(sector);
                if (sectorCombo) sectorCombo->setStyleSheet("QComboBox{border:1px solid #EF4444;}");
                continue;
            }
            sector = canonical;
            if (sectorCombo) {
                sectorCombo->setStyleSheet("");
                sectorCombo->setCurrentText(canonical);
            }
        } else if (sectorCombo) {
            sectorCombo->setStyleSheet("");
        }
        QString holdType = QString::fromUtf8("基金");
        if (auto *tc = qobject_cast<QComboBox *>(m_portfolioTable->cellWidget(r, 2)))
            holdType = tc->currentText();
        QString date;
        if (auto *de = qobject_cast<QDateEdit *>(m_portfolioTable->cellWidget(r, 3)))
            date = de->date().toString("yyyy-MM-dd");
        else
            date = m_portfolioTable->item(r, 3) ? m_portfolioTable->item(r, 3)->text() : QString();

        QString firstEarnDate;
        bool firstEarnDateManual = false;
        if (auto *fe = qobject_cast<QDateEdit *>(m_portfolioTable->cellWidget(r, 4))) {
            firstEarnDate = fe->date().toString("yyyy-MM-dd");
            firstEarnDateManual = fe->property("manualOverride").toBool();
        } else {
            firstEarnDate = m_portfolioTable->item(r, 4) ? m_portfolioTable->item(r, 4)->text() : QString();
            firstEarnDateManual = m_portfolioTable->item(r, 4)
                ? m_portfolioTable->item(r, 4)->data(Qt::UserRole).toBool() : false;
        }
        const double  amount = m_portfolioTable->item(r, 5)
            ? m_portfolioTable->item(r, 5)->text().toDouble() : 0.0;
        bool sold = false;
        if (auto *w = m_portfolioTable->cellWidget(r, 6)) {
            if (auto *scb = w->findChild<QCheckBox *>()) sold = scb->isChecked();
        }
        QJsonObject o;
        o["sector"]   = sector;
        o["holdType"] = holdType;
        o["date"]     = date;
        o["firstEarnDate"] = firstEarnDate;
        o["firstEarnDateManual"] = firstEarnDateManual;
        o["amount"]   = amount;
        o["sold"]     = sold;
        batchArr.push_back(o);
    }
    if (!invalidSectorRows.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("板块名称错误"),
            QString::fromUtf8("存在无效板块名称，请检查后再保存。\n\n错误说明：该板块名称错误，请检查板块名称是否正常。\n\n")
            + invalidSectorRows.join("\n"));
        return;
    }

    settings.setValue("portfolio/batches_json",
        QString::fromUtf8(QJsonDocument(batchArr).toJson(QJsonDocument::Compact)));

    const QMap<QString, double> portfolio = collectPortfolioFromTable();
    m_orchestrator.setPortfolio(portfolio);

    m_aiToggle->setChecked(m_orchestrator.isAIEnabled() && m_orchestrator.isAIAvailable());
    m_aiToggle->setEnabled(m_orchestrator.isAIAvailable());
    
    m_statusLabel->setText(QString::fromUtf8("配置已保存"));
    if (m_workspaceTitle) m_workspaceTitle->setText(QString::fromUtf8("配置"));
    if (m_workspaceMeta) m_workspaceMeta->setText(QString::fromUtf8("AI 接入、持仓、数据刷新和提醒设置"));
}

void MainWindow::openChatTab()
{
    if (m_chatTabIndex >= 0 && m_chatTabIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(m_chatTabIndex);
        if (m_workspaceTitle) m_workspaceTitle->setText(QString::fromUtf8("AI 助手"));
        if (m_workspaceMeta) m_workspaceMeta->setText(QString::fromUtf8("基于当前分析结果的问答和复盘"));
        return;
    }

    auto *chatTab = new QWidget(m_tabWidget);
    auto *chatLayout = new QHBoxLayout(chatTab);
    chatLayout->setContentsMargins(22, 22, 22, 22);
    chatLayout->setSpacing(22);

    const ThemeColors &t = *s_theme;
    auto *contextPanel = new QFrame(chatTab);
    contextPanel->setObjectName("chatContextPanel");
    contextPanel->setFixedWidth(330);
    auto *contextLayout = new QVBoxLayout(contextPanel);
    contextLayout->setContentsMargins(26, 24, 26, 24);
    contextLayout->setSpacing(12);

    auto *contextTitle = new QLabel(QString::fromUtf8("当前上下文"), contextPanel);
    contextTitle->setObjectName("cardTitle");
    contextLayout->addWidget(contextTitle);

    const QString marketText = m_lastResult.marketRegime.regimeName.isEmpty()
        ? QString::fromUtf8("等待分析结果")
        : m_lastResult.marketRegime.regimeName;
    const QString eventText = !m_lastResult.macroEvents.isEmpty()
        ? m_lastResult.macroEvents.first().title
        : QString::fromUtf8("暂无结构化重点事件");
    const QString strongSector = !m_lastResult.sectors.isEmpty()
        ? m_lastResult.sectors.first().industry
        : QString::fromUtf8("等待板块排序");
    QString riskSector = QString::fromUtf8("等待风险识别");
    for (const SectorSnapshot &sector : m_lastResult.sectors) {
        if (!sector.negativeFactors.isEmpty()) {
            riskSector = sector.industry + QString::fromUtf8("：") + sector.negativeFactors.first();
            break;
        }
    }

    auto addContextRow = [contextPanel, contextLayout, &t](const QString &label, const QString &value) {
        auto *box = new QFrame(contextPanel);
        box->setObjectName("configCard");
        auto *boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(18, 12, 18, 12);
        boxLayout->setSpacing(2);
        auto *labelWidget = new QLabel(label, box);
        labelWidget->setObjectName("cardMeta");
        auto *valueWidget = new QLabel(value, box);
        valueWidget->setWordWrap(true);
        valueWidget->setStyleSheet(QString("font-size:13px;font-weight:800;color:%1;").arg(t.headingColor));
        boxLayout->addWidget(labelWidget);
        boxLayout->addWidget(valueWidget);
        contextLayout->addWidget(box);
    };
    addContextRow(QString::fromUtf8("市场状态"), marketText);
    addContextRow(QString::fromUtf8("重点事件"), eventText);
    addContextRow(QString::fromUtf8("强势板块"), strongSector);
    addContextRow(QString::fromUtf8("风险板块"), riskSector);

    auto *quickTitle = new QLabel(QString::fromUtf8("快捷问题"), contextPanel);
    quickTitle->setStyleSheet(QString("font-size:15px;font-weight:800;color:%1;margin-top:12px;").arg(t.headingColor));
    contextLayout->addWidget(quickTitle);
    const QStringList quickQuestions = {
        QString::fromUtf8("为什么推荐半导体？"),
        QString::fromUtf8("有色金属看什么指标？"),
        QString::fromUtf8("我的持仓要调吗？"),
        QString::fromUtf8("哪些风险会让结论失效？")
    };
    for (const QString &question : quickQuestions) {
        auto *button = new QPushButton(question, contextPanel);
        button->setObjectName("quickQuestionButton");
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, question]() {
            if (m_chatInput) {
                m_chatInput->setText(question);
                m_chatInput->setFocus();
            }
        });
        contextLayout->addWidget(button);
    }
    contextLayout->addStretch(1);

    auto *conversationPanel = new QFrame(chatTab);
    conversationPanel->setObjectName("chatConversationPanel");
    auto *conversationLayout = new QVBoxLayout(conversationPanel);
    conversationLayout->setContentsMargins(24, 24, 24, 18);
    conversationLayout->setSpacing(14);

    auto *conversationTitle = new QLabel(QString::fromUtf8("对话"), conversationPanel);
    conversationTitle->setObjectName("cardTitle");
    conversationLayout->addWidget(conversationTitle);

    m_chatDisplay = new QTextBrowser(conversationPanel);
    m_chatDisplay->setOpenExternalLinks(true);
    m_chatDisplay->setStyleSheet(
        QString("QTextBrowser{background:%1;border:none;padding:12px;font-size:13px;color:%2;}")
        .arg(t.paneBg, t.bodyColor));

    QString welcome = "<div style='text-align:center;padding:40px 20px;'>"
        "<div style='font-size:28px;margin-bottom:12px;'>&#128172;</div>"
        "<div style='font-size:16px;font-weight:700;color:" + t.bodyColor
        + ";margin-bottom:8px;'>InvestInsight AI &#21161;&#25163;</div>"
        "<div style='font-size:12px;color:" + t.mutedColor
        + ";line-height:1.8;max-width:400px;margin:0 auto;'>"
        "&#22522;&#20110;&#23454;&#26102;&#37319;&#38598;&#30340;&#34892;&#24773;&#25968;&#25454;&#12289;"
        "&#26032;&#38395;&#12289;&#25216;&#26415;&#25351;&#26631;&#21644;&#31574;&#30053;&#20998;&#26512;"
        "&#32467;&#26524;&#65292;<br/>&#20026;&#24744;&#25552;&#20379;&#19987;&#19994;&#30340;&#25237;"
        "&#36164;&#38382;&#31572;&#26381;&#21153;&#12290;"
        "</div></div>";
    m_chatDisplay->setHtml(welcome);

    auto *inputBar = new QWidget(conversationPanel);
    inputBar->setObjectName("chatInputBar");
    inputBar->setStyleSheet(QString(
        "QWidget#chatInputBar{background:%1;border:1px solid %2;border-radius:8px;padding:4px;}")
        .arg(t.paneBg, t.paneBorder));
    auto *inputLayout = new QHBoxLayout(inputBar);
    inputLayout->setContentsMargins(12, 10, 12, 10);
    inputLayout->setSpacing(10);

    m_chatInput = new QLineEdit(inputBar);
    m_chatInput->setPlaceholderText(QString::fromUtf8("输入问题，例如：请解释半导体的事件传导路径"));
    m_chatInput->setFixedHeight(38);
    m_chatInput->setStyleSheet(QString(
        "QLineEdit{border:1px solid %1;border-radius:10px;padding:6px 14px;"
        "font-size:13px;background:%2;color:%3;}"
        "QLineEdit:focus{border-color:%4;}")
        .arg(t.paneBorder, t.paneBg, t.bodyColor, t.btnBg));
    connect(m_chatInput, &QLineEdit::returnPressed, this, &MainWindow::sendChatMessage);

    m_chatSendBtn = new QPushButton(QString::fromUtf8("发送"), inputBar);
    m_chatSendBtn->setFixedWidth(76);
    m_chatSendBtn->setFixedHeight(38);
    m_chatSendBtn->setCursor(Qt::PointingHandCursor);
    m_chatSendBtn->setStyleSheet(QString(
        "QPushButton{background:%1;color:#fff;border:none;border-radius:10px;"
        "font-size:13px;font-weight:600;letter-spacing:0.5px;}"
        "QPushButton:hover{background:%2;}"
        "QPushButton:pressed{background:#3730A3;}"
        "QPushButton:disabled{background:%3;color:rgba(255,255,255,0.4);}")
        .arg(t.btnBg, t.btnHover, t.btnDisabled));
    connect(m_chatSendBtn, &QPushButton::clicked, this, &MainWindow::sendChatMessage);

    inputLayout->addWidget(m_chatInput, 1);
    inputLayout->addWidget(m_chatSendBtn);

    conversationLayout->addWidget(m_chatDisplay, 1);
    conversationLayout->addWidget(inputBar);
    chatLayout->addWidget(contextPanel);
    chatLayout->addWidget(conversationPanel, 1);

    m_chatTabIndex = static_cast<ScrollableTabWidget *>(m_tabWidget)->addClosableTab(
        chatTab, QString::fromUtf8("AI助手"));
    m_tabWidget->setCurrentIndex(m_chatTabIndex);
    if (m_workspaceTitle) m_workspaceTitle->setText(QString::fromUtf8("AI 助手"));
    if (m_workspaceMeta) m_workspaceMeta->setText(QString::fromUtf8("基于当前分析结果的问答和复盘"));
}

void MainWindow::sendChatMessage()
{
    if (!m_chatInput || !m_chatDisplay) return;
    const QString question = m_chatInput->text().trimmed();
    if (question.isEmpty()) return;
    m_chatInput->clear();

    const ThemeColors &t = *s_theme;

    static QString chatHistory;

    chatHistory += "<div style='text-align:right;margin:10px 0;'>"
        "<div style='display:inline-block;max-width:80%;text-align:left;padding:10px 14px;"
        "border-radius:12px 12px 2px 12px;background:#4338CA;color:#fff;font-size:13px;line-height:1.6;'>"
        + question.toHtmlEscaped() + "</div></div>";

    const QString thinkTag = "THINKING_PLACEHOLDER_TAG_" + QString::number(reinterpret_cast<quintptr>(this));
    chatHistory += "<div style='text-align:left;margin:10px 0;'>"
        "<div style='display:inline-block;max-width:85%;text-align:left;padding:10px 14px;"
        "border-radius:12px 12px 12px 2px;background:" + t.narrativeBg + ";border:1px solid " + t.cardBorder
        + ";color:" + t.bodyColor + ";font-size:13px;line-height:1.6;'>"
        + thinkTag + "</div></div>";
    m_chatDisplay->setHtml(chatHistory);
    QScrollBar *scrollB = m_chatDisplay->verticalScrollBar();
    if (scrollB) scrollB->setValue(scrollB->maximum());

    m_chatSendBtn->setEnabled(false);
    m_chatInput->setEnabled(false);

    if (m_lastResult.sectors.isEmpty()) {
        chatHistory.replace(thinkTag, QString::fromUtf8("请先点击「开始分析」收集数据后再提问。"));
        m_chatDisplay->setHtml(chatHistory);
        m_chatSendBtn->setEnabled(true);
        m_chatInput->setEnabled(true);
        return;
    }

    if (!m_orchestrator.isAIAvailable()) {
        chatHistory.replace(thinkTag, QString::fromUtf8("AI 未配置，请先在配置页设置 API Key。"));
        m_chatDisplay->setHtml(chatHistory);
        m_chatSendBtn->setEnabled(true);
        m_chatInput->setEnabled(true);
        return;
    }

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, thinkTag]() {
        const QString answer = watcher->result();
        watcher->deleteLater();

        const QString escapedAnswer = answer.toHtmlEscaped().replace("\n", "<br/>");
        chatHistory.replace(thinkTag, escapedAnswer);
        if (m_chatDisplay) {
            m_chatDisplay->setHtml(chatHistory);
            QScrollBar *sb = m_chatDisplay->verticalScrollBar();
            if (sb) sb->setValue(sb->maximum());
        }
        if (m_chatSendBtn) m_chatSendBtn->setEnabled(true);
        if (m_chatInput) { m_chatInput->setEnabled(true); m_chatInput->setFocus(); }
    });

    const AnalysisResult ctx = m_lastResult;
    watcher->setFuture(QtConcurrent::run([this, question, ctx]() {
        return m_orchestrator.chatQuery(question, ctx);
    }));
}
