#include "ui/MainWindow.h"
#include "ui/AppTheme.h"
#include "ui/renderers/ChartRenderer.h"
#include "core/TechIndicators.h"

#include <algorithm>
#include <cmath>

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
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDesktopServices>
#include <QFormLayout>
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
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPixmap>
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
QString actionText(AdviceAction a)
{
    switch (a) {
    case AdviceAction::Increase: return "增配";
    case AdviceAction::Decrease: return "减配";
    default: return "持有";
    }
}

QString tagClass(AdviceAction a)
{
    switch (a) {
    case AdviceAction::Increase: return "tag-up";
    case AdviceAction::Decrease: return "tag-down";
    default: return "tag-hold";
    }
}

QString pct(double v)
{
    return (v >= 0 ? "+" : "") + QString::number(v, 'f', 2) + "%";
}

QString clr(double v)
{
    if (v > 0.001) return "#EF4444";
    if (v < -0.001) return "#3B82F6";
    return s_theme ? s_theme->neutralColor : "#455A64";
}

QString num(double v, int d = 2) { return QString::number(v, 'f', d); }

QString px2b64(const QPixmap &pm)
{
    QByteArray b; QBuffer buf(&b); buf.open(QIODevice::WriteOnly); pm.save(&buf, "PNG");
    return QString::fromLatin1(b.toBase64());
}

QString scoreBar(double score)
{
    const int bw = static_cast<int>(qAbs(score) * 60);
    const QString bgColor = score >= 0 ? "rgba(239,68,68,0.12)" : "rgba(59,130,246,0.12)";
    const QString fgColor = score >= 0 ? "#EF4444" : "#3B82F6";
    return "<span style='display:inline-flex;align-items:center;gap:4px;'>"
        "<span style='display:inline-block;width:60px;height:4px;border-radius:2px;background:" + bgColor + ";position:relative;overflow:hidden;'>"
        "<span style='display:inline-block;width:" + num(bw, 0) + "px;height:4px;border-radius:2px;background:" + fgColor + ";'></span></span>"
        " <b style='color:" + clr(score) + ";font-size:11px;'>" + num(score) + "</b></span>";
}

QString fmtFactor(double v)
{
    QString s = (v >= 0 ? "+" : "") + QString::number(v, 'f', 4);
    QString c = s_theme ? s_theme->neutralColor : "#455A64";
    if (v > 0.001) c = "#EF4444";
    if (v < -0.001) c = "#3B82F6";
    return "<span style='color:" + c + ";'>" + s + "</span>";
}

QString tc(const QString &fallback)
{
    return s_theme ? s_theme->subtleColor : fallback;
}
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
    m_backToSetupButton->setEnabled(false);
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
    m_backToSetupButton->setEnabled(true);
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
    if (m_overviewBrowser)
        m_overviewBrowser->setHtml(buildSectorTableHtml(analysis));
    if (m_strategyBrowser)
        m_strategyBrowser->setHtml(buildStrategyHtml(analysis));
}

void MainWindow::openSectorTab(const QString &sectorName)
{
    if (m_openSectorTabs.contains(sectorName)) {
        m_tabWidget->setCurrentIndex(m_openSectorTabs.value(sectorName));
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
            return;
        }
    }
}

void MainWindow::openIndexTab(const QString &indexKey)
{
    const QString tabKey = "index:" + indexKey;
    if (m_openSectorTabs.contains(tabKey)) {
        m_tabWidget->setCurrentIndex(m_openSectorTabs.value(tabKey));
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
    const ThemeColors &t = *s_theme;
    QString h = "<html><head><style>" + UiTheme::buildHtmlCss(t) + "</style></head><body>";

    h += "<h1 style='font-size:18px;'>数据总览</h1>";
    const int idxCount =
        (!analysis.marketCtx.shanghai.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.shenzhen.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.chinext.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.csi300.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.csi500.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.nasdaq.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.sp500.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.dowjones.name.isEmpty() ? 1 : 0);
    h += "<div class='meta'>"
        + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")
        + " · " + num(analysis.sectors.size(), 0) + " 板块 + " + num(idxCount, 0) + " 指数 · "
        "12路数据源";
    if (analysis.aiAvailable)
        h += " · <span class='ai-badge'>AI</span>";
    h += "</div>";

    const bool simpleMode = m_viewMode && m_viewMode->currentIndex() == 0;
    const QString posClr = "#EF4444";
    const QString negClr = "#3B82F6";
    const QString cardBg = t.narrativeBg;

    // ===== 我的持仓仪表盘（最高优先级，放在最前面）=====
    {
        const QMap<QString, double> &portfolio = m_orchestrator.portfolio();
        QMap<QString, double> holdTotalBySector;
        QMap<QString, double> holdConfirmedBySector;
        {
            QSettings ps("InvestInsight", "InvestInsight");
            const QString batchKey = ps.contains("portfolio/batches_json")
                ? "portfolio/batches_json" : "portfolio/entries_json";
            const QJsonDocument pfDoc = QJsonDocument::fromJson(
                ps.value(batchKey).toString().toUtf8());
            auto calcEarnDate = [](const QString &buyDate, const QString &tp) -> QDate {
                QDate d = QDate::fromString(buyDate, "yyyy-MM-dd");
                if (!d.isValid()) return QDate::currentDate();
                if (tp == QString::fromUtf8("基金")) {
                    d = d.addDays(1);
                    while (d.dayOfWeek() > 5) d = d.addDays(1);
                }
                return d;
            };
            const QDate today = QDate::currentDate();
            if (pfDoc.isArray()) {
                for (const QJsonValue &v : pfDoc.array()) {
                    const QJsonObject o = v.toObject();
                    const QString sector = o.value("sector").toString();
                    if (sector.isEmpty()) continue;
                    if (o.value("sold").toBool(false)) continue;
                    const double amt = o.value("amount").toDouble();
                    const QString holdType = o.value("holdType").toString(QString::fromUtf8("基金"));
                    const QString buyDate = o.value("date").toString();
                    const QString firstEarnDateStr = o.value("firstEarnDate").toString();
                    QDate earnDate = QDate::fromString(firstEarnDateStr, "yyyy-MM-dd");
                    if (!earnDate.isValid()) earnDate = calcEarnDate(buyDate, holdType);
                    qDebug() << "[Dashboard] batch:" << sector << "amt=" << amt
                             << "earnDate=" << earnDate.toString("yyyy-MM-dd")
                             << "today=" << today.toString("yyyy-MM-dd")
                             << "confirmed=" << (earnDate <= today);
                    holdTotalBySector[sector] += amt;
                    if (earnDate <= today) holdConfirmedBySector[sector] += amt;
                }
            }
        }
        QList<const SectorSnapshot *> heldSectors;
        for (const SectorSnapshot &s : analysis.sectors) {
            if (holdTotalBySector.contains(s.industry) || portfolio.contains(s.industry))
                heldSectors.push_back(&s);
        }
        if (!heldSectors.isEmpty()) {
            h += "<div class='section-title'>&#127981; 我的持仓总览</div>";

            // 检测休市状态
            bool anyMarketClosed = false;
            QString lastTradingDate;
            for (const SectorSnapshot *s : heldSectors) {
                if (s->marketClosed) { anyMarketClosed = true; lastTradingDate = s->lastDataDate; break; }
            }

            // 持仓概要统计
            double totalValue = 0, totalConfirmed = 0, totalPending = 0, totalPnl = 0, totalCumulPnl = 0, weightedForecast = 0;
            int urgentCount = 0;
            for (const SectorSnapshot *s : heldSectors) {
                double amt = holdTotalBySector.value(s->industry, portfolio.value(s->industry));
                double confirmedAmt = qMin(amt, holdConfirmedBySector.value(s->industry, amt));
                totalValue += amt;
                totalConfirmed += confirmedAmt;
                totalPending += (amt - confirmedAmt);
                if (!anyMarketClosed && s->todayChangePctValid)
                    totalPnl += confirmedAmt * s->todayChangePct / 100.0;
                totalCumulPnl += confirmedAmt * s->cumulativeReturn / 100.0;
                weightedForecast += amt * s->forecastScore;
                if (s->action != AdviceAction::Hold) ++urgentCount;
            }
            if (totalValue > 0) weightedForecast /= totalValue;

            const QString todayLabel = anyMarketClosed
                ? QString::fromUtf8("休市（") + lastTradingDate + "）"
                : QString::fromUtf8("今日盈亏");

            // 概要卡片
            const QString pnlClr = anyMarketClosed ? t.mutedColor : (totalPnl >= 0 ? posClr : negClr);
            const QString cumulClr = totalCumulPnl >= 0 ? posClr : negClr;
            const QString foreClr = weightedForecast >= 0 ? posClr : negClr;
            h += "<table style='width:100%;border-collapse:separate;border-spacing:6px 0;margin-bottom:10px;'><tr>";
            h += QString(
                "<td style='width:20%;padding:12px 10px;background:%1;border-radius:10px;text-align:center;border:1px solid " + t.cardBorder + ";'>"
                "<div style='font-size:10px;color:%2;letter-spacing:0.5px;text-transform:uppercase;'>持仓总额</div>"
                "<div style='font-size:20px;font-weight:800;color:%3;margin:4px 0 2px;'>%4<span style=\'font-size:12px;font-weight:500;\'> 元</span></div>"
                "<div style='font-size:10px;color:%5;'>%6 个板块</div></td>")
                .arg(cardBg, t.mutedColor, t.bodyColor)
                .arg(totalValue, 0, 'f', 0)
                .arg(t.mutedColor)
                .arg(heldSectors.size());
            if (anyMarketClosed) {
                h += QString(
                    "<td style='width:20%;padding:12px 10px;background:%1;border-radius:10px;text-align:center;border:1px solid " + t.cardBorder + ";'>"
                    "<div style='font-size:10px;color:%2;letter-spacing:0.5px;text-transform:uppercase;'>%3</div>"
                    "<div style='font-size:16px;font-weight:700;color:%2;margin:4px 0 2px;'>休市中</div>"
                    "<div style='font-size:10px;color:%2;'>数据截至 %4</div></td>")
                    .arg(cardBg, t.mutedColor, todayLabel, lastTradingDate);
            } else {
                h += QString(
                    "<td style='width:20%;padding:12px 10px;background:%1;border-radius:10px;text-align:center;border:1px solid " + t.cardBorder + ";'>"
                    "<div style='font-size:10px;color:%2;letter-spacing:0.5px;text-transform:uppercase;'>今日盈亏</div>"
                    "<div style='font-size:20px;font-weight:800;color:%3;margin:4px 0 2px;'>%4%5<span style=\'font-size:12px;font-weight:500;\'> 元</span></div>"
                    "<div style='font-size:10px;color:%2;'>按已确认资金计算</div></td>")
                    .arg(cardBg, t.mutedColor, pnlClr)
                    .arg(totalPnl >= 0 ? "+" : "")
                    .arg(totalPnl, 0, 'f', 2);
            }
            h += QString(
                "<td style='width:20%;padding:12px 10px;background:%1;border-radius:10px;text-align:center;border:1px solid " + t.cardBorder + ";'>"
                "<div style='font-size:10px;color:%2;letter-spacing:0.5px;text-transform:uppercase;'>累计盈亏</div>"
                "<div style='font-size:20px;font-weight:800;color:%3;margin:4px 0 2px;'>%4%5<span style=\'font-size:12px;font-weight:500;\'> 元</span></div>"
                "<div style='font-size:10px;color:%2;'>自追踪以来</div></td>")
                .arg(cardBg, t.mutedColor, cumulClr)
                .arg(totalCumulPnl >= 0 ? "+" : "")
                .arg(totalCumulPnl, 0, 'f', 2);
            h += QString(
                "<td style='width:20%;padding:12px 10px;background:%1;border-radius:10px;text-align:center;border:1px solid " + t.cardBorder + ";'>"
                "<div style='font-size:10px;color:%2;letter-spacing:0.5px;text-transform:uppercase;'>加权预测</div>"
                "<div style='font-size:20px;font-weight:800;color:%3;margin:4px 0;'>%4</div></td>")
                .arg(cardBg, t.mutedColor, foreClr)
                .arg(weightedForecast, 0, 'f', 3);
            QString urgClr = urgentCount > 0 ? "#FF6F00" : "#43A047";
            h += QString(
                "<td style='width:20%;padding:12px 10px;background:%1;border-radius:10px;text-align:center;border:1px solid " + t.cardBorder + ";'>"
                "<div style='font-size:10px;color:%2;letter-spacing:0.5px;text-transform:uppercase;'>需关注</div>"
                "<div style='font-size:20px;font-weight:800;color:%3;margin:4px 0 2px;'>%4</div>"
                "<div style='font-size:10px;color:%5;'>需操作</div></td>")
                .arg(cardBg, t.mutedColor, urgClr)
                .arg(urgentCount).arg(t.mutedColor);
            h += "</tr></table>";
            if (anyMarketClosed) {
                h += "<div style='margin:-2px 2px 8px;color:" + t.mutedColor + ";font-size:11px;'>"
                     + QString::fromUtf8("当前处于休市/节假日状态，行情数据截至上一交易日 ")
                     + lastTradingDate + "</div>";
            }
            if (totalPending > 0) {
                h += "<div style='margin:-2px 2px 8px;color:#D97706;font-size:11px;'>"
                     + QString::fromUtf8("待确认资金：¥")
                     + QString::number(totalPending, 'f', 2)
                     + QString::fromUtf8("（未到首次收益日，暂不计入盈亏）</div>");
            }

            // 每个持仓的行动卡片
            const QString todayColHeader = anyMarketClosed
                ? QString::fromUtf8("上一交易日") : QString::fromUtf8("今日");
            h += "<table class='overview' style='margin-bottom:6px;'>";
            h += "<tr><th>持仓板块</th><th>金额</th><th style='text-align:right;'>" + todayColHeader + "</th>"
                "<th style='text-align:right;'>累计</th>"
                "<th style='text-align:center;'>建议</th><th style='text-align:center;'>策略</th>"
                "<th>一句话建议</th></tr>";

            for (const SectorSnapshot *s : heldSectors) {
                double amt = holdTotalBySector.value(s->industry, portfolio.value(s->industry));
                double confirmedAmt = qMin(amt, holdConfirmedBySector.value(s->industry, amt));
                double pendingAmt = amt - confirmedAmt;
                double pnl = (anyMarketClosed || !s->todayChangePctValid)
                    ? 0.0 : (confirmedAmt * s->todayChangePct / 100.0);

                // 生成一句话建议
                QString oneliner;
                switch (s->action) {
                case AdviceAction::Increase:
                    oneliner = "评分" + num(s->forecastScore) + "偏多，可考虑加仓"
                        + QString::number(amt * 0.2, 'f', 0) + "元。止盈+" + num(s->strategy.takeProfitPct, 1) + "%";
                    break;
                case AdviceAction::Decrease:
                    oneliner = "评分" + num(s->forecastScore) + "偏空，建议减仓"
                        + QString::number(amt * 0.3, 'f', 0) + "元。止损" + num(s->strategy.stopLossPct, 1) + "%";
                    break;
                default:
                    oneliner = "评分" + num(s->forecastScore) + "中性，暂不操作，关注"
                        + (s->tech.macdGoldenCross ? "MACD金叉后续" :
                           s->tech.macdDeadCross ? "MACD死叉风险" : "后续信号");
                    break;
                }

                const bool isUrgent = s->action != AdviceAction::Hold;
                const QString rowStyle = isUrgent
                    ? " style='background:" + QString(s->action == AdviceAction::Decrease ? "rgba(239,68,68,0.04)" : "rgba(34,197,94,0.04)") + ";'"
                    : "";

                h += "<tr" + rowStyle + ">";
                int sectorIdx = 0;
                for (int si = 0; si < analysis.sectors.size(); ++si) {
                    if (&analysis.sectors[si] == s) { sectorIdx = si + 1; break; }
                }
                h += "<td><a href='jump-" + QString::number(sectorIdx) + "'>"
                    + s->industry + "</a></td>";
                h += "<td>¥" + QString::number(amt, 'f', 0) + "</td>";
                QString todayCell;
                if (anyMarketClosed) {
                    todayCell = QString::fromUtf8("<span style='color:") + t.mutedColor + ";'>休市</span>";
                } else if (confirmedAmt <= 0 && pendingAmt > 0) {
                    todayCell = QString::fromUtf8("<span style='color:#D97706;'>全部待确认</span>");
                } else if (!s->todayChangePctValid) {
                    todayCell = QString::fromUtf8("<span style='color:") + t.mutedColor + ";'>"
                        + QString::fromUtf8("数据缺失") + "</span>";
                } else {
                    todayCell = pct(s->todayChangePct) + " (" + (pnl >= 0 ? "+" : "")
                        + QString::number(pnl, 'f', 2) + "¥)";
                    if (pendingAmt > 0) {
                        todayCell += QString::fromUtf8("<br/><span style='font-size:10px;color:#D97706;'>待确认 ¥")
                            + QString::number(pendingAmt, 'f', 0) + "</span>";
                    }
                }
                const QString todayCellClr = (!s->todayChangePctValid || anyMarketClosed) ? t.mutedColor : clr(s->todayChangePct);
                h += "<td style='text-align:right;color:" + todayCellClr + ";font-weight:700;'>"
                    + todayCell + "</td>";
                double cumulPnl = confirmedAmt * s->cumulativeReturn / 100.0;
                QString cumulCell;
                if (confirmedAmt <= 0 && pendingAmt > 0) {
                    cumulCell = QString::fromUtf8("<span style='color:#D97706;'>待确认</span>");
                } else {
                    cumulCell = pct(s->cumulativeReturn) + " (" + (cumulPnl >= 0 ? "+" : "")
                        + QString::number(cumulPnl, 'f', 2) + "¥)";
                }
                h += "<td style='text-align:right;color:" + clr(s->cumulativeReturn) + ";font-weight:600;font-size:11px;'>"
                    + cumulCell + "</td>";
                h += "<td style='text-align:center;'><span class='tag " + tagClass(s->action) + "'>" + actionText(s->action) + "</span></td>";
                h += "<td style='text-align:center;font-size:11px;'>" + s->strategy.actionLabel + "</td>";
                h += "<td style='font-size:11px;'>" + oneliner + "</td>";
                h += "</tr>";
            }
            h += "</table>";

            // 组合风险提示
            if (heldSectors.size() >= 2) {
                int increaseCount = 0, decreaseCount = 0;
                double maxConcentration = 0;
                for (const SectorSnapshot *s : heldSectors) {
                    double amt = holdTotalBySector.value(s->industry, portfolio.value(s->industry));
                    double pct = totalValue > 0 ? amt / totalValue : 0.0;
                    if (pct > maxConcentration) maxConcentration = pct;
                    if (s->action == AdviceAction::Increase) ++increaseCount;
                    if (s->action == AdviceAction::Decrease) ++decreaseCount;
                }

                QStringList tips;
                if (maxConcentration > 0.5)
                    tips << QString("单一板块占比 %1%，集中度过高，建议分散配置").arg(maxConcentration * 100, 0, 'f', 0);
                if (decreaseCount > heldSectors.size() / 2)
                    tips << QString("持仓中 %1/%2 建议减配，整体风险偏高，建议降低总仓位").arg(decreaseCount).arg(heldSectors.size());
                if (increaseCount > 0 && decreaseCount > 0)
                    tips << "持仓存在方向分歧，可考虑将减配板块资金转移到增配板块";
                if (weightedForecast < -0.1)
                    tips << "加权预测偏空，整体持仓面临调整压力，注意控制风险";

                if (!tips.isEmpty()) {
                    h += "<div style='background:" + cardBg + ";border-left:3px solid #F59E0B;border-radius:8px;padding:10px 14px;margin-bottom:8px;font-size:12px;border:1px solid " + t.cardBorder + ";'>"
                        "<b style='color:#D97706;font-size:11px;letter-spacing:0.3px;'>⚠ 组合风险提示</b><br/>";
                    for (const QString &tip : tips) h += "• " + tip.toHtmlEscaped() + "<br/>";
                    h += "</div>";
                }
            }
            h += "<hr class='divider'/>";
        }
    }

    // ===== 市场仪表盘 =====
    const MarketContext &mctx = analysis.marketCtx;
    if (mctx.valid) {
        h += "<div class='section-title'>市场仪表盘</div>";
        h += "<table style='width:100%;border-collapse:separate;border-spacing:5px 0;margin-bottom:10px;'><tr>";

        auto indexCard = [&](const IndexSnapshot &idx) -> QString {
            const QString clrVal = idx.changePct >= 0 ? posClr : negClr;
            const QString arrow = idx.changePct >= 0 ? "&#9650;" : "&#9660;";
            return QString(
                "<td style='width:20%%;padding:10px 8px;background:%1;border-radius:10px;text-align:center;border:1px solid %9;'>"
                "<div style='font-size:10px;color:%2;font-weight:600;letter-spacing:0.3px;'>%3</div>"
                "<div style='font-size:15px;font-weight:700;color:%4;margin:3px 0 2px;'>%5</div>"
                "<div style='font-size:11px;color:%6;font-weight:700;'>%7 %8%%</div>"
                "</td>")
                .arg(cardBg, t.mutedColor, idx.name)
                .arg(t.bodyColor)
                .arg(idx.lastClose, 0, 'f', 2)
                .arg(clrVal, arrow)
                .arg(idx.changePct, 0, 'f', 2)
                .arg(t.cardBorder);
        };

        h += indexCard(mctx.shanghai);
        h += indexCard(mctx.shenzhen);
        h += indexCard(mctx.chinext);
        h += indexCard(mctx.csi300);
        h += indexCard(mctx.csi500);
        h += "</tr></table>";

        // 美股指数（若有数据则展示）
        if (mctx.nasdaq.lastClose > 0 || mctx.sp500.lastClose > 0 || mctx.dowjones.lastClose > 0) {
            h += "<div style='font-size:11px;font-weight:600;color:" + t.mutedColor + ";margin:4px 0 4px 2px;letter-spacing:0.3px;'>美股参考</div>";
            h += "<table style='width:100%;border-collapse:separate;border-spacing:5px 0;margin-bottom:10px;'><tr>";
            auto usCard = [&](const IndexSnapshot &idx) -> QString {
                if (idx.lastClose <= 0) return QString();
                const QString clrVal = idx.changePct >= 0 ? posClr : negClr;
                const QString arrow  = idx.changePct >= 0 ? "&#9650;" : "&#9660;";
                return QString(
                    "<td style='width:33%%;padding:8px 8px;background:%1;border-radius:10px;text-align:center;border:1px solid %6;'>"
                    "<div style='font-size:10px;color:%2;font-weight:600;letter-spacing:0.3px;'>%3</div>"
                    "<div style='font-size:14px;font-weight:700;color:%4;margin:3px 0 2px;'>%5</div>"
                    "<div style='font-size:11px;color:%7;font-weight:700;'>%8 %9%%</div>"
                    "</td>")
                    .arg(cardBg, t.mutedColor, idx.name)
                    .arg(t.bodyColor)
                    .arg(idx.lastClose > 10000
                         ? QString::number(idx.lastClose, 'f', 0)
                         : QString::number(idx.lastClose, 'f', 2))
                    .arg(t.cardBorder, clrVal, arrow)
                    .arg(idx.changePct, 0, 'f', 2);
            };
            h += usCard(mctx.nasdaq);
            h += usCard(mctx.sp500);
            h += usCard(mctx.dowjones);
            h += "</tr></table>";
        }

        if (simpleMode) {
            // 简明模式：仅一行文字概述
            QString sentimentClr = mctx.marketRiskScore >= 60 ? posClr : (mctx.marketRiskScore <= 35 ? negClr : "#FF6F00");
            h += "<div style='background:" + cardBg + ";border-radius:8px;padding:8px 14px;margin-bottom:8px;font-size:12px;border:1px solid " + t.cardBorder + ";'>"
                "<b style='color:" + sentimentClr + ";'>" + mctx.riskLevel + "</b>"
                " &nbsp;|&nbsp; 涨 " + QString::number(mctx.advanceCount) + " 跌 " + QString::number(mctx.declineCount)
                + " &nbsp;|&nbsp; 北向 <span style='color:" + (mctx.northboundNetBuy >= 0 ? posClr : negClr)
                + ";font-weight:700;'>" + (mctx.northboundNetBuy >= 0 ? "+" : "") + num(mctx.northboundNetBuy, 1) + "亿</span>"
                + " &nbsp;|&nbsp; 风险 " + num(analysis.riskRadar.compositeRisk, 0) + "/100"
                + "</div>";
        } else {
        h += "<table style='width:100%;border-collapse:separate;border-spacing:5px 0;margin-bottom:10px;'><tr>";

        if (mctx.northboundFlowValid) {
            const QString nbClr = mctx.northboundNetBuy >= 0 ? posClr : negClr;
            h += QString(
                "<td style='width:24%;padding:10px 8px;background:%1;border-radius:10px;text-align:center;border:1px solid " + t.cardBorder + ";'>"
                "<div style='font-size:10px;color:%2;font-weight:600;letter-spacing:0.3px;'>北向资金</div>"
                "<div style='font-size:15px;font-weight:700;color:%3;margin:3px 0 2px;'>%4 亿</div>"
                "<div style='font-size:10px;color:%5;'>5d %6 | 20d %7</div>"
                "</td>")
                .arg(cardBg, t.mutedColor, nbClr)
                .arg(mctx.northboundNetBuy, 0, 'f', 2)
                .arg(t.mutedColor)
                .arg(mctx.northbound5dAvg, 0, 'f', 2)
                .arg(mctx.northbound20dAvg, 0, 'f', 2);
        }

        h += QString(
            "<td style='width:24%;padding:10px 8px;background:%1;border-radius:10px;text-align:center;border:1px solid " + t.cardBorder + ";'>"
            "<div style='font-size:10px;color:%2;font-weight:600;letter-spacing:0.3px;'>市场广度</div>"
            "<div style='font-size:13px;margin:3px 0 2px;'>"
            "<span style='color:%3;font-weight:700;'>&#9650; %4</span>"
            " : "
            "<span style='color:%5;font-weight:700;'>&#9660; %6</span>"
            "</div>"
            "<div style='font-size:10px;color:%7;'>涨停 %8 | 跌停 %9</div>"
            "</td>")
            .arg(cardBg, t.mutedColor)
            .arg(posClr).arg(mctx.advanceCount)
            .arg(negClr).arg(mctx.declineCount)
            .arg(t.mutedColor)
            .arg(mctx.limitUpCount).arg(mctx.limitDownCount);

        const MarketRiskRadar &radar = analysis.riskRadar;
        QString riskClr = t.warningColor;
        if (mctx.marketRiskScore >= 60) riskClr = posClr;
        else if (mctx.marketRiskScore <= 35) riskClr = negClr;
        h += QString(
            "<td style='width:24%;padding:10px 8px;background:%1;border-radius:10px;text-align:center;border:1px solid " + t.cardBorder + ";'>"
            "<div style='font-size:10px;color:%2;font-weight:600;letter-spacing:0.3px;'>情绪</div>"
            "<div style='font-size:15px;font-weight:700;color:%3;margin:3px 0 2px;'>%4</div>"
            "<div style='font-size:10px;color:%5;'>%6/100</div>"
            "</td>")
            .arg(cardBg, t.mutedColor, riskClr)
            .arg(mctx.riskLevel)
            .arg(t.mutedColor)
            .arg(mctx.marketRiskScore, 0, 'f', 0);

        QString radarClr = "#FF6F00";
        if (radar.compositeRisk >= 65) radarClr = negClr;
        else if (radar.compositeRisk <= 35) radarClr = posClr;
        h += QString(
            "<td style='width:24%;padding:10px 8px;background:%1;border-radius:10px;text-align:center;border:1px solid " + t.cardBorder + ";'>"
            "<div style='font-size:10px;color:%2;font-weight:600;letter-spacing:0.3px;'>风险</div>"
            "<div style='font-size:15px;font-weight:700;color:%3;margin:3px 0 2px;'>%4/100</div>"
            "<div style='font-size:10px;color:%5;'>V%6 M%7 C%8</div>"
            "</td>")
            .arg(cardBg, t.mutedColor, radarClr)
            .arg(radar.compositeRisk, 0, 'f', 0)
            .arg(t.mutedColor)
            .arg(radar.valuationRisk, 0, 'f', 0)
            .arg(radar.momentumRisk, 0, 'f', 0)
            .arg(radar.capitalFlowRisk, 0, 'f', 0);

        h += "</tr></table>";

        h += "<div style='background:" + cardBg + ";border-radius:8px;padding:10px 14px;margin-bottom:10px;font-size:12px;color:" + t.bodyColor + ";line-height:1.6;border:1px solid " + t.cardBorder + ";'>"
            "<b style='color:" + radarClr + ";font-size:11px;'>⚠ 风险提示</b><br/>" + radar.riskAdvice.toHtmlEscaped() + "</div>";

        // 市场状态机
        {
            const MarketRegimeResult &mr = analysis.marketRegime;
            QString regClr = "#6B7280";
            if (mr.regime == MarketRegime::BullMarket || mr.regime == MarketRegime::RiskOn) regClr = posClr;
            else if (mr.regime == MarketRegime::BearMarket || mr.regime == MarketRegime::RiskOff) regClr = negClr;
            else if (mr.regime == MarketRegime::HighVolatility) regClr = "#F59E0B";
            h += "<div style='display:flex;gap:8px;margin-bottom:10px;'>"
                "<div style='flex:1;background:" + cardBg + ";border:1px solid " + t.cardBorder + ";border-radius:10px;padding:10px 14px;'>"
                "<div style='font-size:10px;color:" + t.mutedColor + ";'>" + QString::fromUtf8("市场状态") + "</div>"
                "<div style='font-size:15px;font-weight:700;color:" + regClr + ";'>" + mr.regimeName + "</div>"
                "<div style='font-size:10px;color:" + t.mutedColor + ";'>" + QString::fromUtf8("置信度 ") + num(mr.regimeConfidence, 0) + "%</div></div>"
                "<div style='flex:2;background:" + cardBg + ";border:1px solid " + t.cardBorder + ";border-radius:10px;padding:10px 14px;'>"
                "<div style='font-size:10px;color:" + t.mutedColor + ";margin-bottom:4px;'>" + QString::fromUtf8("动态因子权重") + "</div>"
                "<div style='font-size:11px;color:" + t.bodyColor + ";'>"
                + QString::fromUtf8("动量 ") + num(mr.weights.momentumWeight * 100, 0) + "% &nbsp; "
                + QString::fromUtf8("估值 ") + num(mr.weights.valuationWeight * 100, 0) + "% &nbsp; "
                + QString::fromUtf8("情绪 ") + num(mr.weights.sentimentWeight * 100, 0) + "% &nbsp; "
                + QString::fromUtf8("风险 ") + num(mr.weights.riskWeight * 100, 0) + "% &nbsp; "
                + QString::fromUtf8("宽度 ") + num(mr.weights.breadthWeight * 100, 0) + "% &nbsp; "
                + QString::fromUtf8("持续 ") + num(mr.weights.sustainabilityWeight * 100, 0) + "%"
                "</div></div></div>";
        }
        } // end if (!simpleMode) for detailed market dashboard row

        if (!simpleMode && !analysis.rotationSignals.isEmpty()) {
            h += "<div style='margin-bottom:10px;'>";
            h += "<table style='width:100%;border-collapse:collapse;'><tr>";
            h += "<td style='width:49%;vertical-align:top;padding-right:4px;'>";
            h += "<div style='font-size:11px;font-weight:700;color:" + posClr + ";margin-bottom:4px;letter-spacing:0.3px;'>&#9650; 资金轮入</div>";
            h += "<table class='overview' style='font-size:11px;'><tr><th>板块</th><th>轮动分</th><th>动量加速</th></tr>";
            int inCount = 0;
            for (const RotationSignal &rs : analysis.rotationSignals) {
                if (!rs.isRotatingIn) continue;
                if (++inCount > 5) break;
                h += "<tr><td>" + rs.sector + "</td><td style='text-align:right;color:" + posClr + ";font-weight:700;'>" + num(rs.rotationScore, 2) + "</td>"
                    "<td style='text-align:right;color:" + posClr + ";'>+" + num(rs.momentumDelta, 2) + "%</td></tr>";
            }
            if (inCount == 0) h += "<tr><td colspan='3' style='color:" + t.mutedColor + ";text-align:center;'>暂无明显轮入信号</td></tr>";
            h += "</table></td>";

            h += "<td style='width:49%;vertical-align:top;padding-left:4px;'>";
            h += "<div style='font-size:11px;font-weight:700;color:" + negClr + ";margin-bottom:4px;letter-spacing:0.3px;'>&#9660; 资金轮出</div>";
            h += "<table class='overview' style='font-size:11px;'><tr><th>板块</th><th>轮动分</th><th>动量减速</th></tr>";
            int outCount = 0;
            for (int ri = analysis.rotationSignals.size() - 1; ri >= 0; --ri) {
                const RotationSignal &rs = analysis.rotationSignals[ri];
                if (!rs.isRotatingOut) continue;
                if (++outCount > 5) break;
                h += "<tr><td>" + rs.sector + "</td><td style='text-align:right;color:" + negClr + ";font-weight:700;'>" + num(rs.rotationScore, 2) + "</td>"
                    "<td style='text-align:right;color:" + negClr + ";'>" + num(rs.momentumDelta, 2) + "%</td></tr>";
            }
            if (outCount == 0) h += "<tr><td colspan='3' style='color:" + t.mutedColor + ";text-align:center;'>暂无明显轮出信号</td></tr>";
            h += "</table></td></tr></table></div>";
        }

        h += "<hr class='divider'/>";
    }

    if (!analysis.aiErrors.isEmpty() && !analysis.aiAvailable) {
        h += "<div style='background:" + t.warningBg + ";border:1px solid " + t.warningBorder + ";border-radius:8px;padding:10px 14px;margin-bottom:10px;font-size:12px;color:" + t.warningColor + ";'>"
            "<b>⚠ AI 分析未完成</b> — " + analysis.aiErrors.join("；").toHtmlEscaped()
            + "<br/><span style='font-size:11px;'>当前使用规则引擎。检查 API Key 和余额。</span></div>";
    }

    if (analysis.aiAvailable && !analysis.aiOverallSummary.isEmpty()) {
        h += "<div class='section-title'>市场研判综述 <span class='ai-badge'>AI</span></div>";
        h += "<div class='narrative'>" + analysis.aiOverallSummary.toHtmlEscaped().replace("\n", "<br/>") + "</div>";
        h += "<hr class='divider'/>";
    }

    // 参考五层投研框架：数据 -> 分析 -> 决策 -> 执行 -> 监控
    if (!simpleMode) {
        h += "<div class='section-title'>系统架构闭环</div>";
        h += "<table class='overview' style='font-size:11px;margin-bottom:10px;'>";
        h += "<tr><th>层级</th><th>核心职责</th><th>当前能力</th></tr>";
        h += "<tr><td><b>数据层</b></td><td>多源接入、清洗、缓存</td><td>12路新闻+行情+资金流并行采集</td></tr>";
        h += "<tr><td><b>分析层</b></td><td>情绪、技术、宏观、资金分析</td><td>MACD/RSI/KDJ + 新闻事件 + 风险雷达</td></tr>";
        h += "<tr><td><b>决策层</b></td><td>信号生成、回测验证、风险过滤</td><td>多策略回测 + 评分模型 + 周期识别</td></tr>";
        h += "<tr><td><b>执行层</b></td><td>策略落地、仓位转换</td><td>全局策略建议 + 个股/板块操作建议</td></tr>";
        h += "<tr><td><b>调度监控</b></td><td>任务编排、可视化、告警</td><td>进度可视化 + AI状态/异常提示 + 事件日历</td></tr>";
        h += "</table>";
    }

    // 专业模式才显示方法论
    if (!simpleMode) {
        h += "<div class='section-title'>方法论";
        if (analysis.aiAvailable) h += " <span class='ai-badge'>AI</span>";
        h += "</div>";

        if (analysis.aiAvailable && !analysis.aiMethodologyNote.isEmpty()) {
            h += "<div style='font-size:12px;color:" + t.mutedColor + ";line-height:1.8;margin-bottom:8px;'>"
                + analysis.aiMethodologyNote.toHtmlEscaped().replace("\n", "<br/>") + "</div>";
        } else {
            h += "<div style='font-size:12px;color:" + t.mutedColor + ";line-height:1.8;margin-bottom:8px;'>"
                "<b>数据来源</b>：12路并行采集（东方财富、新浪财经、财联社、证券时报、上海证券报、同花顺、华尔街见闻、网易财经、Google News、国务院政策、发改委）<br/>"
                "<b>预测评分</b> = 情绪 + 动量 + 涨跌 + 资金流 + 热度 + <b>技术面(MACD/RSI/KDJ/均线/量价)</b> + <b>估值分位 + 拥挤度</b> + 均值回归，范围 [-1.00, 1.00]<br/>"
                "<b>数据质量权重</b>：依据行情/资金流/估值/多周期/新闻覆盖度计算质量分（0-100），对预测评分进行动态降权，低质量数据自动降低信号强度<br/>"
                "<b>跨源一致性权重</b>：对比多数据源涨跌口径一致性，差异越大自动降权，减少单源异常数据误导<br/>"
                "<b>市场上下文</b>：大盘指数 + 北向资金 + 涨跌家数比 + 板块轮动检测 + 六维风险雷达<br/>"
                "<b>技术综合评分</b> = MACD金叉/死叉 + RSI超买超卖 + KDJ信号 + 均线排列 + 量价配合，范围 [-1.00, 1.00]<br/>"
                "<b>操作策略</b>：综合技术面信号和基本面数据，给出止盈止损、支撑压力、多时间维度展望。"
                "（点击板块查看完整技术指标和策略详情）"
                "</div>";
        }
    }

    h += "</body></html>";
    return h;
}

// ========== 子Tab2: 板块&指数信息（搜索/过滤 + 混合表 + 审计摘要）==========
QString MainWindow::buildSectorTableHtml(const AnalysisResult &analysis) const
{
    const ThemeColors &t = *s_theme;
    QString h = "<html><head><style>" + UiTheme::buildHtmlCss(t) + "</style></head><body>";
    const bool simpleMode = m_viewMode && m_viewMode->currentIndex() == 0;

    // 读取过滤 / 排序状态
    const QString searchText  = m_overviewSearch  ? m_overviewSearch->text().trimmed()  : QString();
    const int trendIdx   = m_trendFilter  ? m_trendFilter->currentIndex()  : 0;
    const int actionIdx  = m_actionFilter ? m_actionFilter->currentIndex() : 0;
    const int sortIdx    = m_sortBy       ? m_sortBy->currentIndex()       : 0;

    static const QStringList trendNames = {
        "", "强势看多", "偏多", "横盘震荡", "方向不明", "偏空", "强势看空"};
    static const QStringList actionNames = { "", "增配", "持有", "减配" };
    const QString filterTrend  = (trendIdx  > 0 && trendIdx  < trendNames.size())  ? trendNames[trendIdx]  : QString();
    const QString filterAction = (actionIdx > 0 && actionIdx < actionNames.size()) ? actionNames[actionIdx] : QString();

    auto inferTrendByChange = [](double chg) -> QString {
        if (chg >= 2.0) return QString::fromUtf8("强势看多");
        if (chg >= 0.5) return QString::fromUtf8("偏多");
        if (chg <= -2.0) return QString::fromUtf8("强势看空");
        if (chg <= -0.5) return QString::fromUtf8("偏空");
        if (std::abs(chg) <= 0.2) return QString::fromUtf8("横盘震荡");
        return QString::fromUtf8("方向不明");
    };
    auto inferActionByChange = [](double chg) -> AdviceAction {
        if (chg >= 1.2) return AdviceAction::Increase;
        if (chg <= -1.2) return AdviceAction::Decrease;
        return AdviceAction::Hold;
    };

    // 建立混合工作列表（板块 + 指数）
    struct MixedRow {
        bool isIndex = false;
        int origIdx = 0; // 板块详情跳转索引；指数为0
        QString indexKey; // 指数详情跳转键
        const SectorSnapshot *s = nullptr;
        IndexSnapshot idx;
        QString sourceType; // 板块 / 指数
        QString trend;
        AdviceAction action = AdviceAction::Hold;
        double todayChange = 0;
        double forecast = 0;
        double confidence = 0;
        double dataQuality = 0;
        double consistency = 0;
        double fiveDay = 0;
    };
    QList<MixedRow> rows;
    const auto passFilter = [&](const QString &name, const QString &trend, AdviceAction action) -> bool {
        if (!searchText.isEmpty() && !name.contains(searchText, Qt::CaseInsensitive)) return false;
        if (!filterTrend.isEmpty() && trend != filterTrend) return false;
        if (!filterAction.isEmpty() && actionText(action) != filterAction) return false;
        return true;
    };
    for (int i = 0; i < analysis.sectors.size(); ++i) {
        const SectorSnapshot &s = analysis.sectors[i];
        if (!passFilter(s.industry, s.trendSummary, s.action)) continue;
        MixedRow r;
        r.isIndex = false;
        r.origIdx = i + 1;
        r.s = &s;
        r.sourceType = QString::fromUtf8("板块");
        r.trend = s.trendSummary;
        r.action = s.action;
        r.todayChange = s.todayChangePct;
        r.forecast = s.forecastScore;
        r.confidence = s.confidence;
        r.dataQuality = s.dataQualityScore;
        r.consistency = s.sourceConsistencyScore;
        r.fiveDay = s.fiveDayMomentum;
        rows.push_back(r);
    }
    auto appendIndex = [&](const IndexSnapshot &idx, const QString &key, const QString &label) {
        if (idx.name.isEmpty()) return;
        const QString trend = inferTrendByChange(idx.changePct);
        const AdviceAction act = inferActionByChange(idx.changePct);
        if (!passFilter(idx.name, trend, act)) return;
        double fiveDay = 0.0;
        if (idx.klineSeries.size() >= 6) {
            const double base = idx.klineSeries[idx.klineSeries.size() - 6];
            if (base > 0) fiveDay = (idx.klineSeries.last() - base) / base * 100.0;
        }
        MixedRow r;
        r.isIndex = true;
        r.indexKey = key;
        r.idx = idx;
        r.sourceType = QString::fromUtf8("指数");
        r.trend = trend;
        r.action = act;
        r.todayChange = idx.changePct;
        r.forecast = qBound(-1.0, idx.changePct / 3.0, 1.0);
        r.confidence = idx.klineSeries.size() >= 60 ? 92.0 : 82.0;
        r.dataQuality = idx.klineSeries.size() >= 60 ? 95.0 : (idx.klineSeries.size() >= 20 ? 85.0 : 70.0);
        r.consistency = idx.klineSeries.isEmpty() ? 80.0 : 95.0;
        r.fiveDay = fiveDay;
        Q_UNUSED(label);
        rows.push_back(r);
    };
    appendIndex(analysis.marketCtx.shanghai, "SH", QString::fromUtf8("A股"));
    appendIndex(analysis.marketCtx.shenzhen, "SZ", QString::fromUtf8("A股"));
    appendIndex(analysis.marketCtx.chinext, "CY", QString::fromUtf8("A股"));
    appendIndex(analysis.marketCtx.csi300, "CSI300", QString::fromUtf8("A股"));
    appendIndex(analysis.marketCtx.csi500, "CSI500", QString::fromUtf8("A股"));
    appendIndex(analysis.marketCtx.nasdaq, "NASDAQ", QString::fromUtf8("美股"));
    appendIndex(analysis.marketCtx.sp500, "SP500", QString::fromUtf8("美股"));
    appendIndex(analysis.marketCtx.dowjones, "DJI", QString::fromUtf8("美股"));

    // 排序
    auto rowForecast = [](const MixedRow &r) { return r.forecast; };
    auto rowToday = [](const MixedRow &r) { return r.todayChange; };
    auto rowConfidence = [](const MixedRow &r) { return r.confidence; };
    auto rowQuality = [](const MixedRow &r) { return r.dataQuality; };
    auto rowConsistency = [](const MixedRow &r) { return r.consistency; };
    switch (sortIdx) {
        case 1: std::sort(rows.begin(), rows.end(), [&](const MixedRow &a, const MixedRow &b){ return rowForecast(a) > rowForecast(b); }); break;
        case 2: std::sort(rows.begin(), rows.end(), [&](const MixedRow &a, const MixedRow &b){ return rowForecast(a) < rowForecast(b); }); break;
        case 3: std::sort(rows.begin(), rows.end(), [&](const MixedRow &a, const MixedRow &b){ return rowToday(a) > rowToday(b); }); break;
        case 4: std::sort(rows.begin(), rows.end(), [&](const MixedRow &a, const MixedRow &b){ return rowToday(a) < rowToday(b); }); break;
        case 5: std::sort(rows.begin(), rows.end(), [&](const MixedRow &a, const MixedRow &b){ return rowConfidence(a) > rowConfidence(b); }); break;
        case 6: std::sort(rows.begin(), rows.end(), [&](const MixedRow &a, const MixedRow &b){ return rowQuality(a) > rowQuality(b); }); break;
        case 7: std::sort(rows.begin(), rows.end(), [&](const MixedRow &a, const MixedRow &b){ return rowConsistency(a) > rowConsistency(b); }); break;
        default: break; // 默认热度（已由 fetchAll 排好）
    }

    // 搜索/过滤结果统计
    const bool isFiltered = !searchText.isEmpty() || trendIdx > 0 || actionIdx > 0 || sortIdx > 0;
    if (!simpleMode && !analysis.sectors.isEmpty()) {
        int missingAny = 0;
        int klineOk = 0;
        int flowOk = 0;
        int valuationOk = 0;
        double avgQuality = 0.0;
        double avgConsistency = 0.0;
        QStringList weakSectors;
        QStringList weakConsistency;
        for (const SectorSnapshot &s : analysis.sectors) {
            avgQuality += s.dataQualityScore;
            avgConsistency += s.sourceConsistencyScore;
            if (s.trendSeries.size() >= 20) ++klineOk;
            if (!s.fundFlowSeries.isEmpty()) ++flowOk;
            if (s.peRatio > 0 || s.pbRatio > 0) ++valuationOk;
            if (!s.missingDataItems.isEmpty()) {
                ++missingAny;
                if (weakSectors.size() < 5) {
                    weakSectors.push_back(s.industry + "（" + s.missingDataItems.join("、") + "）");
                }
            }
            if (s.sourceConsistencyScore < 70 && weakConsistency.size() < 5) {
                weakConsistency.push_back(s.industry + "（一致性" + num(s.sourceConsistencyScore, 0) + "）");
            }
        }
        avgQuality /= analysis.sectors.size();
        avgConsistency /= analysis.sectors.size();
        QString qClr = avgQuality >= 80 ? "#059669" : (avgQuality >= 60 ? "#D97706" : "#DC2626");
        QString cClr = avgConsistency >= 85 ? "#059669" : (avgConsistency >= 70 ? "#D97706" : "#DC2626");
        h += "<div style='margin:4px 0 8px 0;padding:10px 12px;border-radius:10px;border:1px solid " + t.cardBorder + ";background:" + t.narrativeBg + ";font-size:11px;line-height:1.7;'>";
        h += "数据审计：<b style='color:" + qClr + ";'>全局质量 " + num(avgQuality, 0) + "</b>"
            + " &nbsp;|&nbsp; <b style='color:" + cClr + ";'>源一致性 " + num(avgConsistency, 0) + "</b>"
            + " &nbsp;|&nbsp; K线覆盖 " + num(klineOk, 0) + "/" + num(analysis.sectors.size(), 0)
            + " &nbsp;|&nbsp; 资金流覆盖 " + num(flowOk, 0) + "/" + num(analysis.sectors.size(), 0)
            + " &nbsp;|&nbsp; 估值覆盖 " + num(valuationOk, 0) + "/" + num(analysis.sectors.size(), 0)
            + " &nbsp;|&nbsp; 缺口板块 " + num(missingAny, 0) + " 个";
        if (!weakSectors.isEmpty()) {
            h += "<br/><span style='color:#D97706;'>重点补齐：</span>" + weakSectors.join("；").toHtmlEscaped();
        }
        if (!weakConsistency.isEmpty()) {
            h += "<br/><span style='color:#DC2626;'>一致性偏弱：</span>" + weakConsistency.join("；").toHtmlEscaped();
        }
        h += "</div>";
    }
    const int indexCount =
        (!analysis.marketCtx.shanghai.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.shenzhen.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.chinext.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.csi300.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.csi500.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.nasdaq.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.sp500.name.isEmpty() ? 1 : 0) +
        (!analysis.marketCtx.dowjones.name.isEmpty() ? 1 : 0);
    const int totalUniverse = analysis.sectors.size() + indexCount;
    h += "<div class='section-title'>板块&指数总览";
    if (isFiltered)
        h += " <span style='font-size:11px;font-weight:400;color:" + t.mutedColor + ";'>" + QString::number(rows.size()) + "/" + QString::number(totalUniverse) + "</span>";
    h += "</div>";

    // 检测休市
    bool globalMarketClosed = false;
    QString globalLastDate;
    for (const SectorSnapshot &s : analysis.sectors) {
        if (s.marketClosed) { globalMarketClosed = true; globalLastDate = s.lastDataDate; break; }
    }
    if (globalMarketClosed) {
        h += "<div style='margin-bottom:8px;padding:6px 14px;background:" + t.narrativeBg + ";border:1px solid "
            + t.cardBorder + ";border-radius:8px;font-size:12px;color:" + t.mutedColor + ";'>"
            + QString::fromUtf8("当前处于休市/节假日状态，涨跌幅数据为上一交易日（")
            + globalLastDate + QString::fromUtf8("）收盘数据</div>");
    }

    const QString stratTodayCol = globalMarketClosed
        ? QString::fromUtf8("上一交易日") : QString::fromUtf8("今日");
    h += "<table class='overview'>";
    if (simpleMode) {
        h += "<tr>"
            "<th>#</th>"
            "<th>板块&指数</th>"
            "<th style='text-align:right;'>" + stratTodayCol + "</th>"
            "<th style='text-align:center;'>评分</th>"
            "<th style='text-align:center;'>建议</th>"
            "<th>一句话看点</th>"
            "</tr>";
    } else {
        h += "<tr>"
            "<th>#</th>"
            "<th>板块&指数</th>"
            "<th style='text-align:right;'>" + stratTodayCol + "</th>"
            "<th style='text-align:right;'>5日</th>"
            "<th style='text-align:center;'>评分</th>"
            "<th style='text-align:center;'>技术</th>"
            "<th style='text-align:center;'>估值</th>"
            "<th style='text-align:center;'>拥挤</th>"
            "<th style='text-align:center;'>数据质</th>"
            "<th style='text-align:center;'>一致性</th>"
            "<th style='text-align:center;'>MACD</th>"
            "<th>趋势</th>"
            "<th>建议</th>"
            "<th>策略</th>"
            "<th>周期</th>"
            "<th style='text-align:right;'>累计</th>"
            "<th>回测胜率</th>"
            "</tr>";
    }

    int displayIdx = 0;
    for (const MixedRow &row : rows) {
        ++displayIdx;
        const SectorSnapshot *ss = row.s;
        const SectorSnapshot &s = ss ? *ss : SectorSnapshot();
        const TechSignals &ts = ss ? s.tech : TechSignals();
        const QString rowName = row.isIndex ? row.idx.name : s.industry;
        const QString rowBg = (displayIdx % 2 == 0) ? " style='background:" + t.tableAltRowBg + ";'" : "";
        h += "<tr" + rowBg + ">";

        h += "<td style='color:" + t.subtleColor + ";text-align:center;'>" + QString::number(displayIdx) + "</td>";

        if (simpleMode) {
            // 简明行
            if (row.isIndex) {
                h += "<td><a href='jumpi-" + row.indexKey + "'>" + rowName + "</a>"
                    " <span style='font-size:10px;color:#7C3AED;'>[指数]</span></td>";
            } else {
                h += "<td><a href='jump-" + QString::number(row.origIdx) + "'>" + rowName + "</a>"
                + (s.personalAdvice.isEmpty() ? "" : " <span style='font-size:10px;color:#2e7d32;'>&#127981;</span>")
                + "</td>";
            }
            h += "<td style='text-align:right;color:" + clr(row.todayChange) + ";font-weight:700;'>" + pct(row.todayChange) + "</td>";
            h += "<td style='text-align:center;'>" + scoreBar(row.forecast) + "</td>";
            h += "<td style='text-align:center;'><span class='tag " + tagClass(row.action) + "'>" + actionText(row.action) + "</span></td>";

            // 一句话看点
            QString oneliner;
            if (row.isIndex) {
                oneliner = row.trend + QString::fromUtf8("，建议以指数方向管理相关仓位与风险敞口");
            } else if (s.action == AdviceAction::Increase) {
                oneliner = s.trendSummary + "，" + s.strategy.actionLabel;
                if (ts.macdGoldenCross) oneliner += "，MACD金叉";
                if (ts.maLongArrange) oneliner += "，均线多头";
            } else if (s.action == AdviceAction::Decrease) {
                oneliner = s.trendSummary + "，" + s.strategy.actionLabel;
                if (ts.macdDeadCross) oneliner += "，MACD死叉";
                if (ts.maShortArrange) oneliner += "，均线空头";
            } else {
                oneliner = s.trendSummary + "，" + s.strategy.actionLabel;
            }
            if (!row.isIndex && s.newsHitCount > 3) oneliner += "（" + num(s.newsHitCount, 0) + "条新闻）";
            h += "<td style='font-size:11px;'>" + oneliner + "</td>";
        } else {
            // 专业行
            if (row.isIndex) {
                h += "<td><a href='jumpi-" + row.indexKey + "'>" + rowName + "</a>"
                    " <span style='font-size:10px;color:#7C3AED;'>[指数]</span></td>";
            } else {
                h += "<td><a href='jump-" + QString::number(row.origIdx) + "'>" + rowName + "</a>"
                    + " <span style='font-size:9px;color:" + t.mutedColor + ";'>[" + s.sectorTierLabel + "]</span>"
                    + (s.personalAdvice.isEmpty() ? "" : " <span style='font-size:10px;color:#059669;'>&#9679;</span>")
                    + (!s.missingDataItems.isEmpty() ? " <span style='font-size:10px;color:#D97706;' title='数据存在缺口'>&#9888;</span>" : "")
                    + "</td>";
            }
            {
                const bool pctOk = row.s ? row.s->todayChangePctValid : true;
                if (pctOk)
                    h += "<td style='text-align:right;color:" + clr(row.todayChange) + ";font-weight:700;'>" + pct(row.todayChange) + "</td>";
                else
                    h += "<td style='text-align:right;color:" + t.mutedColor + ";font-size:11px;'>—</td>";
            }
            h += "<td style='text-align:right;color:" + clr(row.fiveDay) + ";font-weight:700;'>" + pct(row.fiveDay) + "</td>";
            h += "<td style='text-align:center;'>" + scoreBar(row.forecast) + "</td>";
            h += "<td style='text-align:center;'>" + (row.isIndex ? "-" : scoreBar(ts.techScore)) + "</td>";

            // 估值分位
            {
                QString valClr = "#6B7280";
                if (s.pePercentile < 30) valClr = "#059669";
                else if (s.pePercentile > 70) valClr = "#DC2626";
                h += "<td style='text-align:center;font-size:11px;color:" + valClr + ";font-weight:600;'>"
                    + (!row.isIndex && s.peRatio > 0 ? num(s.pePercentile, 0) + "%" : "-") + "</td>";
            }
            // 拥挤度
            {
                QString crowdClr = "#6B7280";
                if (s.crowdingIndex > 70) crowdClr = "#DC2626";
                else if (s.crowdingIndex < 30) crowdClr = "#059669";
                h += "<td style='text-align:center;font-size:11px;color:" + crowdClr + ";font-weight:600;'>"
                    + (row.isIndex ? "-" : num(s.crowdingIndex, 0) + "%") + "</td>";
            }
            // 数据质量
            {
                QString qClr = "#DC2626";
                if (row.dataQuality >= 80) qClr = "#059669";
                else if (row.dataQuality >= 60) qClr = "#D97706";
                h += "<td style='text-align:center;font-size:11px;color:" + qClr + ";font-weight:700;' title='"
                    + (row.isIndex ? QString::fromUtf8("指数行情源数据质量") : s.dataQualityNote).toHtmlEscaped() + "'>"
                    + num(row.dataQuality, 0) + "</td>";
            }
            {
                QString cClr = "#DC2626";
                if (row.consistency >= 85) cClr = "#059669";
                else if (row.consistency >= 70) cClr = "#D97706";
                h += "<td style='text-align:center;font-size:11px;color:" + cClr + ";font-weight:700;' title='跨数据源一致性评分'>"
                    + num(row.consistency, 0) + "</td>";
            }

            QString macdIcon = row.isIndex ? "-"
                : (ts.macdGoldenCross ? "<span style='color:#DC2626;font-weight:700;'>&#9650;金叉</span>"
                : (ts.macdDeadCross ? "<span style='color:#2563EB;font-weight:700;'>&#9660;死叉</span>"
                : (ts.macdHist > 0 ? "<span style='color:#EF4444;'>&#9650;多</span>" : "<span style='color:#3B82F6;'>&#9660;空</span>")));
            h += "<td style='text-align:center;font-size:11px;'>" + macdIcon + "</td>";

            h += "<td style='font-size:11px;'>" + (row.isIndex ? row.trend : s.trendSummary) + "</td>";
            h += "<td><span class='tag " + tagClass(row.action) + "'>" + actionText(row.action) + "</span></td>";
            h += "<td style='font-size:11px;'>" + (row.isIndex ? QString::fromUtf8("跟踪指数方向，管理相关仓位") : s.strategy.actionLabel) + "</td>";
            {
                static const QStringList cycColors = {"#3B82F6", "#059669", "#F59E0B", "#EF4444", "#8B5CF6"};
                const bool hasCycleInfo = !row.isIndex && !s.cycle.phaseName.isEmpty();
                const QString cyPhase = hasCycleInfo
                    ? "<span style='font-size:10px;font-weight:600;color:" + cycColors[s.cycle.phaseIndex] + ";'>" + s.cycle.phaseName + "</span>"
                    : "<span style='font-size:10px;color:" + t.mutedColor + ";'>-</span>";
                h += "<td>" + cyPhase + "</td>";
            }
            {
                const QString cumClr = s.cumulativeReturn >= 0 ? "#EF4444" : "#3B82F6";
                h += "<td style='text-align:right;font-size:11px;font-weight:600;color:" + cumClr + ";'>"
                    + (!row.isIndex && s.trackingDays > 0 ? ((s.cumulativeReturn >= 0 ? "+" : "") + num(s.cumulativeReturn, 1) + "%") : "-") + "</td>";
            }
            {
                const QString wrClr = s.bestStrategyWinRate >= 60 ? "#059669" : (s.bestStrategyWinRate >= 40 ? t.bodyColor : "#DC2626");
                h += "<td style='font-size:10px;'>"
                    + (row.isIndex || s.bestStrategyName.isEmpty() ? "-" :
                        "<span style='color:" + wrClr + ";font-weight:600;'>" + num(s.bestStrategyWinRate, 0) + "%</span>"
                        " <span style='color:" + t.mutedColor + ";'>" + s.bestStrategyName + "</span>") + "</td>";
            }
        }
        h += "</tr>";
    }
    h += "</table>";

    h += "</body></html>";
    return h;
}

// ========== 子Tab3: 投资策略 ==========
QString MainWindow::buildStrategyHtml(const AnalysisResult &analysis) const
{
    const ThemeColors &t = *s_theme;
    QString h = "<html><head><style>" + UiTheme::buildHtmlCss(t) + "</style></head><body>";
    const bool simpleMode = m_viewMode && m_viewMode->currentIndex() == 0;

    auto inferTrendByChange = [](double chg) -> QString {
        if (chg >= 2.0) return QString::fromUtf8("强势看多");
        if (chg >= 0.5) return QString::fromUtf8("偏多");
        if (chg <= -2.0) return QString::fromUtf8("强势看空");
        if (chg <= -0.5) return QString::fromUtf8("偏空");
        if (std::abs(chg) <= 0.2) return QString::fromUtf8("横盘震荡");
        return QString::fromUtf8("方向不明");
    };
    auto inferActionByChange = [](double chg) -> AdviceAction {
        if (chg >= 1.2) return AdviceAction::Increase;
        if (chg <= -1.2) return AdviceAction::Decrease;
        return AdviceAction::Hold;
    };
    struct StrategyItem {
        bool isIndex = false;
        int origIdx = 0;
        QString indexKey;
        QString name;
        AdviceAction action = AdviceAction::Hold;
        double forecastScore = 0.0;
        double todayChangePct = 0.0;
        QString trendSummary;
        QString strategyLabel;
        const SectorSnapshot *sector = nullptr;
    };
    QList<StrategyItem> rows;
    for (int i = 0; i < analysis.sectors.size(); ++i) {
        const SectorSnapshot &s = analysis.sectors[i];
        rows.push_back({false, i + 1, QString(), s.industry, s.action, s.forecastScore,
            s.todayChangePct, s.trendSummary, s.strategy.actionLabel, &s});
    }
    auto appendIndex = [&](const IndexSnapshot &idx, const QString &key) {
        if (idx.name.isEmpty()) return;
        rows.push_back({true, 0, key, idx.name, inferActionByChange(idx.changePct),
            qBound(-1.0, idx.changePct / 3.0, 1.0), idx.changePct,
            inferTrendByChange(idx.changePct), QString::fromUtf8("跟踪指数方向，管理相关仓位"), nullptr});
    };
    appendIndex(analysis.marketCtx.shanghai, "SH");
    appendIndex(analysis.marketCtx.shenzhen, "SZ");
    appendIndex(analysis.marketCtx.chinext, "CY");
    appendIndex(analysis.marketCtx.csi300, "CSI300");
    appendIndex(analysis.marketCtx.csi500, "CSI500");
    appendIndex(analysis.marketCtx.nasdaq, "NASDAQ");
    appendIndex(analysis.marketCtx.sp500, "SP500");
    appendIndex(analysis.marketCtx.dowjones, "DJI");

    // ---- 市场操作建议板块 ----
    if (rows.size() >= 5) {
        h += "<hr class='divider'/>";
        h += "<div class='section-title'>市场操作建议</div>";

        // 板块和指数分开排序，避免指数挤占板块推荐
        QList<StrategyItem> sectorSorted, indexSorted;
        for (const StrategyItem &it : rows) {
            if (it.isIndex) indexSorted.push_back(it);
            else sectorSorted.push_back(it);
        }
        std::sort(sectorSorted.begin(), sectorSorted.end(), [](const StrategyItem &a, const StrategyItem &b) {
            return a.forecastScore > b.forecastScore;
        });
        std::sort(indexSorted.begin(), indexSorted.end(), [](const StrategyItem &a, const StrategyItem &b) {
            return a.forecastScore > b.forecastScore;
        });

        int bullCount = 0, bearCount = 0;
        for (const StrategyItem &it : sectorSorted) {
            if (it.forecastScore > 0.1) ++bullCount;
            if (it.forecastScore < -0.1) ++bearCount;
        }
        QString marketTone;
        if (bullCount > bearCount * 2) marketTone = "当前市场整体偏多，多数板块技术面和资金面向好，可适度积极参与。";
        else if (bearCount > bullCount * 2) marketTone = "当前市场整体偏空，多数板块面临调整压力，建议保持谨慎，控制仓位。";
        else marketTone = "当前市场多空交织，热点分散，建议精选板块、控制仓位，等待市场方向明确。";

        h += "<div class='narrative'>" + marketTone + "</div>";

        h += "<div style='display:flex;gap:16px;margin-top:8px;'>";

        h += "<div style='flex:1;'>";
        h += "<div style='font-size:12px;font-weight:700;color:#059669;margin-bottom:6px;'>&#9650; 推荐关注板块 Top5</div>";
        h += "<table class='fund'><tr><th>板块</th><th>评分</th><th>策略</th></tr>";
        for (int i = 0; i < qMin(5, sectorSorted.size()); ++i) {
            const StrategyItem &it = sectorSorted[i];
            const QString nameHtml = "<a href='jump-" + QString::number(it.origIdx) + "'>" + it.name + "</a>";
            h += "<tr><td>" + nameHtml + "</td><td style='color:" + clr(it.forecastScore) + ";font-weight:700;'>" + num(it.forecastScore) + "</td>"
                "<td style='font-size:11px;'>" + it.strategyLabel + "</td></tr>";
        }
        h += "</table></div>";

        h += "<div style='flex:1;'>";
        h += "<div style='font-size:12px;font-weight:700;color:#DC2626;margin-bottom:6px;'>&#9660; 建议回避板块 Top5</div>";
        h += "<table class='fund'><tr><th>板块</th><th>评分</th><th>策略</th></tr>";
        for (int i = sectorSorted.size() - 1; i >= qMax(0, sectorSorted.size() - 5); --i) {
            const StrategyItem &it = sectorSorted[i];
            const QString nameHtml = "<a href='jump-" + QString::number(it.origIdx) + "'>" + it.name + "</a>";
            h += "<tr><td>" + nameHtml + "</td><td style='color:" + clr(it.forecastScore) + ";font-weight:700;'>" + num(it.forecastScore) + "</td>"
                "<td style='font-size:11px;'>" + it.strategyLabel + "</td></tr>";
        }
        h += "</table></div>";

        h += "</div>";

        // 指数方向参考（独立展示）
        if (!indexSorted.isEmpty()) {
            h += "<div style='margin-top:10px;'>";
            h += "<div style='font-size:12px;font-weight:700;color:#7C3AED;margin-bottom:6px;'>&#9679; 指数方向参考</div>";
            h += "<table class='fund'><tr><th>指数</th><th>评分</th><th>方向</th></tr>";
            for (const StrategyItem &it : indexSorted) {
                const QString nameHtml = "<a href='jumpi-" + it.indexKey + "'>" + it.name + "</a>";
                h += "<tr><td>" + nameHtml + "</td><td style='color:" + clr(it.forecastScore) + ";font-weight:700;'>" + num(it.forecastScore) + "</td>"
                    "<td style='font-size:11px;'>" + it.strategyLabel + "</td></tr>";
            }
            h += "</table></div>";
        }

        h += "<div style='margin-top:8px;padding:10px 14px;border:1px solid " + t.cardBorder + ";border-radius:10px;font-size:12px;background:" + t.narrativeBg + ";'>"
            "<b style='color:" + t.sectionTitleColor + ";'>仓位建议</b> &nbsp;";
        if (bullCount > bearCount * 2)
            h += "激进型 70-80%，稳健型 50-60%，保守型 30-40%";
        else if (bearCount > bullCount * 2)
            h += "激进型 40-50%，稳健型 30-40%，保守型 10-20%";
        else
            h += "激进型 50-60%，稳健型 40-50%，保守型 20-30%";
        h += "</div>";

        const QString posClr = "#EF4444";
        const QString negClr = "#3B82F6";

        // ---- 持仓诊断（读取用户批次数据，支持股票/ETF/基金类型区分）----
        {
            QSettings ps("InvestInsight", "InvestInsight");
            const QString batchKey = ps.contains("portfolio/batches_json")
                ? "portfolio/batches_json" : "portfolio/entries_json";
            const QJsonDocument pfDoc = QJsonDocument::fromJson(
                ps.value(batchKey).toString().toUtf8());
            if (pfDoc.isArray() && !pfDoc.array().isEmpty()) {
                h += "<hr class='divider'/>";
                h += "<div class='section-title'>&#128200; 我的持仓诊断</div>";

                // 交易规则说明（一次性）
                h += "<div style='padding:8px 14px;margin-bottom:8px;background:" + t.narrativeBg + ";"
                     "border-radius:8px;border:1px solid " + t.cardBorder + ";font-size:11px;color:" + t.mutedColor + ";'>"
                     "<b style='color:" + t.bodyColor + ";'>交易规则说明</b>&nbsp;&nbsp;"
                     "&#9679;&nbsp;<b>股票/ETF</b>：实时成交，T+1可卖出，价格同步反映；"
                     "&nbsp;&nbsp;&#9679;&nbsp;<b>普通基金</b>：15:00前申购按当日净值成交（T+1确认），赎回T+3~7个工作日到账，"
                     "净值变动滞后1个交易日。</div>";

                // 计算首次收益日（同 UI 逻辑）
                auto calcEarnDate = [](const QString &buyDate, const QString &tp) -> QDate {
                    QDate d = QDate::fromString(buyDate, "yyyy-MM-dd");
                    if (!d.isValid()) return QDate::currentDate();
                    if (tp == QString::fromUtf8("基金")) {
                        d = d.addDays(1);
                        while (d.dayOfWeek() > 5) d = d.addDays(1);
                    }
                    return d;
                };

                // 数据结构：持仓类型为板块级别
                struct HoldInfo {
                    struct Batch {
                        QString label;
                        double  amount   = 0;
                        QString holdType;
                        QDate   earnDate; // 首次收益日
                        bool    confirmed = true; // earnDate <= today
                    };
                    QList<Batch> batches;
                    double totalInvested      = 0;
                    double confirmedInvested  = 0; // earnDate 已到期的金额
                    QString dominantType;
                    QMap<QString,double> typeAmounts;
                    QString latestDate;
                };
                const QDate today = QDate::currentDate();
                QMap<QString, HoldInfo> holdMap;
                for (const QJsonValue &v : pfDoc.array()) {
                    const QJsonObject o = v.toObject();
                    const QString sector   = o.value("sector").toString();
                    if (sector.isEmpty()) continue;
                    if (o.value("sold").toBool(false)) continue;
                    const double  amt      = o.value("amount").toDouble();
                    const QString holdType = o.value("holdType").toString(QString::fromUtf8("基金"));
                    const QString dt       = o.value("date").toString();
                    const QString firstEarnDateStr = o.value("firstEarnDate").toString();
                    const QString rem      = o.value("remark").toString();
                    const QString label    = dt.isEmpty() ? rem : (rem.isEmpty() ? dt : dt + " " + rem);
                    QDate earnDate = QDate::fromString(firstEarnDateStr, "yyyy-MM-dd");
                    if (!earnDate.isValid()) earnDate = calcEarnDate(dt, holdType);
                    const bool    confirmed = (earnDate <= today);
                    holdMap[sector].batches.push_back({label, amt, holdType, earnDate, confirmed});
                    holdMap[sector].totalInvested += amt;
                    if (confirmed) holdMap[sector].confirmedInvested += amt;
                    holdMap[sector].typeAmounts[holdType] += amt;
                    if (holdMap[sector].latestDate < dt) holdMap[sector].latestDate = dt;
                }
                // 确定每个板块的主要持仓类型（金额最大的类型）
                for (auto it = holdMap.begin(); it != holdMap.end(); ++it) {
                    HoldInfo &hi = it.value();
                    double maxAmt = -1;
                    for (auto tit = hi.typeAmounts.cbegin(); tit != hi.typeAmounts.cend(); ++tit) {
                        if (tit.value() > maxAmt) { maxAmt = tit.value(); hi.dominantType = tit.key(); }
                    }
                }

                const QString fundType  = QString::fromUtf8("基金");
                const QString etfType   = QString::fromUtf8("ETF");
                const QString stockType = QString::fromUtf8("股票");

                // 按持仓金额降序
                QList<QString> holdSectors = holdMap.keys();
                std::sort(holdSectors.begin(), holdSectors.end(),
                    [&holdMap](const QString &a, const QString &b){
                        return holdMap[a].totalInvested > holdMap[b].totalInvested;
                    });

                // 检测休市
                bool stratMarketClosed = false;
                QString stratLastDate;
                for (const SectorSnapshot &s : analysis.sectors) {
                    if (s.marketClosed) { stratMarketClosed = true; stratLastDate = s.lastDataDate; break; }
                }
                const QString stratTodayHeader = stratMarketClosed
                    ? QString::fromUtf8("参考变动（休市）") : QString::fromUtf8("今日参考变动");

                h += "<table class='fund'><tr>"
                     "<th>" + QString::fromUtf8("板块") + "</th>"
                     "<th>" + QString::fromUtf8("类型") + "</th>"
                     "<th>" + QString::fromUtf8("买入批次") + "</th>"
                     "<th>" + QString::fromUtf8("总投入（元）") + "</th>"
                     "<th>" + stratTodayHeader + "</th>"
                     "<th>" + QString::fromUtf8("预测评分") + "</th>"
                     "<th>" + QString::fromUtf8("建议操作") + "</th>"
                     "</tr>";

                for (const QString &sector : holdSectors) {
                    const HoldInfo &hi = holdMap[sector];
                    const bool isFund  = (hi.dominantType == fundType);
                    const bool isETF   = (hi.dominantType == etfType);

                    const SectorSnapshot *ss = nullptr;
                    for (const SectorSnapshot &s : analysis.sectors) {
                        if (s.industry == sector) { ss = &s; break; }
                    }

                    const double todayChangePct = ss ? ss->todayChangePct : 0.0;
                    const bool todayPctOk = ss && ss->todayChangePctValid;
                    const double confirmedAmt   = hi.confirmedInvested;
                    const double pendingAmt     = hi.totalInvested - confirmedAmt;
                    const double todayPnl       = (stratMarketClosed || !todayPctOk) ? 0.0 : (confirmedAmt * todayChangePct / 100.0);
                    const double cumulReturn    = ss ? ss->cumulativeReturn : 0.0;
                    const double cumulPnl       = confirmedAmt * cumulReturn / 100.0;
                    const QString pnlClr        = todayPnl >= 0 ? posClr : negClr;
                    const QString cumulPnlClr   = cumulPnl >= 0 ? posClr : negClr;

                    QString pnlStr;
                    if (stratMarketClosed) {
                        pnlStr = QString::fromUtf8("<span style='color:") + t.mutedColor + ";'>"
                            + QString::fromUtf8("休市") + "</span>";
                        if (confirmedAmt > 0) {
                            pnlStr += "&nbsp;&nbsp;<b>累计</b> <span style='color:" + cumulPnlClr + ";'>"
                                + (cumulPnl >= 0 ? QString("+") : QString(""))
                                + QString::number(cumulPnl, 'f', 2) + " ¥"
                                + " (" + (cumulReturn >= 0 ? "+" : "")
                                + QString::number(cumulReturn, 'f', 2) + "%)</span>";
                        }
                    } else if (pendingAmt > 0 && confirmedAmt <= 0) {
                        pnlStr = QString::fromUtf8("<span style='color:#D97706;'>全部待确认</span>");
                    } else if (!todayPctOk) {
                        pnlStr = QString::fromUtf8("<b>今日</b> <span style='color:") + t.mutedColor + ";'>"
                            + QString::fromUtf8("涨跌数据缺失") + "</span>";
                    } else {
                        pnlStr = "<b>今日</b> " + (todayPnl >= 0 ? QString("+") : QString(""))
                            + QString::number(todayPnl, 'f', 2) + " ¥"
                            + " (" + (todayChangePct >= 0 ? "+" : "")
                            + QString::number(todayChangePct, 'f', 2) + "%)";
                        pnlStr += "&nbsp;&nbsp;<b>累计</b> <span style='color:" + cumulPnlClr + ";'>"
                            + (cumulPnl >= 0 ? QString("+") : QString(""))
                            + QString::number(cumulPnl, 'f', 2) + " ¥"
                            + " (" + (cumulReturn >= 0 ? "+" : "")
                            + QString::number(cumulReturn, 'f', 2) + "%)</span>";
                        if (isFund) pnlStr += QString::fromUtf8(
                            " <span style='color:#D97706;font-size:10px;'>净值T+1</span>");
                        if (pendingAmt > 0)
                            pnlStr += QString::fromUtf8(" <span style='color:#D97706;font-size:10px;'>+¥")
                                + QString::number(pendingAmt, 'f', 0)
                                + QString::fromUtf8("待确认</span>");
                    }

                    // 批次汇总，标注首次收益日和确认状态
                    QString batchStr;
                    for (const auto &b : hi.batches) {
                        if (!batchStr.isEmpty()) batchStr += "<br/>";
                        QString statusTag;
                        if (!b.confirmed) {
                            const QString earnStr = b.earnDate.toString("MM-dd");
                            if (b.holdType == fundType) {
                                statusTag = QString::fromUtf8(
                                    " <span style='color:#D97706;font-size:9px;'>[基金申购中，") +
                                    earnStr + QString::fromUtf8("起计收益]</span>");
                            } else {
                                statusTag = QString::fromUtf8(
                                    " <span style='color:#9CA3AF;font-size:9px;'>[待确认 ")
                                    + earnStr + "]</span>";
                            }
                        } else if (b.earnDate == today) {
                            statusTag = QString::fromUtf8(
                                " <span style='color:#059669;font-size:9px;'>[今日首次计收益]</span>");
                        }
                        QString typeTag;
                        if (hi.batches.size() > 1) {
                            // 多批次时显示每批类型
                            typeTag = " <span style='font-size:9px;color:" + t.mutedColor
                                + ";'>" + b.holdType + "</span>";
                        }
                        batchStr += b.label.toHtmlEscaped() + "&nbsp;¥"
                            + QString::number(b.amount, 'f', 0) + typeTag + statusTag;
                    }

                    // 类型标签样式
                    QString typeBadgeClr = "#4F46E5";
                    if (isFund)      typeBadgeClr = "#D97706";
                    else if (isETF)  typeBadgeClr = "#059669";
                    const QString typeBadge = "<span style='padding:1px 7px;border-radius:8px;background:"
                        + typeBadgeClr + ";color:#fff;font-size:10px;font-weight:700;'>"
                        + hi.dominantType + "</span>";

                    // 建议操作
                    const QString adviceStr = ss ? ss->strategy.actionLabel : QString::fromUtf8("—");
                    const double  fScore    = ss ? ss->forecastScore : 0.0;
                    const QString advClr    = clr(fScore);

                    // 类型感知 + 延迟感知的定制建议
                    QString personalAdvice;
                    const bool allPending = (confirmedAmt <= 0 && pendingAmt > 0);
                    const bool hasPending = (pendingAmt > 0);

                    if (allPending) {
                        // 全部批次未到首次收益日
                        if (isFund) {
                            personalAdvice = QString::fromUtf8(
                                "【申购确认中】基金申购已提交，T+1 工作日确认后方可计算收益；"
                                "在此期间无法赎回，请耐心等待确认。");
                            if (fScore < -0.08)
                                personalAdvice += QString::fromUtf8(
                                    "当前板块预测偏弱，确认到账后请密切观察净值走势，"
                                    "若持续下跌及时评估赎回计划（赎回需 T+3~7 工作日到账）。");
                        } else {
                            personalAdvice = QString::fromUtf8(
                                "【待确认】持仓尚未生效，T+1 后方可操作。今日数据仅供跟踪参考。");
                        }
                    } else if (ss) {
                        // 基础建议（按预测评分）
                        if (fScore > 0.12) {
                            personalAdvice = QString::fromUtf8("预测强势，建议继续持有，止损参考 −")
                                + num(ss->strategy.stopLossPct, 1) + "%";
                        } else if (fScore < -0.12) {
                            personalAdvice = QString::fromUtf8("预测偏弱，建议减仓或止损，支撑参考 ¥")
                                + num(ss->strategy.supportLevel1, 2);
                        } else if (todayChangePct < -2.0) {
                            personalAdvice = QString::fromUtf8("今日跌幅较大，注意观察量能，若放量下跌考虑减仓");
                        } else {
                            personalAdvice = ss->strategy.operationAdvice;
                        }
                        // 追加类型特有注意事项
                        if (isFund) {
                            personalAdvice += QString::fromUtf8(
                                "【基金提示】今日净值变动 T+1 才能确认；"
                                "如需赎回请注意 T+3~7 工作日到账，市场剧烈波动时需提前规划流动性。");
                            if (fScore < -0.08)
                                personalAdvice += QString::fromUtf8(
                                    "当前评分偏低，若打算止损请尽早提交赎回，避免因延迟扩大损失。");
                        } else if (isETF) {
                            personalAdvice += QString::fromUtf8(
                                "【ETF提示】支持盘中实时买卖，止损灵活；"
                                "注意避免在开盘/收盘前15分钟流动性较差时段大额交易。");
                        } else {
                            personalAdvice += QString::fromUtf8(
                                "【股票提示】T+1 制度，今日买入明日方可卖出；止损单可预挂，注意缺口风险。");
                        }
                        if (hasPending && isFund)
                            personalAdvice += QString::fromUtf8(
                                "注：部分批次仍在申购确认中，待确认资金暂未计入今日收益。");
                    } else {
                        personalAdvice = QString::fromUtf8("该板块暂无分析数据，建议手动查阅最新行情。");
                        if (isFund) personalAdvice += QString::fromUtf8("基金净值 T+1 更新，今日数据仅供参考。");
                    }

                    h += "<tr>"
                         "<td style='font-weight:700;'>" + sector + "</td>"
                         "<td>" + typeBadge + "</td>"
                         "<td style='font-size:11px;line-height:1.5;'>" + batchStr + "</td>"
                         "<td style='text-align:right;'>¥" + QString::number(hi.totalInvested, 'f', 0) + "</td>"
                         "<td style='color:" + pnlClr + ";font-weight:700;text-align:right;'>" + pnlStr + "</td>"
                         "<td style='color:" + advClr + ";font-weight:700;text-align:center;'>" + num(fScore) + "</td>"
                         "<td style='font-size:11px;color:" + advClr + ";font-weight:600;'>" + adviceStr + "</td>"
                         "</tr>"
                         "<tr><td colspan='7' style='font-size:11px;color:" + t.mutedColor + ";"
                         "padding:2px 10px 8px;line-height:1.6;border-bottom:1px solid " + t.tableBorder + ";'>"
                         + personalAdvice + "</td></tr>";
                }
                h += "</table>";

                // 整体持仓汇总
                double totalInvest = 0, totalConfirmed = 0, totalPending = 0, totalTodayPnl = 0;
                double stockAmount = 0, fundAmount = 0;
                for (const QString &s : holdSectors) {
                    totalInvest    += holdMap[s].totalInvested;
                    totalConfirmed += holdMap[s].confirmedInvested;
                    totalPending   += (holdMap[s].totalInvested - holdMap[s].confirmedInvested);
                    if (!stratMarketClosed) {
                        const SectorSnapshot *ss = nullptr;
                        for (const SectorSnapshot &sec : analysis.sectors)
                            if (sec.industry == s) { ss = &sec; break; }
                        if (ss) totalTodayPnl += holdMap[s].confirmedInvested * ss->todayChangePct / 100.0;
                    }
                    if (holdMap[s].dominantType == stockType || holdMap[s].dominantType == etfType)
                        stockAmount += holdMap[s].totalInvested;
                    else
                        fundAmount += holdMap[s].totalInvested;
                }
                const QString sumClr = stratMarketClosed ? t.mutedColor : (totalTodayPnl >= 0 ? posClr : negClr);
                const double pctChg  = totalConfirmed > 0 ? totalTodayPnl / totalConfirmed * 100.0 : 0.0;
                h += "<div style='display:flex;flex-wrap:wrap;gap:12px;margin-top:8px;padding:10px 14px;"
                     "background:" + t.narrativeBg + ";border-radius:10px;"
                     "border:1px solid " + t.cardBorder + ";font-size:12px;'>"
                     "<span>" + QString::fromUtf8("总投入：") + "<b>¥" + QString::number(totalInvest, 'f', 0) + "</b></span>";
                if (totalPending > 0)
                    h += "<span style='color:#D97706;'>" + QString::fromUtf8("待确认：")
                         + "<b>¥" + QString::number(totalPending, 'f', 0) + "</b>"
                         + QString::fromUtf8("（申购中/T+1未到）</span>");
                if (stratMarketClosed) {
                    h += "<span style='color:" + t.mutedColor + ";'>"
                         + QString::fromUtf8("当日休市，数据截至 ") + stratLastDate + "</span>";
                } else {
                    h += "<span>" + QString::fromUtf8("已确认参考变动：") + "<b style='color:" + sumClr + ";'>"
                         + (totalTodayPnl >= 0 ? "+" : "") + QString::number(totalTodayPnl, 'f', 2) + " ¥";
                    if (totalConfirmed > 0)
                        h += QString(" (") + (pctChg >= 0 ? "+" : "") + QString::number(pctChg, 'f', 2) + "%)";
                    h += "</b></span>";
                }
                if (fundAmount > 0)
                    h += "<span style='color:#D97706;'>" + QString::fromUtf8("基金类：")
                         + "<b>¥" + QString::number(fundAmount, 'f', 0) + "</b>"
                         + QString::fromUtf8("（净值T+1确认，赎回T+3~7到账）") + "</span>";
                if (stockAmount > 0)
                    h += "<span style='color:#059669;'>" + QString::fromUtf8("股票/ETF类：")
                         + "<b>¥" + QString::number(stockAmount, 'f', 0) + "</b>"
                         + QString::fromUtf8("（实时成交，T+1可卖出）") + "</span>";
                h += "</div>";
            }
        }

        // ---- 整体投资策略 ----
        h += "<hr class='divider'/>";
        h += "<div class='section-title'>&#128202; 整体投资策略</div>";
        {
            int increaseN = 0, holdN = 0, decreaseN = 0;
            double avgForecast = 0;
            QStringList topBuy, topSell;
            // 仅用板块数据统计，指数单独展示方向参考
            for (const StrategyItem &it : sectorSorted) {
                if (it.action == AdviceAction::Increase) ++increaseN;
                else if (it.action == AdviceAction::Decrease) ++decreaseN;
                else ++holdN;
                avgForecast += it.forecastScore;
            }
            avgForecast /= qMax(1, sectorSorted.size());

            for (int i = 0; i < qMin(3, sectorSorted.size()); ++i) {
                if (sectorSorted[i].forecastScore > 0.05)
                    topBuy.push_back(sectorSorted[i].name);
            }
            for (int i = sectorSorted.size() - 1; i >= qMax(0, sectorSorted.size() - 3); --i) {
                if (sectorSorted[i].forecastScore < -0.05)
                    topSell.push_back(sectorSorted[i].name);
            }

            QString strategyLevel;
            QString strategyDetail;
            if (avgForecast > 0.08 && bullCount > bearCount * 2) {
                strategyLevel = "积极进攻";
                strategyDetail = "市场整体偏多，技术面与资金面共振向上。建议以进攻为主，重仓配置强势板块，轻仓参与波段操作。";
            } else if (avgForecast < -0.08 && bearCount > bullCount * 2) {
                strategyLevel = "防御收缩";
                strategyDetail = "市场整体偏空，建议大幅降低仓位，以现金或债券为主，只保留极少数强势板块的底仓。";
            } else if (avgForecast > 0.03) {
                strategyLevel = "稳健偏多";
                strategyDetail = "市场略偏积极，建议维持中等仓位，优先配置评分靠前的板块，同时设置好止损线。";
            } else if (avgForecast < -0.03) {
                strategyLevel = "谨慎偏空";
                strategyDetail = "市场存在调整压力，建议降低仓位至半仓以下，规避弱势板块，关注超跌反弹机会。";
            } else {
                strategyLevel = "均衡配置";
                strategyDetail = "市场方向不明，多空力量均衡。建议以观望为主，少量参与确定性较高的板块，严格执行止盈止损。";
            }

            QString slClr = avgForecast > 0.03 ? "#059669" : (avgForecast < -0.03 ? "#DC2626" : "#D97706");
            h += "<div style='padding:12px 16px;border-radius:10px;background:" + t.narrativeBg + ";border:1px solid " + t.cardBorder + ";margin-bottom:10px;'>"
                "<div style='display:flex;align-items:center;gap:10px;margin-bottom:8px;'>"
                "<span style='padding:4px 14px;border-radius:16px;background:" + slClr + ";color:#fff;font-weight:700;font-size:13px;'>"
                + strategyLevel + "</span>"
                "<span style='font-size:11px;color:" + t.mutedColor + ";'>全市场均值评分 " + num(avgForecast) + " | 增配/持有/减配 = "
                + QString::number(increaseN) + "/" + QString::number(holdN) + "/" + QString::number(decreaseN) + "</span></div>"
                "<div style='font-size:12px;line-height:1.7;color:" + t.bodyColor + ";'>" + strategyDetail + "</div></div>";

            h += "<table style='width:100%;border-collapse:separate;border-spacing:8px 0;'><tr>";
            h += "<td style='width:50%;vertical-align:top;'>";
            h += "<div style='padding:10px 14px;border-radius:10px;border:1px solid rgba(5,150,105,0.3);background:rgba(5,150,105,0.04);'>"
                "<div style='font-size:11px;font-weight:700;color:#059669;margin-bottom:6px;'>&#9650; 推荐配置方向</div>";
            if (topBuy.isEmpty()) {
                h += "<div style='font-size:12px;color:" + t.mutedColor + ";'>当前无明显推荐方向</div>";
            } else {
                for (const QString &name : topBuy) {
                    h += "<div style='font-size:12px;margin:2px 0;'>&#8226; " + name + "</div>";
                }
            }
            h += "</div></td>";
            h += "<td style='width:50%;vertical-align:top;'>";
            h += "<div style='padding:10px 14px;border-radius:10px;border:1px solid rgba(220,38,38,0.3);background:rgba(220,38,38,0.04);'>"
                "<div style='font-size:11px;font-weight:700;color:#DC2626;margin-bottom:6px;'>&#9660; 建议回避方向</div>";
            if (topSell.isEmpty()) {
                h += "<div style='font-size:12px;color:" + t.mutedColor + ";'>当前无明显回避方向</div>";
            } else {
                for (const QString &name : topSell) {
                    h += "<div style='font-size:12px;margin:2px 0;'>&#8226; " + name + "</div>";
                }
            }
            h += "</div></td></tr></table>";

            // 策略回测汇总
            int totalStrategies = 0;
            double bestOverallWR = 0;
            QString bestOverallName, bestOverallSector;
            for (const StrategyItem &it : sectorSorted) {
                if (!it.sector) continue;
                for (const StrategyBacktest &bt : it.sector->backtestResults) {
                    ++totalStrategies;
                    if (bt.totalTrades >= 2 && bt.winRate > bestOverallWR) {
                        bestOverallWR = bt.winRate;
                        bestOverallName = bt.name;
                        bestOverallSector = it.name;
                    }
                }
            }
            if (totalStrategies > 0) {
                h += "<div style='margin-top:8px;padding:8px 14px;border:1px solid " + t.cardBorder + ";border-radius:10px;font-size:11px;color:" + t.mutedColor + ";'>"
                    "&#128640; 全市场回测覆盖 " + QString::number(totalStrategies) + " 个策略组合";
                if (!bestOverallName.isEmpty())
                    h += " | 最佳：<b style='color:#059669;'>" + bestOverallSector + " - " + bestOverallName
                        + "（胜率 " + num(bestOverallWR, 0) + "%）</b>";
                h += "</div>";
            }
        }
    }

    // ---- 未来事件日历 ----
    {
        struct FutureEventEntry { QString sector; QString event; bool isAI; };
        QList<FutureEventEntry> allEvents;
        for (const SectorSnapshot &s : analysis.sectors) {
            for (const QString &ev : s.upcomingEvents)
                allEvents.push_back({s.industry, ev, false});
            for (const QString &ev : s.futureEventsAI)
                allEvents.push_back({s.industry, ev, true});
        }
        if (!allEvents.isEmpty()) {
            h += "<hr class='divider'/>";
            h += "<div class='section-title'>&#128197; 未来事件日历 <span style='font-size:11px;font-weight:400;color:" + t.mutedColor + ";'>"
                + QString::number(allEvents.size()) + " 条前瞻事件</span></div>";

            QMap<QString, QList<FutureEventEntry>> bySector;
            for (const FutureEventEntry &e : allEvents)
                bySector[e.sector].push_back(e);

            for (auto it = bySector.constBegin(); it != bySector.constEnd(); ++it) {
                h += "<div style='margin-bottom:8px;'>"
                    "<div style='font-size:12px;font-weight:700;color:" + t.sectionTitleColor + ";margin-bottom:4px;'>"
                    + it.key() + "</div>";
                for (const FutureEventEntry &e : it.value()) {
                    const QString borderClr = e.isAI ? "#F59E0B" : "#8B5CF6";
                    const QString label = e.isAI ? "AI&#21069;&#30651;" : "&#26032;&#38395;&#25552;&#21462;";
                    h += "<div style='padding:5px 12px;margin:2px 0;border-radius:8px;background:" + t.narrativeBg
                        + ";font-size:11px;border-left:3px solid " + borderClr + ";border:1px solid " + t.cardBorder + ";line-height:1.5;'>"
                        "<span style='color:" + borderClr + ";font-weight:600;font-size:10px;'>" + label + "</span> "
                        + e.event.toHtmlEscaped() + "</div>";
                }
                h += "</div>";
            }
        }
    }

    h += "<hr class='divider'/>";
    h += "<div style='font-size:10px;color:" + t.disclaimerColor + ";margin-top:10px;line-height:1.5;letter-spacing:0.1px;'>"
        "免责声明：以上分析基于公开数据与规则引擎/AI推断，不构成投资建议或承诺。投资有风险，决策需谨慎。"
        "</div>";

    h += "</body></html>";
    return h;
}

QString MainWindow::buildSectorHtml(const SectorSnapshot &s, bool aiAvailable, bool simpleMode) const
{
    const ThemeColors &t = *s_theme;
    const QPixmap chart = buildTrendChart(s, 800, 850);
    const QString chartB64 = px2b64(chart);

    QString h = "<html><head><style>" + UiTheme::buildHtmlCss(t) + "</style></head><body>";

    // ============================================================
    // 标题 + 基本信息
    // ============================================================
    if (s.marketClosed) {
        h += "<h2>" + s.industry + " <span style='color:" + t.mutedColor + ";font-size:14px;'>"
            + QString::fromUtf8("休市（上一交易日 ") + pct(s.todayChangePct) + "）</span></h2>";
    } else if (!s.todayChangePctValid) {
        h += "<h2>" + s.industry + " <span style='color:" + t.mutedColor + ";font-size:14px;'>"
            + QString::fromUtf8("涨跌数据缺失") + "</span></h2>";
    } else {
        h += "<h2>" + s.industry + " <span style='color:" + clr(s.todayChangePct) + ";font-size:16px;'>" + pct(s.todayChangePct) + "</span></h2>";
    }
    {
        QString metaStr = QString::fromUtf8("热度 ") + num(s.sectorHotScore, 1)
            + " &nbsp;|&nbsp; " + QString::fromUtf8("成分股 ") + num(s.sectorStockCount > 0 ? s.sectorStockCount : s.stockCount, 0) + QString::fromUtf8(" 只");
        if (s.totalMarketCap > 0)
            metaStr += " &nbsp;|&nbsp; " + QString::fromUtf8("总市值 ") + num(s.totalMarketCap, 0) + QString::fromUtf8(" 亿");
        h += "<div class='meta'>" + metaStr + "</div>";
    }

    // ============================================================
    // PART 1: 投资结论（最前面）
    // ============================================================
    {
        const TradingStrategy &st = s.strategy;
        QString actionBg = "#6B7280";
        if (st.actionLabel.contains(QString::fromUtf8("买入"))) actionBg = "#DC2626";
        else if (st.actionLabel.contains(QString::fromUtf8("增配"))) actionBg = "#EF4444";
        else if (st.actionLabel.contains(QString::fromUtf8("止损"))) actionBg = "#2563EB";
        else if (st.actionLabel.contains(QString::fromUtf8("减仓"))) actionBg = "#3B82F6";
        else if (st.actionLabel.contains(QString::fromUtf8("持有"))) actionBg = "#059669";

        h += "<div style='margin:12px 0;padding:16px 20px;border-radius:12px;background:" + t.narrativeBg
            + ";border:2px solid " + actionBg + ";'>";

        h += "<div style='display:flex;align-items:center;gap:12px;margin-bottom:12px;'>"
            "<div style='padding:8px 24px;border-radius:24px;background:" + actionBg
            + ";color:#fff;font-size:18px;font-weight:800;letter-spacing:1px;'>" + st.actionLabel + "</div>"
            "<div style='font-size:13px;color:" + t.bodyColor + ";'>"
            + QString::fromUtf8("综合评分 ") + "<b style='color:" + clr(s.forecastScore) + ";font-size:16px;'>" + num(s.forecastScore)
            + "</b> &nbsp; " + QString::fromUtf8("置信度 ") + "<b>" + num(s.confidence) + "</b>"
            + " &nbsp; " + QString::fromUtf8("趋势 ") + "<b>" + s.trendSummary + "</b></div></div>";

        // 预期收益 vs 风险
        h += "<div style='display:flex;gap:12px;margin-bottom:12px;'>";
        h += "<div style='flex:1;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:12px 16px;background:" + t.paneBg + ";text-align:center;'>"
            "<div style='font-size:10px;color:" + t.subtleColor + ";letter-spacing:0.5px;text-transform:uppercase;'>"
            + QString::fromUtf8("预期收益（止盈目标）") + "</div>"
            "<div style='font-size:22px;font-weight:800;color:#059669;margin:4px 0;'>+" + num(st.takeProfitPct, 1)
            + "<span style='font-size:12px;'>%</span></div>"
            "<div style='font-size:11px;color:" + t.mutedColor + ";'>" + st.takeProfitReason.toHtmlEscaped() + "</div></div>";
        h += "<div style='flex:1;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:12px 16px;background:" + t.paneBg + ";text-align:center;'>"
            "<div style='font-size:10px;color:" + t.subtleColor + ";letter-spacing:0.5px;text-transform:uppercase;'>"
            + QString::fromUtf8("最大风险（止损线）") + "</div>"
            "<div style='font-size:22px;font-weight:800;color:#DC2626;margin:4px 0;'>" + num(st.stopLossPct, 1)
            + "<span style='font-size:12px;'>%</span></div>"
            "<div style='font-size:11px;color:" + t.mutedColor + ";'>" + st.stopLossReason.toHtmlEscaped() + "</div></div>";
        if (s.trackingDays > 0 || s.cumulativeReturn != 0) {
            const QString cumClr = s.cumulativeReturn >= 0 ? "#EF4444" : "#3B82F6";
            h += "<div style='flex:1;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:12px 16px;background:" + t.paneBg + ";text-align:center;'>"
                "<div style='font-size:10px;color:" + t.subtleColor + ";letter-spacing:0.5px;text-transform:uppercase;'>"
                + QString::fromUtf8("累计收益（") + num(s.trackingDays, 0) + QString::fromUtf8("天）") + "</div>"
                "<div style='font-size:22px;font-weight:800;color:" + cumClr + ";margin:4px 0;'>"
                + (s.cumulativeReturn >= 0 ? "+" : "") + num(s.cumulativeReturn, 2)
                + "<span style='font-size:12px;'>%</span></div></div>";
        }
        h += "</div>";

        // 短中长期分析
        h += "<div style='display:flex;gap:10px;margin-bottom:10px;'>";
        h += "<div style='flex:1;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:12px 16px;background:" + t.paneBg + ";'>"
            "<div style='font-size:12px;font-weight:700;color:" + t.sectionTitleColor + ";margin-bottom:6px;'>"
            + QString::fromUtf8("短期（1-5日）") + "</div>"
            "<div style='font-size:12px;line-height:1.7;color:" + t.bodyColor + ";'>" + st.shortTermView.toHtmlEscaped() + "</div></div>";
        h += "<div style='flex:1;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:12px 16px;background:" + t.paneBg + ";'>"
            "<div style='font-size:12px;font-weight:700;color:" + t.sectionTitleColor + ";margin-bottom:6px;'>"
            + QString::fromUtf8("中期（1-4周）") + "</div>"
            "<div style='font-size:12px;line-height:1.7;color:" + t.bodyColor + ";'>" + st.mediumTermView.toHtmlEscaped() + "</div></div>";
        h += "<div style='flex:1;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:12px 16px;background:" + t.paneBg + ";'>"
            "<div style='font-size:12px;font-weight:700;color:" + t.sectionTitleColor + ";margin-bottom:6px;'>"
            + QString::fromUtf8("长期（1-3月）") + "</div>"
            "<div style='font-size:12px;line-height:1.7;color:" + t.bodyColor + ";'>" + st.longTermView.toHtmlEscaped() + "</div></div>";
        h += "</div>";

        // 操作建议
        if (!st.operationAdvice.isEmpty()) {
            h += "<div style='padding:10px 14px;border-radius:8px;background:" + t.paneBg + ";border:1px solid " + t.cardBorder
                + ";font-size:12px;line-height:1.7;color:" + t.bodyColor + ";'>"
                + st.operationAdvice.toHtmlEscaped().replace("\n", "<br/>") + "</div>";
        }

        // 看多/看空因素
        if (!s.positiveFactors.isEmpty() || !s.negativeFactors.isEmpty()) {
            h += "<div style='display:flex;gap:16px;margin-top:10px;'>";
            h += "<div style='flex:1;'><div style='font-size:12px;font-weight:700;color:#059669;margin-bottom:4px;'>"
                + QString::fromUtf8("&#9650; 看多因素") + "</div><ul class='factor-list'>";
            for (const QString &f : s.positiveFactors) h += "<li>" + f.toHtmlEscaped() + "</li>";
            h += "</ul></div>";
            h += "<div style='flex:1;'><div style='font-size:12px;font-weight:700;color:#DC2626;margin-bottom:4px;'>"
                + QString::fromUtf8("&#9660; 看空因素") + "</div><ul class='factor-list'>";
            for (const QString &f : s.negativeFactors) h += "<li>" + f.toHtmlEscaped() + "</li>";
            h += "</ul></div></div>";
        }

        h += "</div>"; // conclusion box

        // AI/规则引擎深度分析
        if (aiAvailable && !s.aiAnalysis.isEmpty()) {
            h += "<div class='section-title'>" + QString::fromUtf8("深度行业分析") + " <span class='ai-badge'>AI</span></div>";
            h += "<div class='narrative'>" + s.aiAnalysis.toHtmlEscaped().replace("\n", "<br/>") + "</div>";
            if (!s.aiPredictionReason.isEmpty()) {
                h += "<div class='section-title'>" + QString::fromUtf8("走势研判") + " <span class='ai-badge'>AI</span></div>";
                h += "<div class='narrative'>" + s.aiPredictionReason.toHtmlEscaped().replace("\n", "<br/>") + "</div>";
            }
        } else if (!s.analysisNarrative.isEmpty()) {
            h += "<div class='section-title'>" + QString::fromUtf8("综合分析") + "</div>";
            h += "<div class='narrative'>" + s.analysisNarrative.toHtmlEscaped().replace("\n", "<br/>") + "</div>";
        }

        // 持仓建议
        if (!s.personalAdvice.isEmpty()) {
            h += "<div style='background:" + t.narrativeBg + ";border-left:3px solid #059669;"
                 "border-radius:8px;padding:12px 16px;margin:12px 0;"
                 "font-size:13px;color:" + t.bodyColor + ";border:1px solid " + t.cardBorder + ";'>"
                 "<b style='color:#059669;'>" + QString::fromUtf8("&#127981; 我的持仓建议") + "</b><br/>"
                 + s.personalAdvice.toHtmlEscaped().replace("\n", "<br/>") + "</div>";
        }
    }

    // ============================================================
    // PART 1.5: 趋势生命周期 + 健康度 + 风险预警（升级版核心）
    // ============================================================
    if (!s.trendStageResult.stageName.isEmpty()) {
        h += "<div style='margin:14px 0;padding:16px 20px;border-radius:12px;background:" + t.narrativeBg
            + ";border:1px solid " + t.cardBorder + ";'>";
        h += "<div class='section-title' style='margin-top:0;'>" + QString::fromUtf8("趋势生命周期分析") + "</div>";

        // 趋势阶段标签 + 投资状态
        QString stageBg = "#6B7280";
        if (s.trendStageResult.stage == TrendStage::MainUptrend) stageBg = "#059669";
        else if (s.trendStageResult.stage == TrendStage::EarlyBreakout) stageBg = "#3B82F6";
        else if (s.trendStageResult.stage == TrendStage::Acceleration) stageBg = "#F59E0B";
        else if (s.trendStageResult.stage == TrendStage::Distribution) stageBg = "#EF4444";
        else if (s.trendStageResult.stage == TrendStage::Downtrend) stageBg = "#DC2626";
        else if (s.trendStageResult.stage == TrendStage::BottomAccumulation) stageBg = "#8B5CF6";

        h += "<div style='display:flex;align-items:center;gap:12px;margin-bottom:12px;'>"
            "<div style='padding:6px 20px;border-radius:20px;background:" + stageBg
            + ";color:#fff;font-size:15px;font-weight:700;'>" + s.trendStageResult.stageName + "</div>"
            "<div style='padding:6px 16px;border-radius:20px;background:" + t.tableHeaderBg + ";"
            "font-size:13px;font-weight:600;color:" + t.bodyColor + ";border:1px solid " + t.cardBorder + ";'>"
            + s.explanation.stateName + "</div>"
            "<div style='font-size:12px;color:" + t.mutedColor + ";'>"
            + QString::fromUtf8("阶段置信度 ") + num(s.trendStageResult.stageConfidence, 0) + "%</div></div>";

        // 推理链描述
        h += "<div style='font-size:12px;line-height:1.8;color:" + t.bodyColor + ";margin-bottom:12px;'>"
            + s.explanation.trendStageDesc.toHtmlEscaped() + "</div>";

        // 趋势健康度 5 维雷达（用卡片代替）
        h += "<div style='display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;'>";
        auto healthCard = [&](const QString &label, double val) {
            QString c = (val >= 65) ? "#059669" : (val >= 40) ? "#F59E0B" : "#DC2626";
            return QString("<div style='flex:1;min-width:90px;border:1px solid " + t.cardBorder
                + ";border-radius:10px;padding:8px 10px;text-align:center;background:" + t.paneBg + ";'>"
                "<div style='font-size:9px;color:" + t.subtleColor + ";'>") + label + "</div>"
                "<div style='font-size:18px;font-weight:800;color:" + c + ";margin:2px 0;'>"
                + num(val, 0) + "</div></div>";
        };
        h += healthCard(QString::fromUtf8("结构"), s.trendHealth.structureScore);
        h += healthCard(QString::fromUtf8("动量"), s.trendHealth.momentumQuality);
        h += healthCard(QString::fromUtf8("回撤"), s.trendHealth.pullbackQuality);
        h += healthCard(QString::fromUtf8("量价"), s.trendHealth.volumeHealth);
        h += healthCard(QString::fromUtf8("波动"), s.trendHealth.volatilityHealth);
        h += healthCard(QString::fromUtf8("可持续"), s.trendHealth.sustainability);
        h += "</div>";

        // 三维评分：趋势强度 / 趋势质量 / 耗竭风险
        h += "<div style='display:flex;gap:10px;margin-bottom:12px;'>";
        {
            auto scoreBar = [&](const QString &label, double val, bool inversed) {
                QString c = inversed
                    ? ((val < 35) ? "#059669" : (val < 60) ? "#F59E0B" : "#DC2626")
                    : ((val >= 65) ? "#059669" : (val >= 40) ? "#F59E0B" : "#DC2626");
                return QString("<div style='flex:1;border:1px solid " + t.cardBorder
                    + ";border-radius:10px;padding:10px 14px;background:" + t.paneBg + ";'>"
                    "<div style='font-size:10px;color:" + t.subtleColor + ";margin-bottom:4px;'>") + label + "</div>"
                    "<div style='height:6px;border-radius:3px;background:" + t.tableBorder + ";overflow:hidden;'>"
                    "<div style='height:100%;width:" + num(qBound(0.0, val, 100.0), 0) + "%;background:" + c + ";border-radius:3px;'></div></div>"
                    "<div style='font-size:14px;font-weight:700;color:" + c + ";margin-top:4px;'>"
                    + num(val, 0) + "/100</div></div>";
            };
            h += scoreBar(QString::fromUtf8("趋势强度"), s.explanation.trendStrength, false);
            h += scoreBar(QString::fromUtf8("趋势质量"), s.explanation.trendQuality, false);
            h += scoreBar(QString::fromUtf8("耗竭风险"), s.explanation.exhaustionRisk, true);
        }
        h += "</div>";

        // 强度/可持续性/风险原因
        h += "<div style='font-size:12px;line-height:1.8;color:" + t.bodyColor + ";'>";
        h += "<b>" + QString::fromUtf8("强度：") + "</b>" + s.explanation.strengthReason.toHtmlEscaped() + "<br/>";
        h += "<b>" + QString::fromUtf8("可持续性：") + "</b>" + s.explanation.sustainabilityReason.toHtmlEscaped() + "<br/>";
        h += "<b>" + QString::fromUtf8("风险：") + "</b>" + s.explanation.riskReason.toHtmlEscaped();
        h += "</div>";

        // 推理结论
        h += "<div style='margin-top:10px;padding:10px 14px;border-radius:8px;background:" + t.paneBg + ";border:1px solid " + t.cardBorder
            + ";font-size:12px;line-height:1.8;color:" + t.bodyColor + ";'>"
            "<b>" + QString::fromUtf8("结论：") + "</b>"
            + s.explanation.conclusion.toHtmlEscaped() + "</div>";

        // 资金结构
        if (!s.flowStructure.flowPattern.isEmpty() && s.flowStructure.flowPattern != QString::fromUtf8("数据不足")) {
            h += "<div style='display:flex;gap:8px;margin-top:10px;'>";
            h += "<div style='flex:1;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:8px 12px;background:" + t.paneBg + ";'>"
                "<div style='font-size:10px;color:" + t.subtleColor + ";'>" + QString::fromUtf8("资金模式") + "</div>"
                "<div style='font-size:14px;font-weight:700;color:" + t.bodyColor + ";'>" + s.flowStructure.flowPattern + "</div></div>";
            h += "<div style='flex:1;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:8px 12px;background:" + t.paneBg + ";'>"
                "<div style='font-size:10px;color:" + t.subtleColor + ";'>" + QString::fromUtf8("连续流入") + "</div>"
                "<div style='font-size:14px;font-weight:700;color:" + t.bodyColor + ";'>" + num(s.flowStructure.continuousInflowDays, 0) + QString::fromUtf8(" 天") + "</div></div>";
            h += "<div style='flex:1;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:8px 12px;background:" + t.paneBg + ";'>"
                "<div style='font-size:10px;color:" + t.subtleColor + ";'>" + QString::fromUtf8("机构特征") + "</div>"
                "<div style='font-size:14px;font-weight:700;color:" + t.bodyColor + ";'>" + num(s.flowStructure.institutionalScore, 0) + "/100</div></div>";
            h += "<div style='flex:1;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:8px 12px;background:" + t.paneBg + ";'>"
                "<div style='font-size:10px;color:" + t.subtleColor + ";'>" + QString::fromUtf8("投机风险") + "</div>"
                "<div style='font-size:14px;font-weight:700;color:"
                + (s.flowStructure.speculativeFlowRisk > 50 ? "#DC2626" : t.bodyColor) + ";'>"
                + num(s.flowStructure.speculativeFlowRisk, 0) + "/100</div></div>";
            h += "</div>";
        }

        // 风险警告
        if (!s.overheat.warnings.isEmpty()) {
            h += "<div style='margin-top:10px;padding:10px 14px;border-radius:8px;background:" + t.warningBg + ";border:1px solid " + t.warningBorder + ";'>";
            for (const QString &w : s.overheat.warnings) {
                h += "<div style='font-size:12px;color:" + t.warningColor + ";line-height:1.6;'>"
                    + QString::fromUtf8("&#9888; ") + w.toHtmlEscaped() + "</div>";
            }
            h += "</div>";
        }

        // 过热指数条
        if (s.overheat.compositeOverheat > 20) {
            QString ohClr = (s.overheat.compositeOverheat > 60) ? "#DC2626" : (s.overheat.compositeOverheat > 40) ? "#F59E0B" : "#059669";
            h += "<div style='margin-top:10px;'>"
                "<div style='font-size:10px;color:" + t.subtleColor + ";margin-bottom:3px;'>"
                + QString::fromUtf8("过热综合指数") + "</div>"
                "<div style='height:8px;border-radius:4px;background:" + t.tableBorder + ";overflow:hidden;'>"
                "<div style='height:100%;width:" + num(s.overheat.compositeOverheat, 0) + "%;background:" + ohClr + ";border-radius:4px;'></div></div>"
                "<div style='font-size:12px;font-weight:700;color:" + ohClr + ";margin-top:2px;'>"
                + num(s.overheat.compositeOverheat, 0) + "/100</div></div>";
        }

        h += "</div>"; // trend lifecycle box
    }

    h += "<hr class='divider'/>";

    // ============================================================
    // PART 2: 数据图表
    // ============================================================
    h += "<div class='section-title'>" + QString::fromUtf8("趋势图 & K线图") + "</div>";
    h += "<div style='margin-bottom:12px;border-radius:10px;overflow:hidden;border:1px solid " + t.cardBorder + ";'>"
        "<img src='data:image/png;base64," + chartB64 + "' style='display:block;width:100%;' /></div>";

    // 核心指标卡片
    h += "<div class='section-title'>" + QString::fromUtf8("核心指标") + "</div>";
    h += "<div style='display:flex;gap:6px;flex-wrap:wrap;margin-bottom:8px;'>";
    {
        auto metricCard = [&](const QString &label, const QString &val, const QString &valClr) {
            return "<div style='flex:1;min-width:80px;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:8px 12px;text-align:center;background:" + t.narrativeBg + ";'>"
                "<div style='font-size:9px;color:" + t.subtleColor + ";letter-spacing:0.3px;text-transform:uppercase;'>" + label + "</div>"
                "<div style='font-size:16px;font-weight:800;margin-top:2px;color:" + valClr + ";'>" + val + "</div></div>";
        };
        QString valClr = "#6B7280";
        if (s.pePercentile < 30) valClr = "#059669";
        else if (s.pePercentile > 70) valClr = "#DC2626";
        QString crowdClr = "#6B7280";
        if (s.crowdingIndex > 70) crowdClr = "#DC2626";
        else if (s.crowdingIndex < 30) crowdClr = "#059669";

        h += metricCard(QString::fromUtf8("5日动量"), pct(s.fiveDayMomentum), clr(s.fiveDayMomentum));
        h += metricCard(QString::fromUtf8("20日动量"), pct(s.twentyDayMomentum), clr(s.twentyDayMomentum));
        h += metricCard(QString::fromUtf8("周动量"), pct(s.weekMomentum), clr(s.weekMomentum));
        h += metricCard(QString::fromUtf8("月动量"), pct(s.monthMomentum), clr(s.monthMomentum));
        h += metricCard(QString::fromUtf8("估值分位"), num(s.pePercentile, 0) + "%", valClr);
        h += metricCard(QString::fromUtf8("拥挤度"), num(s.crowdingIndex, 0) + "%", crowdClr);
    }
    h += "</div>";

    // 周期分析
    if (s.cycle.isCyclical || !s.cycle.phaseName.isEmpty()) {
        const CycleAnalysis &ca = s.cycle;
        const QString phClr = (ca.phaseIndex >= 0 && ca.phaseIndex < 5)
            ? QStringList({"#3B82F6","#059669","#F59E0B","#EF4444","#8B5CF6"})[ca.phaseIndex] : t.mutedColor;
        h += "<div style='display:flex;gap:8px;margin:8px 0;'>";
        h += "<div style='flex:2;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:10px 14px;background:" + t.narrativeBg + ";'>"
            "<div style='font-size:10px;color:" + t.subtleColor + ";'>" + QString::fromUtf8("周期/趋势") + "</div>"
            "<div style='font-size:16px;font-weight:800;color:" + phClr + ";margin:3px 0;'>" + ca.phaseName + "</div>"
            "<div style='font-size:11px;color:" + t.mutedColor + ";'>" + ca.phaseAdvice + "</div></div>";
        if (ca.isCyclical) {
            h += "<div style='flex:1;border:1px solid " + t.cardBorder + ";border-radius:10px;padding:10px 14px;background:" + t.narrativeBg + ";text-align:center;'>"
                "<div style='font-size:10px;color:" + t.subtleColor + ";'>" + QString::fromUtf8("周期") + "</div>"
                "<div style='font-size:14px;font-weight:700;margin:3px 0;'>" + num(ca.estimatedPeriodDays, 0) + QString::fromUtf8(" 日") + "</div>"
                "<div style='font-size:10px;color:" + t.mutedColor + ";'>" + QString::fromUtf8("振幅 ") + num(ca.amplitude, 1) + "%</div></div>";
        }
        h += "</div>";
    }

    // 技术指标（专业模式）
    if (!simpleMode) {
        h += "<hr class='divider'/>";
        h += "<div class='section-title'>" + QString::fromUtf8("技术指标") + "</div>";
        const TechSignals &ts = s.tech;
        auto flag = [](bool cond, const QString &trueText, const QString &falseText = "") -> QString {
            if (cond) return " <span style='font-size:9px;padding:1px 6px;border-radius:10px;background:rgba(239,68,68,0.1);color:#DC2626;font-weight:600;'>" + trueText + "</span>";
            if (!falseText.isEmpty()) return " <span style='font-size:9px;padding:1px 6px;border-radius:10px;background:rgba(59,130,246,0.1);color:#2563EB;font-weight:600;'>" + falseText + "</span>";
            return "";
        };
        h += "<table class='formula' style='width:100%;'>";
        h += "<tr><th>" + QString::fromUtf8("指标") + "</th><th>" + QString::fromUtf8("数值") + "</th><th>" + QString::fromUtf8("状态") + "</th></tr>";
        QString macdState = ts.macdGoldenCross ? QString::fromUtf8("金叉") : (ts.macdDeadCross ? QString::fromUtf8("死叉") : (ts.macdHist > 0 ? QString::fromUtf8("多头") : QString::fromUtf8("空头")));
        h += "<tr><td>MACD</td><td>DIF:" + num(ts.macdDIF, 3) + " DEA:" + num(ts.macdDEA, 3) + " <b style='color:" + clr(ts.macdHist) + ";'>" + num(ts.macdHist, 3) + "</b></td>"
            "<td>" + macdState + flag(ts.macdGoldenCross, QString::fromUtf8("金叉")) + flag(ts.macdDeadCross, QString::fromUtf8("死叉")) + "</td></tr>";
        h += "<tr><td>RSI</td><td>RSI6:<b>" + num(ts.rsi6, 1) + "</b> RSI12:" + num(ts.rsi12, 1) + " RSI24:" + num(ts.rsi24, 1) + "</td>"
            "<td>" + flag(ts.rsiOverbought, QString::fromUtf8("超买")) + flag(ts.rsiOversold, QString::fromUtf8("超卖")) + ((!ts.rsiOverbought && !ts.rsiOversold) ? QString::fromUtf8("正常") : "") + "</td></tr>";
        h += "<tr><td>KDJ</td><td>K:" + num(ts.kdjK, 1) + " D:" + num(ts.kdjD, 1) + " J:<b>" + num(ts.kdjJ, 1) + "</b></td>"
            "<td>" + flag(ts.kdjGoldenCross, QString::fromUtf8("金叉")) + flag(ts.kdjOverbought, QString::fromUtf8("J超买")) + flag(ts.kdjOversold, QString::fromUtf8("J超卖")) + "</td></tr>";
        QString maState = ts.maLongArrange ? QString::fromUtf8("多头排列") : (ts.maShortArrange ? QString::fromUtf8("空头排列") : QString::fromUtf8("交织"));
        h += "<tr><td>" + QString::fromUtf8("均线") + "</td><td>MA5:" + num(ts.ma5) + " MA20:" + num(ts.ma20) + " MA60:" + num(ts.ma60) + "</td>"
            "<td><b>" + maState + "</b></td></tr>";
        h += "<tr><td>" + QString::fromUtf8("量比") + "</td><td><b>" + num(ts.volRatio) + "</b></td>"
            "<td>" + flag(ts.volExpansion, QString::fromUtf8("放量")) + flag(ts.volShrink, QString::fromUtf8("缩量")) + "</td></tr>";
        h += "<tr style='background:" + t.formulaHighlightBg + ";font-weight:700;'><td>" + QString::fromUtf8("综合评分") + "</td><td colspan='2'>" + scoreBar(ts.techScore) + "</td></tr>";
        h += "</table>";

        // 支撑/压力位
        if (s.strategy.supportLevel1 > 0 || s.strategy.resistLevel1 > 0) {
            h += "<table class='formula' style='width:auto;margin:8px 0;'>";
            h += "<tr><th>" + QString::fromUtf8("价位") + "</th><th>" + QString::fromUtf8("第一位") + "</th><th>" + QString::fromUtf8("第二位") + "</th></tr>";
            h += "<tr><td style='color:#43A047;font-weight:700;'>" + QString::fromUtf8("支撑") + "</td><td>" + num(s.strategy.supportLevel1) + "</td><td>" + num(s.strategy.supportLevel2) + "</td></tr>";
            h += "<tr><td style='color:#E53935;font-weight:700;'>" + QString::fromUtf8("压力") + "</td><td>" + num(s.strategy.resistLevel1) + "</td><td>" + num(s.strategy.resistLevel2) + "</td></tr>";
            h += "</table>";
        }
    }

    // 策略回测
    if (!s.backtestResults.isEmpty()) {
        h += "<hr class='divider'/>";
        h += "<div class='section-title'>" + QString::fromUtf8("策略回测") + "</div>";
        if (!s.investmentRecommendation.isEmpty()) {
            h += "<div style='padding:8px 14px;margin-bottom:8px;border-radius:8px;background:" + t.narrativeBg + ";border:1px solid " + t.cardBorder + ";font-size:12px;'>"
                + s.investmentRecommendation.toHtmlEscaped() + "</div>";
        }
        h += "<table class='overview' style='font-size:11px;'>";
        h += "<tr><th>" + QString::fromUtf8("策略") + "</th><th style='text-align:center;'>" + QString::fromUtf8("次数") + "</th><th style='text-align:center;'>"
            + QString::fromUtf8("胜率") + "</th><th style='text-align:center;'>" + QString::fromUtf8("平均收益") + "</th><th style='text-align:center;'>"
            + QString::fromUtf8("回撤") + "</th><th style='text-align:center;'>" + QString::fromUtf8("信号") + "</th></tr>";
        for (const StrategyBacktest &bt : s.backtestResults) {
            const bool isBest = bt.name == s.bestStrategyName;
            h += "<tr" + QString(isBest ? " style='background:rgba(5,150,105,0.06);'" : "") + ">"
                "<td>" + (isBest ? "<b>" + bt.name + " ★</b>" : bt.name) + "</td>"
                "<td style='text-align:center;'>" + num(bt.totalTrades, 0) + "</td>"
                "<td style='text-align:center;font-weight:700;color:" + (bt.winRate >= 50 ? "#059669" : "#DC2626") + ";'>" + num(bt.winRate, 0) + "%</td>"
                "<td style='text-align:center;color:" + clr(bt.avgReturn) + ";'>" + (bt.avgReturn >= 0 ? "+" : "") + num(bt.avgReturn, 2) + "%</td>"
                "<td style='text-align:center;color:#DC2626;'>-" + num(bt.maxDrawdown, 1) + "%</td>"
                "<td style='text-align:center;font-size:10px;'>" + bt.currentSignal + "</td></tr>";
        }
        h += "</table>";
    }

    // 主力资金异动
    if (!s.capitalAnomalies.isEmpty()) {
        h += "<div class='section-title'>" + QString::fromUtf8("主力资金异动") + "</div>";
        h += "<div style='display:flex;flex-wrap:wrap;gap:6px;margin-bottom:8px;'>";
        for (const CapitalAnomaly &ca : s.capitalAnomalies) {
            const QString bgClr = ca.zScore > 0 ? "rgba(239,68,68,0.08)" : "rgba(59,130,246,0.08)";
            const QString txtClr = ca.zScore > 0 ? "#DC2626" : "#2563EB";
            h += "<div style='display:inline-block;padding:4px 10px;border-radius:8px;background:" + bgClr + ";font-size:11px;'>"
                "<span style='color:" + t.mutedColor + ";'>" + ca.date + "</span> "
                "<span style='color:" + txtClr + ";font-weight:600;'>" + ca.desc + "</span></div>";
        }
        h += "</div>";
    }

    h += "<hr class='divider'/>";

    // ============================================================
    // PART 3: 关键事件 + 新闻（按时间排序 + 链接）
    // ============================================================
    h += "<div class='section-title'>" + QString::fromUtf8("关键事件与新闻") + "</div>";

    // 未来事件
    if (!s.upcomingEvents.isEmpty() || !s.futureEventsAI.isEmpty()) {
        for (const QString &ev : s.upcomingEvents) {
            h += "<div style='padding:6px 12px;margin:3px 0;border-radius:8px;background:" + t.narrativeBg
                + ";font-size:12px;border-left:3px solid #8B5CF6;border:1px solid " + t.cardBorder + ";line-height:1.6;'>"
                "<span style='color:#8B5CF6;font-weight:700;'>" + QString::fromUtf8("&#128240; 前瞻") + "</span> " + ev.toHtmlEscaped() + "</div>";
        }
        for (const QString &ev : s.futureEventsAI) {
            h += "<div style='padding:6px 12px;margin:3px 0;border-radius:8px;background:" + t.narrativeBg
                + ";font-size:12px;border-left:3px solid #F59E0B;border:1px solid " + t.cardBorder + ";line-height:1.6;'>"
                "<span style='color:#F59E0B;font-weight:700;'>" + QString::fromUtf8("&#128302; AI前瞻") + "</span> " + ev.toHtmlEscaped() + "</div>";
        }
    }

    // 新闻列表（带时间排序和链接）
    if (!s.newsEntries.isEmpty()) {
        h += "<div style='margin:10px 0 4px;font-size:12px;font-weight:700;color:" + t.sectionTitleColor + ";'>"
            + QString::fromUtf8("相关新闻（") + QString::number(s.newsEntries.size()) + QString::fromUtf8(" 条）") + "</div>";
        for (const NewsEntry &ne : s.newsEntries) {
            const QString sentClr = ne.sentiment > 0.1 ? "#059669" : (ne.sentiment < -0.1 ? "#DC2626" : t.mutedColor);
            const QString sentIcon = ne.sentiment > 0.1 ? QString::fromUtf8("&#9650;") : (ne.sentiment < -0.1 ? QString::fromUtf8("&#9660;") : QString::fromUtf8("&#9679;"));
            h += "<div style='padding:6px 10px;margin:2px 0;border-radius:6px;border:1px solid " + t.cardBorder + ";font-size:12px;line-height:1.5;display:flex;align-items:flex-start;gap:8px;'>";
            h += "<div style='min-width:20px;color:" + sentClr + ";font-size:10px;margin-top:2px;'>" + sentIcon + "</div>";
            h += "<div style='flex:1;'>";
            if (!ne.url.isEmpty()) {
                h += "<a href='" + ne.url.toHtmlEscaped() + "' style='color:" + t.bodyColor + ";text-decoration:none;font-weight:600;'>" + ne.title.toHtmlEscaped() + "</a>";
            } else {
                h += "<span style='font-weight:600;'>" + ne.title.toHtmlEscaped() + "</span>";
            }
            h += "<div style='font-size:10px;color:" + t.mutedColor + ";margin-top:2px;'>";
            if (!ne.date.isEmpty()) h += ne.date + " &nbsp; ";
            if (!ne.source.isEmpty()) h += ne.source;
            h += "</div></div></div>";
        }
    } else if (!s.newsHeadlines.isEmpty()) {
        h += "<div style='margin:10px 0 4px;font-size:12px;font-weight:700;color:" + t.sectionTitleColor + ";'>"
            + QString::fromUtf8("相关新闻（") + num(s.newsHeadlines.size(), 0) + QString::fromUtf8(" 条）") + "</div>";
        for (const QString &line : s.newsHeadlines) {
            h += "<div class='news-item'>" + line.toHtmlEscaped() + "</div>";
        }
    }

    // ============================================================
    // 底部附加信息
    // ============================================================
    if (!simpleMode) {
        h += "<hr class='divider'/>";
        h += "<div class='section-title'>" + QString::fromUtf8("数据来源审计") + "</div>";
        h += "<table class='formula' style='margin-bottom:8px;font-size:11px;'>";
        h += "<tr><th>" + QString::fromUtf8("数据项") + "</th><th>" + QString::fromUtf8("来源") + "</th><th>" + QString::fromUtf8("状态") + "</th></tr>";
        h += "<tr><td>" + QString::fromUtf8("K线") + "</td><td>" + (s.klineSource.isEmpty() ? "-" : s.klineSource) + "</td><td>"
            + (s.trendSeries.size() >= 20 ? QString::fromUtf8("完整") : QString::fromUtf8("不足")) + "</td></tr>";
        h += "<tr><td>" + QString::fromUtf8("资金流") + "</td><td>" + (s.fundFlowSource.isEmpty() ? "-" : s.fundFlowSource) + "</td><td>"
            + (!s.fundFlowSeries.isEmpty() ? QString::fromUtf8("已接入") : QString::fromUtf8("缺失")) + "</td></tr>";
        h += "<tr><td>" + QString::fromUtf8("行情日期") + "</td><td colspan='2'>" + (s.lastDataDate.isEmpty() ? "-" : s.lastDataDate) + "</td></tr>";
        h += "</table>";
    }

    h += "<div style='font-size:10px;color:" + t.disclaimerColor + ";margin-top:10px;line-height:1.5;'>"
        + QString::fromUtf8("免责声明：以上分析基于公开数据与规则引擎/AI推断，不构成投资建议或承诺。投资有风险，决策需谨慎。") + "</div>";

    h += "</body></html>";
    return h;
}

QString MainWindow::buildIndexHtml(const IndexSnapshot &idx, bool aiAvailable, bool simpleMode) const
{
    auto inferTrend = [](double chg) -> QString {
        if (chg >= 2.0) return QString::fromUtf8("强势看多");
        if (chg >= 0.5) return QString::fromUtf8("偏多");
        if (chg <= -2.0) return QString::fromUtf8("强势看空");
        if (chg <= -0.5) return QString::fromUtf8("偏空");
        if (std::abs(chg) <= 0.2) return QString::fromUtf8("横盘震荡");
        return QString::fromUtf8("方向不明");
    };
    auto inferAction = [](double chg) -> AdviceAction {
        if (chg >= 1.2) return AdviceAction::Increase;
        if (chg <= -1.2) return AdviceAction::Decrease;
        return AdviceAction::Hold;
    };

    SectorSnapshot s;
    s.industry = idx.name;
    s.action = idx.changePctValid ? inferAction(idx.changePct) : AdviceAction::Hold;
    s.todayChangePct = idx.changePct;
    s.todayChangePctValid = idx.changePctValid;
    s.fiveDayMomentum = 0.0;
    s.twentyDayMomentum = 0.0;
    s.forecastScore = idx.changePctValid ? qBound(-1.0, idx.changePct / 3.0, 1.0) : 0.0;
    s.confidence = idx.changePctValid ? 90.0 : 30.0;
    s.dataQualityScore = (idx.volume > 0 || idx.amount > 0) ? 95.0 : 85.0;
    s.dataQualityWeight = 1.0;
    s.dataQualityNote = QString::fromUtf8("指数快照与行情聚合数据");
    s.sourceConsistencyScore = 95.0;
    s.sourceConsistencyWeight = 1.0;
    s.crossSourceValidated = true;
    s.trendSummary = inferTrend(idx.changePct);
    s.analysisNarrative = QString::fromUtf8("指数与板块同级纳入分析：建议先看指数方向，再做板块执行。");
    s.strategy.actionLabel = QString::fromUtf8("跟踪指数方向，管理相关仓位");
    s.strategy.shortTermView = s.trendSummary;
    s.strategy.mediumTermView = QString::fromUtf8("以趋势延续为主，关注波动收敛后方向确认");
    s.strategy.longTermView = QString::fromUtf8("作为市场风向锚，联合板块进行仓位配置");
    s.strategy.operationAdvice = QString::fromUtf8("当指数与板块方向冲突时，优先按指数风控执行。");
    s.strategy.stopLossPct = 3.0;
    s.strategy.takeProfitPct = 5.0;
    s.listSource = QString::fromUtf8("MarketContext");
    s.klineSource = QString::fromUtf8("指数历史K线");
    s.fundFlowSource = QString::fromUtf8("指数成交量额（历史）");
    s.valuationSource = QString::fromUtf8("指数不适用");
    s.lastDataDate = QDate::currentDate().toString("yyyy-MM-dd");
    s.sectorTier = 0;
    s.sectorTierLabel = QString::fromUtf8("指数");
    s.newsHeadlines = QStringList()
        << (QString::fromUtf8("指数代码：") + (idx.code.isEmpty() ? QString("-") : idx.code))
        << (QString::fromUtf8("最新点位：") + num(idx.lastClose, 2))
        << (QString::fromUtf8("成交量(亿)：") + num(idx.volume, 0))
        << (QString::fromUtf8("成交额(亿)：") + num(idx.amount, 0));
    s.peRatio = 0.0;
    s.pbRatio = 0.0;
    s.pePercentile = 50.0;
    s.crowdingIndex = 50.0;
    s.totalMarketCap = 0.0;
    s.stockCount = 0;
    s.sectorStockCount = 0;
    s.backtestResults.clear();
    s.bestStrategyName.clear();
    s.bestStrategyWinRate = 0.0;
    s.cycle.isCyclical = false;
    s.cycle.phaseName = QString::fromUtf8("指数趋势");

    // 使用真实指数历史序列
    s.dailyBars = idx.dailyBars;
    s.trendSeries = idx.klineSeries;
    s.weekSeries = idx.weekSeries;
    s.monthSeries = idx.monthSeries;
    if (!s.dailyBars.isEmpty()) {
        s.lastDataDate = s.dailyBars.last().date;
        QVector<double> closes;
        closes.reserve(s.dailyBars.size());
        for (const KBar &b : s.dailyBars) closes.push_back(b.close);
        if (closes.size() >= 6 && closes[closes.size() - 6] > 0) {
            s.fiveDayMomentum = (closes.last() - closes[closes.size() - 6]) / closes[closes.size() - 6] * 100.0;
        }
        if (closes.size() >= 21 && closes[closes.size() - 21] > 0) {
            s.twentyDayMomentum = (closes.last() - closes[closes.size() - 21]) / closes[closes.size() - 21] * 100.0;
        }
        const int flowSpan = qMin(s.dailyBars.size(), 60);
        const int start = s.dailyBars.size() - flowSpan;
        for (int i = start; i < s.dailyBars.size(); ++i) {
            s.fundFlowSeries.push_back(s.dailyBars[i].volume / 1e8);
        }
        // 真实技术指标
        if (s.dailyBars.size() >= 30) {
            const auto macd = TechIndicators::calcMACD(s.dailyBars);
            const auto rsi = TechIndicators::calcRSI(s.dailyBars);
            const auto kdj = TechIndicators::calcKDJ(s.dailyBars);
            const auto ma = TechIndicators::calcMA(s.dailyBars);
            const auto boll = TechIndicators::calcBOLL(s.dailyBars);
            if (!macd.dif.isEmpty() && !macd.dea.isEmpty() && !macd.hist.isEmpty()) {
                s.tech.macdDIF = macd.dif.last();
                s.tech.macdDEA = macd.dea.last();
                s.tech.macdHist = macd.hist.last();
                if (macd.dif.size() >= 2 && macd.dea.size() >= 2) {
                    s.tech.macdGoldenCross = macd.dif[macd.dif.size() - 2] < macd.dea[macd.dea.size() - 2]
                        && s.tech.macdDIF > s.tech.macdDEA;
                    s.tech.macdDeadCross = macd.dif[macd.dif.size() - 2] > macd.dea[macd.dea.size() - 2]
                        && s.tech.macdDIF < s.tech.macdDEA;
                }
            }
            if (!rsi.rsi6.isEmpty()) s.tech.rsi6 = rsi.rsi6.last();
            if (!rsi.rsi12.isEmpty()) s.tech.rsi12 = rsi.rsi12.last();
            if (!rsi.rsi24.isEmpty()) s.tech.rsi24 = rsi.rsi24.last();
            s.tech.rsiOverbought = s.tech.rsi6 >= 80;
            s.tech.rsiOversold = s.tech.rsi6 <= 20;
            if (!kdj.k.isEmpty()) s.tech.kdjK = kdj.k.last();
            if (!kdj.d.isEmpty()) s.tech.kdjD = kdj.d.last();
            if (!kdj.j.isEmpty()) s.tech.kdjJ = kdj.j.last();
            if (kdj.k.size() >= 2 && kdj.d.size() >= 2) {
                s.tech.kdjGoldenCross = kdj.k[kdj.k.size() - 2] < kdj.d[kdj.d.size() - 2]
                    && s.tech.kdjK > s.tech.kdjD;
            }
            s.tech.kdjOverbought = s.tech.kdjJ > 100;
            s.tech.kdjOversold = s.tech.kdjJ < 0;
            if (!ma.ma5.isEmpty()) s.tech.ma5 = ma.ma5.last();
            if (!ma.ma10.isEmpty()) s.tech.ma10 = ma.ma10.last();
            if (!ma.ma20.isEmpty()) s.tech.ma20 = ma.ma20.last();
            if (!ma.ma60.isEmpty()) s.tech.ma60 = ma.ma60.last();
            s.tech.maLongArrange = s.tech.ma5 > s.tech.ma10 && s.tech.ma10 > s.tech.ma20;
            s.tech.maShortArrange = s.tech.ma5 < s.tech.ma10 && s.tech.ma10 < s.tech.ma20;
            if (!boll.upper.isEmpty()) s.tech.bollUpper = boll.upper.last();
            if (!boll.mid.isEmpty()) s.tech.bollMid = boll.mid.last();
            if (!boll.lower.isEmpty()) s.tech.bollLower = boll.lower.last();
            if (s.tech.bollMid > 0) s.tech.bollWidth = (s.tech.bollUpper - s.tech.bollLower) / s.tech.bollMid * 100.0;
            const double last = closes.last();
            s.tech.priceAboveUpper = s.tech.bollUpper > 0 && last > s.tech.bollUpper;
            s.tech.priceBelowLower = s.tech.bollLower > 0 && last < s.tech.bollLower;
            // 综合技术分
            double techScore = 50.0;
            if (s.tech.macdGoldenCross) techScore += 12.0;
            if (s.tech.macdDeadCross) techScore -= 12.0;
            if (s.tech.maLongArrange) techScore += 10.0;
            if (s.tech.maShortArrange) techScore -= 10.0;
            if (s.tech.rsiOverbought) techScore -= 6.0;
            if (s.tech.rsiOversold) techScore += 6.0;
            if (s.tech.kdjGoldenCross) techScore += 6.0;
            s.tech.techScore = qBound(0.0, techScore, 100.0);
            s.forecastScore = qBound(-1.0, s.forecastScore * 0.55 + (s.tech.techScore - 50.0) / 50.0 * 0.45, 1.0);
            s.action = s.forecastScore > 0.12 ? AdviceAction::Increase
                : (s.forecastScore < -0.12 ? AdviceAction::Decrease : AdviceAction::Hold);
        }
    } else {
        s.missingDataItems.push_back(QString::fromUtf8("指数历史K线缺失"));
    }
    s.strategy.actionLabel = QString::fromUtf8("指数方向：") + actionText(s.action);
    s.strategy.shortTermView = s.trendSummary + QString::fromUtf8("（1-3日）");
    s.strategy.mediumTermView = QString::fromUtf8("结合5日动量 ") + pct(s.fiveDayMomentum);
    s.strategy.longTermView = QString::fromUtf8("结合20日动量 ") + pct(s.twentyDayMomentum);
    s.strategy.operationAdvice = QString::fromUtf8("将该指数作为同方向板块的上层风控锚，优先管理总仓风险。");
    s.strategy.supportLevel1 = idx.lastClose * 0.985;
    s.strategy.resistLevel1 = idx.lastClose * 1.015;

    return buildSectorHtml(s, aiAvailable, simpleMode);
}

static void drawDateLabels(QPainter &painter, const QVector<KBar> &bars,
                           int barOffset, int barCount,
                           const QRectF &dataRect, const QColor &textColor, int labelCount = 5);

static void drawSubPanel(QPainter &painter, const QVector<double> &series,
                         const QRectF &panelRect, const QString &title,
                         const QColor &lineColor, const QColor &gridColor,
                         const QColor &axisTextColor, const QColor &bgColor)
{
    const double marginLeft = 52.0;
    const double marginRight = 12.0;
    const double titleH = 18.0;
    const double bottomPad = 8.0;

    const QRectF dataRect(
        panelRect.left() + marginLeft,
        panelRect.top() + titleH,
        panelRect.width() - marginLeft - marginRight,
        panelRect.height() - titleH - bottomPad
    );

    // 背景
    painter.fillRect(panelRect, bgColor);

    // 标题
    QFont tf; tf.setPixelSize(11); tf.setBold(true);
    painter.setFont(tf);
    painter.setPen(axisTextColor);
    painter.drawText(QRectF(panelRect.left() + marginLeft, panelRect.top(), 220, titleH),
                     Qt::AlignLeft | Qt::AlignVCenter, title);

    if (series.size() < 2) {
        QFont sf; sf.setPixelSize(11); painter.setFont(sf);
        painter.setPen(axisTextColor);
        painter.drawText(dataRect, Qt::AlignCenter, "暂无数据");
        return;
    }

    double minV = series.first(), maxV = series.first();
    for (double v : series) { minV = qMin(minV, v); maxV = qMax(maxV, v); }
    if (qFuzzyCompare(minV, maxV)) maxV = minV + 1.0;
    const double range = maxV - minV;

    // 网格 + Y轴标签
    QFont sf; sf.setPixelSize(9); painter.setFont(sf);
    const int gridLines = 4;
    for (int i = 0; i <= gridLines; ++i) {
        double yFrac = static_cast<double>(i) / gridLines;
        double y = dataRect.top() + dataRect.height() * yFrac;
        QColor lightGrid = gridColor; lightGrid.setAlpha(qMin(gridColor.alpha(), 120));
        painter.setPen(QPen(lightGrid, 0.5, Qt::DotLine));
        painter.drawLine(QPointF(dataRect.left(), y), QPointF(dataRect.right(), y));

        double val = maxV - range * yFrac;
        painter.setPen(QPen(axisTextColor, 1));
        painter.drawText(
            QRectF(panelRect.left(), y - 7, marginLeft - 4, 14),
            Qt::AlignRight | Qt::AlignVCenter,
            QString::number(val, 'f', 2));
    }

    // 折线路径
    const double dx = dataRect.width() / static_cast<double>(series.size() - 1);
    QPainterPath fillPath, linePath;
    for (int i = 0; i < series.size(); ++i) {
        double norm = (series[i] - minV) / range;
        double x = dataRect.left() + dx * i;
        double y = dataRect.bottom() - dataRect.height() * norm;
        if (i == 0) {
            linePath.moveTo(x, y);
            fillPath.moveTo(x, dataRect.bottom());
            fillPath.lineTo(x, y);
        } else {
            linePath.lineTo(x, y);
            fillPath.lineTo(x, y);
        }
    }
    fillPath.lineTo(dataRect.left() + dx * (series.size() - 1), dataRect.bottom());
    fillPath.closeSubpath();

    QLinearGradient fillGrad(0, dataRect.top(), 0, dataRect.bottom());
    QColor fc = lineColor; fc.setAlpha(40);
    QColor fc2 = lineColor; fc2.setAlpha(4);
    fillGrad.setColorAt(0, fc);
    fillGrad.setColorAt(1, fc2);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillGrad);
    painter.drawPath(fillPath);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(lineColor, 1.6));
    painter.drawPath(linePath);

    // 最后一点圆点
    const double lastX = dataRect.left() + dx * (series.size() - 1);
    const double lastY = dataRect.bottom() - dataRect.height() * (series.last() - minV) / range;
    QColor halo = lineColor; halo.setAlpha(50);
    painter.setBrush(halo);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(lastX, lastY), 5, 5);
    painter.setBrush(lineColor);
    painter.drawEllipse(QPointF(lastX, lastY), 2.5, 2.5);

    // 区间涨跌标注
    const double changePct = (series.last() - series.first()) / series.first() * 100.0;
    const QString changeStr = (changePct >= 0 ? "+" : "") + QString::number(changePct, 'f', 2) + "%";
    painter.setPen(lineColor);
    painter.setFont(tf);
    const QString changeLabel = QString::fromUtf8("区间: ") + changeStr;
    const double changeLabelW = QFontMetrics(tf).horizontalAdvance(changeLabel) + 8;
    painter.drawText(
        QRectF(panelRect.right() - changeLabelW - 4, panelRect.top(), changeLabelW, titleH),
        Qt::AlignRight | Qt::AlignVCenter,
        changeLabel);
}

static void drawFundFlowPanel(QPainter &painter, const QVector<double> &series,
                               const QRectF &panelRect, const QColor &gridColor,
                               const QColor &axisTextColor, const QColor &bgColor,
                               int alignedSlots = 0,
                               const QVector<KBar> *barsForDates = nullptr,
                               int dateOffset = 0, int dateCount = 0)
{
    const double marginLeft = 52.0;
    const double marginRight = 12.0;
    const double titleH = 18.0;
    const bool showDates = (barsForDates != nullptr && dateCount > 0);
    const double bottomPad = showDates ? 14.0 : 8.0;
    const QRectF dataRect(
        panelRect.left() + marginLeft,
        panelRect.top() + titleH,
        panelRect.width() - marginLeft - marginRight,
        panelRect.height() - titleH - bottomPad);

    painter.fillRect(panelRect, bgColor);

    QFont tf; tf.setPixelSize(11); tf.setBold(true); painter.setFont(tf);
    painter.setPen(axisTextColor);
    painter.drawText(QRectF(panelRect.left() + marginLeft, panelRect.top(), 240, titleH),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString::fromUtf8("主力资金流（亿元）"));

    if (series.size() < 2) {
        QFont sf; sf.setPixelSize(11); painter.setFont(sf);
        painter.setPen(axisTextColor);
        painter.drawText(dataRect, Qt::AlignCenter, QString::fromUtf8("暂无资金流数据"));
        if (showDates)
            drawDateLabels(painter, *barsForDates, dateOffset, dateCount, dataRect, axisTextColor, 5);
        return;
    }

    const int totalSlots = alignedSlots > 0 ? alignedSlots : series.size();

    double minV = *std::min_element(series.begin(), series.end());
    double maxV = *std::max_element(series.begin(), series.end());
    if (qFuzzyCompare(minV, maxV)) { minV -= 1.0; maxV += 1.0; }
    const double absMax = qMax(qAbs(minV), qAbs(maxV)) * 1.1;
    minV = -absMax; maxV = absMax;
    const double range = maxV - minV;

    const double zeroY = dataRect.bottom() - dataRect.height() * (-minV) / range;
    painter.setPen(QPen(gridColor, 1, Qt::DashLine));
    painter.drawLine(QPointF(dataRect.left(), zeroY), QPointF(dataRect.right(), zeroY));

    QFont sf; sf.setPixelSize(9); painter.setFont(sf);
    for (int i = 0; i <= 4; ++i) {
        double yFrac = static_cast<double>(i) / 4;
        double y = dataRect.top() + dataRect.height() * yFrac;
        double val = maxV - range * yFrac;
        painter.setPen(QPen(gridColor, 1, Qt::DotLine));
        painter.drawLine(QPointF(dataRect.left(), y), QPointF(dataRect.right(), y));
        painter.setPen(QPen(axisTextColor, 1));
        painter.drawText(QRectF(panelRect.left(), y - 7, marginLeft - 4, 14),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(val, 'f', 1));
    }

    const int flowStart = totalSlots - series.size();
    const double dx = dataRect.width() / static_cast<double>(totalSlots);
    const double barW = qMax(2.0, dx - 1.5);
    double totalFlow = 0;
    for (int i = 0; i < series.size(); ++i) {
        const double val = series[i];
        totalFlow += val;
        const int slot = flowStart + i;
        const double barX = dataRect.left() + dx * slot + (dx - barW) / 2.0;
        const double barY = val >= 0 ? dataRect.bottom() - dataRect.height() * (val - minV) / range
                                     : zeroY;
        const double barH = qAbs(val) / range * dataRect.height();
        const QColor barColor = val >= 0 ? QColor("#EF4444") : QColor("#3B82F6");
        painter.fillRect(QRectF(barX, barY, barW, barH), barColor);
    }

    painter.setPen(totalFlow >= 0 ? QColor("#EF4444") : QColor("#3B82F6"));
    painter.setFont(tf);
    const QString totalStr = (totalFlow >= 0 ? QString::fromUtf8("累计净流入+")
                                             : QString::fromUtf8("累计净流出"))
                            + QString::number(qAbs(totalFlow), 'f', 1)
                            + QString::fromUtf8("亿");
    painter.drawText(QRectF(dataRect.left() + 160, panelRect.top(), 200, titleH),
                     Qt::AlignLeft | Qt::AlignVCenter, totalStr);

    if (showDates)
        drawDateLabels(painter, *barsForDates, dateOffset, dateCount, dataRect, axisTextColor, 5);
}

static void drawSeriesLine(QPainter &painter, const QVector<double> &series,
                           const QRectF &dataRect, double minV, double range,
                           const QColor &color, qreal lineW = 1.2)
{
    if (series.size() < 2 || range < 1e-12) return;
    const double dx = dataRect.width() / static_cast<double>(series.size() - 1);
    QPainterPath path;
    for (int i = 0; i < series.size(); ++i) {
        double x = dataRect.left() + dx * i;
        double y = dataRect.bottom() - dataRect.height() * (series[i] - minV) / range;
        if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, lineW));
    painter.drawPath(path);
}

static void drawMACDPanel(QPainter &painter, const MACDResult &macd, int offset,
                          const QRectF &panelRect, const QColor &gridColor,
                          const QColor &axisTextColor, const QColor &bgColor,
                          const QVector<KBar> *barsForDates = nullptr, int dateOffset = 0, int dateCount = 0)
{
    const double marginLeft = 52.0, marginRight = 12.0, titleH = 18.0;
    const double bottomPad = (barsForDates ? 14.0 : 4.0);
    const QRectF dataRect(panelRect.left() + marginLeft, panelRect.top() + titleH,
                          panelRect.width() - marginLeft - marginRight,
                          panelRect.height() - titleH - bottomPad);
    painter.fillRect(panelRect, bgColor);
    QFont tf; tf.setPixelSize(11); tf.setBold(true); painter.setFont(tf);
    painter.setPen(axisTextColor);
    painter.drawText(QRectF(panelRect.left() + marginLeft, panelRect.top(), 200, titleH),
                     Qt::AlignLeft | Qt::AlignVCenter, "MACD(12,26,9)");

    const int n = macd.hist.size();
    if (n - offset < 2) {
        QFont sf; sf.setPixelSize(11); painter.setFont(sf);
        painter.drawText(dataRect, Qt::AlignCenter, "暂无数据");
        return;
    }

    double minV = 0, maxV = 0;
    for (int i = offset; i < n; ++i) {
        minV = qMin(minV, qMin(macd.dif[i], qMin(macd.dea[i], macd.hist[i])));
        maxV = qMax(maxV, qMax(macd.dif[i], qMax(macd.dea[i], macd.hist[i])));
    }
    if (qFuzzyCompare(minV, maxV)) { minV -= 1; maxV += 1; }
    const double range = maxV - minV;

    const int cnt = n - offset;
    const double dx = dataRect.width() / static_cast<double>(cnt - 1);

    for (int i = 0; i < cnt; ++i) {
        double val = macd.hist[offset + i];
        double x = dataRect.left() + dx * i;
        double zeroY = dataRect.bottom() - dataRect.height() * (0 - minV) / range;
        double barY = dataRect.bottom() - dataRect.height() * (val - minV) / range;
        double barW = qMax(1.5, dx * 0.6);
        QColor barColor = val >= 0 ? QColor("#EF4444") : QColor("#3B82F6");
        painter.fillRect(QRectF(x - barW / 2, qMin(zeroY, barY), barW, qAbs(barY - zeroY)), barColor);
    }

    QVector<double> difSlice = macd.dif.mid(offset);
    QVector<double> deaSlice = macd.dea.mid(offset);
    drawSeriesLine(painter, difSlice, dataRect, minV, range, QColor("#FF6F00"), 1.4);
    drawSeriesLine(painter, deaSlice, dataRect, minV, range, QColor("#3B82F6"), 1.4);

    if (barsForDates && dateCount > 0)
        drawDateLabels(painter, *barsForDates, dateOffset, dateCount, dataRect, axisTextColor, 5);
}

static void drawVolumePanel(QPainter &painter, const QVector<KBar> &bars, int offset,
                            const QVector<double> &volMA5, const QVector<double> &volMA10,
                            const QRectF &panelRect, const QColor &gridColor,
                            const QColor &axisTextColor, const QColor &bgColor,
                            bool showDateLabels = false)
{
    const double marginLeft = 52.0, marginRight = 12.0, titleH = 18.0;
    const double bottomPad = showDateLabels ? 14.0 : 4.0;
    const QRectF dataRect(panelRect.left() + marginLeft, panelRect.top() + titleH,
                          panelRect.width() - marginLeft - marginRight,
                          panelRect.height() - titleH - bottomPad);
    painter.fillRect(panelRect, bgColor);
    QFont tf; tf.setPixelSize(11); tf.setBold(true); painter.setFont(tf);
    painter.setPen(axisTextColor);
    painter.drawText(QRectF(panelRect.left() + marginLeft, panelRect.top(), 200, titleH),
                     Qt::AlignLeft | Qt::AlignVCenter, "成交量");

    const int n = bars.size();
    if (n - offset < 2) {
        QFont sf; sf.setPixelSize(11); painter.setFont(sf);
        painter.drawText(dataRect, Qt::AlignCenter, "暂无数据");
        return;
    }
    const int cnt = n - offset;
    double maxVol = 0;
    for (int i = offset; i < n; ++i) maxVol = qMax(maxVol, bars[i].volume);
    if (maxVol < 1e-6) maxVol = 1;

    const double dx = dataRect.width() / static_cast<double>(cnt);
    for (int i = 0; i < cnt; ++i) {
        const KBar &bar = bars[offset + i];
        double barH = bar.volume / maxVol * dataRect.height();
        double barX = dataRect.left() + dx * i;
        double barW = qMax(1.5, dx * 0.7);
        QColor c = bar.close >= bar.open ? QColor("#EF4444") : QColor("#3B82F6");
        painter.fillRect(QRectF(barX, dataRect.bottom() - barH, barW, barH), c);
    }

    if (!volMA5.isEmpty()) {
        QVector<double> slice = volMA5.mid(offset);
        drawSeriesLine(painter, slice, dataRect, 0, maxVol, QColor("#FF9800"), 1.0);
    }
    if (!volMA10.isEmpty()) {
        QVector<double> slice = volMA10.mid(offset);
        drawSeriesLine(painter, slice, dataRect, 0, maxVol, QColor("#42A5F5"), 1.0);
    }

    if (showDateLabels)
        drawDateLabels(painter, bars, offset, cnt, dataRect, axisTextColor, 5);
}

static void drawDateLabels(QPainter &painter, const QVector<KBar> &bars,
                           int barOffset, int barCount,
                           const QRectF &dataRect, const QColor &textColor, int labelCount)
{
    if (barCount < 2) return;
    QFont df; df.setPixelSize(8); painter.setFont(df);
    painter.setPen(textColor);
    const double dx = dataRect.width() / qMax(1.0, static_cast<double>(barCount - 1));
    const int step = qMax(1, (barCount - 1) / (labelCount - 1));
    for (int li = 0; li < labelCount; ++li) {
        int idx = li * step;
        if (idx >= barCount) idx = barCount - 1;
        int bi = barOffset + idx;
        if (bi >= bars.size()) bi = bars.size() - 1;
        const QString dateStr = bars[bi].date.mid(2, 8);
        double x = dataRect.left() + dx * idx;
        QFontMetrics fm(df);
        double tw = fm.horizontalAdvance(dateStr);
        double drawX = qBound(dataRect.left(), x - tw / 2.0, dataRect.right() - tw);
        painter.drawText(QPointF(drawX, dataRect.bottom() + 10), dateStr);
    }
}

static void drawTrendWithDates(QPainter &painter, const QVector<KBar> &allBars,
                               int tailCount, const QRectF &panelRect,
                               const QString &title, const QColor &lineColor,
                               const QColor &gridColor, const QColor &axisTextColor,
                               const QColor &bgColor)
{
    const int n = allBars.size();
    const int count = qMin(tailCount, n);
    const int off = n - count;
    QVector<double> closes;
    closes.reserve(count);
    for (int i = off; i < n; ++i) closes.push_back(allBars[i].close);
    QRectF chartRect = panelRect.adjusted(0, 0, 0, -14);
    drawSubPanel(painter, closes, chartRect, title, lineColor, gridColor, axisTextColor, bgColor);

    const double marginLeft = 52.0, marginRight = 12.0, titleH = 18.0, bottomPad = 8.0;
    const QRectF dataRect(chartRect.left() + marginLeft, chartRect.top() + titleH,
                          chartRect.width() - marginLeft - marginRight,
                          chartRect.height() - titleH - bottomPad);
    drawDateLabels(painter, allBars, off, count, dataRect, axisTextColor, 4);
}

static void drawCandlestickChart(QPainter &painter, const QVector<KBar> &bars,
                                 int barOffset, int barCount,
                                 const QRectF &panelRect, const QString &title,
                                 const QColor &gridColor, const QColor &axisTextColor,
                                 const QColor &bgColor)
{
    const double marginLeft = 52.0, marginRight = 12.0, titleH = 18.0;
    const double bottomPad = 14.0;
    const QRectF dataRect(panelRect.left() + marginLeft, panelRect.top() + titleH,
                          panelRect.width() - marginLeft - marginRight,
                          panelRect.height() - titleH - bottomPad);
    painter.fillRect(panelRect, bgColor);

    QFont tf; tf.setPixelSize(11); tf.setBold(true); painter.setFont(tf);
    painter.setPen(axisTextColor);
    painter.drawText(QRectF(panelRect.left() + marginLeft, panelRect.top(), 220, titleH),
                     Qt::AlignLeft | Qt::AlignVCenter, title);

    const int count = qMin(barCount, bars.size() - barOffset);
    if (count < 2) {
        QFont sf; sf.setPixelSize(11); painter.setFont(sf);
        painter.drawText(dataRect, Qt::AlignCenter, QString::fromUtf8("暂无数据"));
        return;
    }

    double minP = 1e18, maxP = -1e18;
    for (int i = barOffset; i < barOffset + count; ++i) {
        minP = qMin(minP, bars[i].low);
        maxP = qMax(maxP, bars[i].high);
    }
    if (qFuzzyCompare(minP, maxP)) maxP = minP + 1.0;
    const double range = maxP - minP;

    QFont sf; sf.setPixelSize(9); painter.setFont(sf);
    for (int gi = 0; gi <= 4; ++gi) {
        double yFrac = static_cast<double>(gi) / 4.0;
        double y = dataRect.top() + dataRect.height() * yFrac;
        QColor lg = gridColor; lg.setAlpha(80);
        painter.setPen(QPen(lg, 0.5, Qt::DotLine));
        painter.drawLine(QPointF(dataRect.left(), y), QPointF(dataRect.right(), y));
        painter.setPen(axisTextColor);
        painter.drawText(QRectF(panelRect.left(), y - 7, marginLeft - 4, 14),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(maxP - range * yFrac, 'f', 2));
    }

    const double candleW = dataRect.width() / count;
    const double bodyW = qMax(1.0, candleW * 0.6);
    for (int i = 0; i < count; ++i) {
        const KBar &b = bars[barOffset + i];
        double cx = dataRect.left() + candleW * (i + 0.5);
        double yHi = dataRect.top() + dataRect.height() * (1.0 - (b.high - minP) / range);
        double yLo = dataRect.top() + dataRect.height() * (1.0 - (b.low - minP) / range);
        double yOpen = dataRect.top() + dataRect.height() * (1.0 - (b.open - minP) / range);
        double yClose = dataRect.top() + dataRect.height() * (1.0 - (b.close - minP) / range);

        bool isUp = b.close >= b.open;
        QColor candleColor = isUp ? QColor("#EF4444") : QColor("#3B82F6");
        painter.setPen(QPen(candleColor, 1));
        painter.drawLine(QPointF(cx, yHi), QPointF(cx, yLo));

        double bodyTop = qMin(yOpen, yClose);
        double bodyH = qMax(1.0, qAbs(yOpen - yClose));
        if (isUp) {
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(cx - bodyW / 2, bodyTop, bodyW, bodyH));
        } else {
            painter.setBrush(candleColor);
            painter.setPen(Qt::NoPen);
            painter.drawRect(QRectF(cx - bodyW / 2, bodyTop, bodyW, bodyH));
        }
    }

    drawDateLabels(painter, bars, barOffset, count, dataRect, axisTextColor, 5);

    const double chg = (bars[barOffset + count - 1].close - bars[barOffset].close)
                     / bars[barOffset].close * 100.0;
    const QString chgStr = (chg >= 0 ? "+" : "") + QString::number(chg, 'f', 2) + "%";
    const QColor chgClr = chg >= 0 ? QColor("#EF4444") : QColor("#3B82F6");
    painter.setPen(chgClr); painter.setFont(tf);
    painter.drawText(QRectF(dataRect.left() + 140, panelRect.top(), 160, titleH),
                     Qt::AlignLeft | Qt::AlignVCenter, chgStr);
}

static QVector<KBar> synthesizeKBars(const QVector<KBar> &daily, int groupDays)
{
    QVector<KBar> result;
    if (daily.isEmpty()) return result;
    int i = 0;
    while (i < daily.size()) {
        KBar grp = daily[i];
        int end = qMin(i + groupDays, daily.size());
        for (int j = i + 1; j < end; ++j) {
            grp.high = qMax(grp.high, daily[j].high);
            grp.low = qMin(grp.low, daily[j].low);
            grp.close = daily[j].close;
            grp.volume += daily[j].volume;
            grp.date = daily[j].date;
        }
        result.push_back(grp);
        i = end;
    }
    return result;
}

QPixmap MainWindow::buildTrendChart(const SectorSnapshot &snap, int width, int height) const
{
    return UiTheme::ChartRenderer::buildTrendChart(snap, *s_theme, width, height);
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
    m_setupPage = new QWidget(m_pages);
    m_mainPage = new QWidget(m_pages);
    m_pages->addWidget(m_setupPage);
    m_pages->addWidget(m_mainPage);
    root->addWidget(m_pages);
    setCentralWidget(central);

    auto *setupLayout = new QVBoxLayout(m_setupPage);
    auto *mainLayout = new QVBoxLayout(m_mainPage);
    buildSetupPage(setupLayout);
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
    m_pages->setCurrentWidget(m_setupPage);
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

    auto *header = new QWidget(m_setupPage);
    header->setFixedHeight(120);
    header->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "stop:0 #4338CA,stop:0.5 #4F46E5,stop:1 #6366F1);");
    auto *hl = new QVBoxLayout(header);
    hl->setContentsMargins(48, 24, 48, 18);
    auto *appTitle = new QLabel("InvestInsight", header);
    appTitle->setStyleSheet("color:#fff;font-size:28px;font-weight:800;letter-spacing:-0.5px;");
    auto *appSub = new QLabel(QString::fromUtf8("行业信息驱动的投资分析工具 · 配置中心"), header);
    appSub->setStyleSheet("color:rgba(255,255,255,0.75);font-size:12px;margin-top:4px;letter-spacing:0.5px;");
    hl->addWidget(appTitle);
    hl->addWidget(appSub);

    auto *content = new QWidget(m_setupPage);
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(40, 24, 40, 24);
    cl->setSpacing(18);

    // ===== 配置 Tabs（AI接入 / 我的持仓）=====
    auto *configTabs = new QTabWidget(content);
    configTabs->setDocumentMode(true);
    configTabs->setElideMode(Qt::ElideNone);
    configTabs->tabBar()->setStyle(new PaddedTabStyle(configTabs->tabBar()->style()));

    // ──────────────── Tab A: AI接入配置 ────────────────
    auto *aiTabWidget = new QWidget(configTabs);
    auto *aiTabLayout = new QVBoxLayout(aiTabWidget);
    aiTabLayout->setContentsMargins(0, 12, 0, 0);
    aiTabLayout->setSpacing(12);

    auto *aiCard = new QGroupBox(QString::fromUtf8("AI 接入配置（Key 仅保存在本机，不写入代码）"), aiTabWidget);
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
    configTabs->addTab(aiTabWidget, QString::fromUtf8("AI 接入配置"));

    // ──────────────── Tab B: 我的持仓 ────────────────
    auto *pfTabWidget = new QWidget(configTabs);
    auto *pfTabLayout = new QVBoxLayout(pfTabWidget);
    pfTabLayout->setContentsMargins(0, 12, 0, 0);
    pfTabLayout->setSpacing(10);

    auto *pfCard = new QGroupBox(QString::fromUtf8("我的持仓（可选 · 用于个性化操作建议）"), pfTabWidget);
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
    configTabs->addTab(pfTabWidget, QString::fromUtf8("我的持仓"));

    m_enterButton = new QPushButton(QString::fromUtf8("保存并进入主界面  →"), content);
    m_enterButton->setFixedHeight(48);
    m_enterButton->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #4338CA,stop:1 #6366F1);color:#fff;border:none;border-radius:12px;"
        "font-size:15px;font-weight:700;letter-spacing:0.5px;}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #4F46E5,stop:1 #818CF8);}"
        "QPushButton:pressed{background:#3730A3;}");
    connect(m_enterButton, &QPushButton::clicked, this, &MainWindow::saveAndEnterMainPage);

    cl->addWidget(configTabs, 1);
    cl->addSpacing(12);
    cl->addWidget(m_enterButton);

    root->addWidget(header);
    root->addWidget(content, 1);
}

void MainWindow::buildMainPage(QVBoxLayout *mainLayout)
{
    auto *topBarW = new QWidget(m_mainPage);
    topBarW->setObjectName("topBarW");
    topBarW->setStyleSheet(QString("QWidget#topBarW{background:%1;border:1px solid %2;border-radius:10px;}")
        .arg(s_theme->paneBg, s_theme->paneBorder));
    auto *topBar = new QHBoxLayout(topBarW);
    topBar->setContentsMargins(12, 7, 12, 7);
    topBar->setSpacing(10);
    m_refreshButton = new QPushButton("▶  开始分析", m_mainPage);
    m_refreshButton->setObjectName("refreshBtn");

    m_aiToggle = new QCheckBox("AI", m_mainPage);
    m_aiToggle->setChecked(m_orchestrator.isAIEnabled() && m_orchestrator.isAIAvailable());
    m_aiToggle->setEnabled(m_orchestrator.isAIAvailable());
    m_aiToggle->setToolTip(m_orchestrator.isAIAvailable()
        ? "启用/关闭 AI 智能分析"
        : "未配置 AI API Key");

    m_backToSetupButton = new QPushButton("⚙ 配置", m_mainPage);
    m_backToSetupButton->setObjectName("secondaryBtn");
    connect(m_backToSetupButton, &QPushButton::clicked, this, [this]() {
        loadSavedAIConfigToForm();
        m_pages->setCurrentWidget(m_setupPage);
    });

    m_statusLabel = new QLabel("就绪", m_mainPage);
    m_statusLabel->setObjectName("statusLabel");
    m_loadingBar = new QProgressBar(m_mainPage);
    m_loadingBar->setFixedHeight(5);
    m_loadingBar->setVisible(false);
    m_loadingBar->setTextVisible(false);

    auto *chatButton = new QPushButton("💬 AI助手", m_mainPage);
    chatButton->setObjectName("secondaryBtn");
    chatButton->setToolTip("打开AI智能问答助手");
    connect(chatButton, &QPushButton::clicked, this, &MainWindow::openChatTab);

    topBar->addWidget(m_refreshButton);
    topBar->addSpacing(4);
    topBar->addWidget(m_aiToggle);
    topBar->addSpacing(8);
    topBar->addWidget(chatButton);
    topBar->addSpacing(4);
    topBar->addWidget(m_backToSetupButton);
    topBar->addStretch(1);
    topBar->addWidget(m_statusLabel);

    m_tabWidget = new ScrollableTabWidget(m_mainPage);
    m_tabWidget->setUsesScrollButtons(true);
    m_tabWidget->setElideMode(Qt::ElideNone);
    m_tabWidget->setTabsClosable(false);
    m_tabWidget->tabBar()->setExpanding(false);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->tabBar()->setStyle(new PaddedTabStyle(m_tabWidget->tabBar()->style()));

    // ---- 综合总览：3个子Tab（数据总览 / 板块&指数信息 / 投资策略）----
    auto *overviewContainer = new QWidget(m_mainPage);
    auto *overviewContainerLayout = new QVBoxLayout(overviewContainer);
    overviewContainerLayout->setContentsMargins(0, 0, 0, 0);
    overviewContainerLayout->setSpacing(0);

    m_overviewSubTabs = new QTabWidget(overviewContainer);
    m_overviewSubTabs->setDocumentMode(true);
    m_overviewSubTabs->setElideMode(Qt::ElideNone);
    m_overviewSubTabs->tabBar()->setExpanding(false);
    m_overviewSubTabs->tabBar()->setStyle(new PaddedTabStyle(m_overviewSubTabs->tabBar()->style()));

    // -- 子Tab 1: 数据总览 --
    m_dashboardBrowser = new ClickableBrowser(m_overviewSubTabs);
    m_dashboardBrowser->onTabJump = [this](int idx) {
        if (idx <= 0 || idx > static_cast<int>(m_lastResult.sectors.size())) return;
        const QString &name = m_lastResult.sectors[idx - 1].industry;
        openSectorTab(name);
    };
    m_dashboardBrowser->onIndexJump = [this](const QString &key) {
        openIndexTab(key);
    };
    m_overviewSubTabs->addTab(m_dashboardBrowser, "数据总览");

    // -- 子Tab 2: 板块&指数信息（含过滤栏）--
    auto *sectorTab = new QWidget(m_overviewSubTabs);
    auto *sectorLayout = new QVBoxLayout(sectorTab);
    sectorLayout->setContentsMargins(0, 6, 0, 0);
    sectorLayout->setSpacing(4);

    auto *filterBar = new QWidget(sectorTab);
    auto *filterRow = new QHBoxLayout(filterBar);
    filterRow->setContentsMargins(10, 6, 10, 8);
    filterRow->setSpacing(8);

    m_overviewSearch = new QLineEdit(filterBar);
    m_overviewSearch->setPlaceholderText("搜索板块或指数名称...");
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
    m_viewMode->setToolTip("简明模式适合快速查看关键信号；专业模式展示完整分析数据");

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
    m_overviewSubTabs->addTab(sectorTab, "板块&指数信息");

    // -- 子Tab 3: 投资策略 --
    m_strategyBrowser = new ClickableBrowser(m_overviewSubTabs);
    m_strategyBrowser->onTabJump = [this](int idx) {
        if (idx <= 0 || idx > static_cast<int>(m_lastResult.sectors.size())) return;
        const QString &name = m_lastResult.sectors[idx - 1].industry;
        openSectorTab(name);
    };
    m_strategyBrowser->onIndexJump = [this](const QString &key) {
        openIndexTab(key);
    };
    m_overviewSubTabs->addTab(m_strategyBrowser, "投资策略");

    overviewContainerLayout->addWidget(m_overviewSubTabs, 1);
    m_tabWidget->addTab(overviewContainer, "综合总览");

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

    mainLayout->setContentsMargins(8, 6, 8, 4);
    mainLayout->setSpacing(6);
    mainLayout->addWidget(topBarW);
    mainLayout->addWidget(m_loadingBar);
    mainLayout->addWidget(m_tabWidget);
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
    
    m_statusLabel->setText("就绪（点击“开始分析”）");
    m_pages->setCurrentWidget(m_mainPage);
}

void MainWindow::openChatTab()
{
    if (m_chatTabIndex >= 0 && m_chatTabIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(m_chatTabIndex);
        return;
    }

    auto *chatTab = new QWidget(m_tabWidget);
    auto *chatLayout = new QVBoxLayout(chatTab);
    chatLayout->setContentsMargins(0, 4, 0, 0);
    chatLayout->setSpacing(6);

    m_chatDisplay = new QTextBrowser(chatTab);
    m_chatDisplay->setOpenExternalLinks(true);
    const ThemeColors &t = *s_theme;
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

    auto *inputBar = new QWidget(chatTab);
    inputBar->setObjectName("chatInputBar");
    inputBar->setStyleSheet(QString(
        "QWidget#chatInputBar{background:%1;border-top:2px solid %2;padding:4px;}")
        .arg(t.paneBg, t.paneBorder));
    auto *inputLayout = new QHBoxLayout(inputBar);
    inputLayout->setContentsMargins(12, 10, 12, 10);
    inputLayout->setSpacing(10);

    m_chatInput = new QLineEdit(inputBar);
    m_chatInput->setPlaceholderText(QString::fromUtf8("输入您的问题..."));
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

    chatLayout->addWidget(m_chatDisplay, 1);
    chatLayout->addWidget(inputBar);

    m_chatTabIndex = static_cast<ScrollableTabWidget *>(m_tabWidget)->addClosableTab(
        chatTab, QString::fromUtf8("AI助手"));
    m_tabWidget->setCurrentIndex(m_chatTabIndex);
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
