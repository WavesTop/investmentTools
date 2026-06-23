#include "core/AIAnalyzer.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutex>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QtConcurrent>

#include "core/HttpClient.h"

namespace {

QStringList deduplicateFactors(const QList<QStringList> &allFactors)
{
    QStringList merged;
    QSet<QString> seen;
    for (const QStringList &factors : allFactors) {
        for (const QString &f : factors) {
            const QString key = f.simplified();
            if (!seen.contains(key)) {
                seen.insert(key);
                merged.push_back(f);
            }
        }
    }
    return merged;
}

QString trimmedString(const QJsonObject &object, const char *key)
{
    return object.value(QLatin1String(key)).toString().trimmed();
}

QStringList stringArray(const QJsonObject &object, const char *key)
{
    QStringList values;
    const QJsonArray array = object.value(QLatin1String(key)).toArray();
    for (const QJsonValue &value : array) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) values.push_back(text);
    }
    values.removeDuplicates();
    return values;
}

QString firstText(const QStringList &items)
{
    for (const QString &item : items) {
        if (!item.trimmed().isEmpty()) return item.trimmed();
    }
    return {};
}

bool hasAnyReadableField(const AIReadableInsight &insight)
{
    return !insight.readableTitle.isEmpty()
        || !insight.summary.isEmpty()
        || !insight.whyItMatters.isEmpty()
        || !insight.impactPath.isEmpty()
        || !insight.primaryReason.isEmpty()
        || !insight.primaryRisk.isEmpty()
        || !insight.nextCheckpoint.isEmpty()
        || !insight.disagreementNotes.isEmpty()
        || !insight.affectedSectors.isEmpty()
        || !insight.sourceRefs.isEmpty();
}

AIReadableInsight readableInsightFromJson(const QJsonObject &object)
{
    AIReadableInsight insight;
    insight.readableTitle = trimmedString(object, "readable_title");
    if (insight.readableTitle.isEmpty()) insight.readableTitle = trimmedString(object, "readableTitle");
    insight.summary = trimmedString(object, "summary");
    insight.whyItMatters = trimmedString(object, "why_it_matters");
    if (insight.whyItMatters.isEmpty()) insight.whyItMatters = trimmedString(object, "whyItMatters");
    insight.impactPath = trimmedString(object, "impact_path");
    if (insight.impactPath.isEmpty()) insight.impactPath = trimmedString(object, "impactPath");
    insight.affectedSectors = stringArray(object, "affected_sectors");
    if (insight.affectedSectors.isEmpty()) insight.affectedSectors = stringArray(object, "affectedSectors");
    insight.primaryReason = trimmedString(object, "primary_reason");
    if (insight.primaryReason.isEmpty()) insight.primaryReason = trimmedString(object, "primaryReason");
    insight.primaryRisk = trimmedString(object, "primary_risk");
    if (insight.primaryRisk.isEmpty()) insight.primaryRisk = trimmedString(object, "primaryRisk");
    insight.nextCheckpoint = trimmedString(object, "next_checkpoint");
    if (insight.nextCheckpoint.isEmpty()) insight.nextCheckpoint = trimmedString(object, "nextCheckpoint");
    insight.confidence = object.value("confidence").toDouble(0.0);
    insight.sourceRefs = stringArray(object, "source_refs");
    if (insight.sourceRefs.isEmpty()) insight.sourceRefs = stringArray(object, "sourceRefs");
    insight.disagreementNotes = trimmedString(object, "disagreement_notes");
    if (insight.disagreementNotes.isEmpty()) insight.disagreementNotes = trimmedString(object, "disagreementNotes");

    if (insight.summary.isEmpty()) insight.summary = trimmedString(object, "prediction_reason");
    if (insight.primaryReason.isEmpty()) insight.primaryReason = firstText(stringArray(object, "positive_factors"));
    if (insight.primaryRisk.isEmpty()) insight.primaryRisk = firstText(stringArray(object, "negative_factors"));
    if (insight.nextCheckpoint.isEmpty()) insight.nextCheckpoint = firstText(stringArray(object, "future_events"));

    insight.valid = hasAnyReadableField(insight);
    return insight;
}

QString firstNonEmptyText(const QStringList &items)
{
    for (const QString &item : items) {
        const QString text = item.trimmed();
        if (!text.isEmpty()) return text;
    }
    return {};
}

AIReadableInsight mergeReadableInsights(const QList<AIAnalyzer::SectorAIResult> &results)
{
    AIReadableInsight merged;
    QStringList affectedSectors;
    QStringList sourceRefs;
    double confidenceSum = 0.0;
    int confidenceCount = 0;

    for (const AIAnalyzer::SectorAIResult &result : results) {
        const AIReadableInsight &current = result.readable;
        if (!current.valid) continue;
        if (merged.readableTitle.isEmpty()) merged.readableTitle = current.readableTitle;
        if (merged.summary.isEmpty()) merged.summary = current.summary;
        if (merged.whyItMatters.isEmpty()) merged.whyItMatters = current.whyItMatters;
        if (merged.impactPath.isEmpty()) merged.impactPath = current.impactPath;
        if (merged.primaryReason.isEmpty()) merged.primaryReason = current.primaryReason;
        if (merged.primaryRisk.isEmpty()) merged.primaryRisk = current.primaryRisk;
        if (merged.nextCheckpoint.isEmpty()) merged.nextCheckpoint = current.nextCheckpoint;
        if (merged.disagreementNotes.isEmpty()) merged.disagreementNotes = current.disagreementNotes;
        affectedSectors.append(current.affectedSectors);
        sourceRefs.append(current.sourceRefs);
        if (current.confidence > 0.0) {
            confidenceSum += current.confidence;
            ++confidenceCount;
        }
    }

    if (merged.summary.isEmpty()) {
        QStringList reasons;
        for (const AIAnalyzer::SectorAIResult &result : results) reasons << result.predictionReason;
        merged.summary = firstNonEmptyText(reasons);
    }
    if (merged.primaryReason.isEmpty()) {
        for (const AIAnalyzer::SectorAIResult &result : results) {
            merged.primaryReason = firstNonEmptyText(result.positiveFactors);
            if (!merged.primaryReason.isEmpty()) break;
        }
    }
    if (merged.primaryRisk.isEmpty()) {
        for (const AIAnalyzer::SectorAIResult &result : results) {
            merged.primaryRisk = firstNonEmptyText(result.negativeFactors);
            if (!merged.primaryRisk.isEmpty()) break;
        }
    }
    if (merged.nextCheckpoint.isEmpty()) {
        for (const AIAnalyzer::SectorAIResult &result : results) {
            merged.nextCheckpoint = firstNonEmptyText(result.futureEvents);
            if (!merged.nextCheckpoint.isEmpty()) break;
        }
    }

    affectedSectors.removeDuplicates();
    sourceRefs.removeDuplicates();
    merged.affectedSectors = affectedSectors;
    merged.sourceRefs = sourceRefs;
    merged.confidence = confidenceCount > 0 ? confidenceSum / confidenceCount : 0.0;
    merged.valid = hasAnyReadableField(merged);
    return merged;
}

AdviceAction actionFromAiLabel(const QString &label)
{
    if (label.contains(QString::fromUtf8("增")) || label.contains(QStringLiteral("Increase"), Qt::CaseInsensitive))
        return AdviceAction::Increase;
    if (label.contains(QString::fromUtf8("减")) || label.contains(QStringLiteral("Decrease"), Qt::CaseInsensitive))
        return AdviceAction::Decrease;
    return AdviceAction::Hold;
}

QString ruleActionLabel(AdviceAction action)
{
    switch (action) {
    case AdviceAction::Increase:
        return QString::fromUtf8("增配");
    case AdviceAction::Decrease:
        return QString::fromUtf8("减配");
    case AdviceAction::Hold:
    default:
        return QString::fromUtf8("持有");
    }
}
} // namespace

AIAnalyzer::AIAnalyzer()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QSettings settings("InvestInsight", "InvestInsight");
    m_enabled = settings.value("ai/enabled", true).toBool();
    m_deepAnalysisTopN = settings.value("ai/deep_analysis_top_n", 20).toInt();

    const QString providersJson = settings.value("ai/providers_json").toString();
    if (!providersJson.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(providersJson.toUtf8());
        if (doc.isArray()) {
            for (const QJsonValue &v : doc.array()) {
                const QJsonObject po = v.toObject();
                AIProvider p;
                p.name = po.value("name").toString();
                p.apiUrl = po.value("api_url").toString();
                p.apiKey = po.value("api_key").toString();
                p.model = po.value("model").toString();
                if (!p.apiKey.isEmpty() && !p.apiUrl.isEmpty()) {
                    m_providers.push_back(p);
                    qDebug() << "[AIAnalyzer] Provider loaded from settings:" << p.name << "model:" << p.model;
                }
            }
        }
    }

    const QString envKey = env.value("DEEPSEEK_API_KEY", env.value("INVESTINSIGHT_AI_KEY"));
    if (!envKey.isEmpty()) {
        bool alreadyHasDeepSeek = false;
        for (const AIProvider &p : m_providers) {
            if (p.apiUrl.contains("deepseek")) { alreadyHasDeepSeek = true; break; }
        }
        if (!alreadyHasDeepSeek) {
            AIProvider p;
            p.name = "DeepSeek";
            p.apiKey = envKey;
            p.apiUrl = env.value("INVESTINSIGHT_AI_URL", "https://api.deepseek.com/chat/completions");
            p.model = env.value("INVESTINSIGHT_AI_MODEL", "deepseek-chat");
            m_providers.push_back(p);
        }
    }

    qDebug() << "[AIAnalyzer] Total providers:" << m_providers.size()
             << "available:" << isAvailable()
             << "enabled:" << m_enabled
             << "deepTopN:" << m_deepAnalysisTopN;
}

bool AIAnalyzer::isAvailable() const
{
    return !m_providers.isEmpty();
}

int AIAnalyzer::providerCount() const
{
    return m_providers.size();
}

void AIAnalyzer::saveEnabledToConfig(bool enabled) const
{
    QSettings settings("InvestInsight", "InvestInsight");
    settings.setValue("ai/enabled", enabled);
}

void AIAnalyzer::configureProviders(const QList<AIProvider> &providers, bool enabled, int deepAnalysisTopN)
{
    m_providers.clear();
    for (const AIProvider &p : providers) {
        if (!p.apiUrl.trimmed().isEmpty() && !p.apiKey.trimmed().isEmpty()) {
            m_providers.push_back(p);
        }
    }
    m_enabled = enabled;
    m_deepAnalysisTopN = qMax(1, deepAnalysisTopN);

    QJsonArray arr;
    for (const AIProvider &p : m_providers) {
        QJsonObject obj;
        obj["name"] = p.name;
        obj["api_url"] = p.apiUrl;
        obj["api_key"] = p.apiKey;
        obj["model"] = p.model;
        arr.append(obj);
    }

    QSettings settings("InvestInsight", "InvestInsight");
    settings.setValue("ai/providers_json", QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    settings.setValue("ai/enabled", m_enabled);
    settings.setValue("ai/deep_analysis_top_n", m_deepAnalysisTopN);
}

// ===================== Stage 1: News Digest =====================

QString AIAnalyzer::buildDigestPrompt(const QList<RawHeadline> &headlines,
                                       const QStringList &sectorNames) const
{
    QString prompt;
    prompt += QString::fromUtf8(
        "你是资深金融新闻分析师。分析以下财经新闻对A股行业板块的影响。\n\n"
        "规则：\n"
        "1. 每条新闻可影响0-3个板块\n"
        "2. 注意间接影响（如\"降准\"影响银行/证券/地产，\"油价上涨\"影响石油/化工/交通）\n"
        "3. 只从下方板块列表中选择，名称必须完全匹配\n"
        "4. 无板块影响的新闻跳过\n\n");

    prompt += QString::fromUtf8("板块列表：");
    prompt += sectorNames.join("|");
    prompt += "\n\n";

    prompt += QString::fromUtf8("新闻列表：\n");
    for (int i = 0; i < headlines.size(); ++i) {
        const auto &h = headlines[i];
        prompt += QString("[%1] %2\n").arg(i).arg(h.title);
    }

    prompt += QString::fromUtf8(
        "\n输出纯JSON数组，格式：\n"
        "[{\"id\":0,\"s\":[{\"n\":\"板块名\",\"d\":1,\"i\":4}],\"e\":\"关键事件≤12字\"},...]"
        "\nd: 1=利好, -1=利空, 0=中性。i: 影响程度1-5。"
        "\n只输出有板块影响的新闻。纯JSON，无其他文字。");

    return prompt;
}

AINewsDigestResult AIAnalyzer::parseDigestResponse(const QString &content) const
{
    AINewsDigestResult result;

    QString normalized = content.trimmed();
    normalized.remove("```json");
    normalized.remove("```JSON");
    normalized.remove("```");

    QJsonArray arr;
    QJsonDocument doc = QJsonDocument::fromJson(normalized.toUtf8());
    if (doc.isArray()) {
        arr = doc.array();
    } else if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.value("data").isArray()) arr = obj.value("data").toArray();
        else if (obj.value("signals").isArray()) arr = obj.value("signals").toArray();
        else if (obj.value("items").isArray()) arr = obj.value("items").toArray();
    }

    if (arr.isEmpty()) {
        int arrStart = normalized.indexOf('[');
        int arrEnd = normalized.lastIndexOf(']');
        if (arrStart >= 0 && arrEnd > arrStart) {
            QJsonDocument fallbackDoc = QJsonDocument::fromJson(
                normalized.mid(arrStart, arrEnd - arrStart + 1).toUtf8());
            if (fallbackDoc.isArray()) {
                arr = fallbackDoc.array();
            }
        }
    }

    if (arr.isEmpty()) {
        QRegularExpression entryRe(
            R"###(\{\s*"id"\s*:\s*(\d+)\s*,\s*"s"\s*:\s*\[(.*?)\]\s*,\s*"e"\s*:\s*"([^"]*)"\s*\})###",
            QRegularExpression::DotMatchesEverythingOption);
        QRegularExpression sectorRe(
            R"###(\{\s*"n"\s*:\s*"([^"]+)"(?:\s*,\s*"d"\s*:\s*(-?\d+))?(?:\s*,\s*"i"\s*:\s*(\d+))?\s*\})###");

        QRegularExpressionMatchIterator it = entryRe.globalMatch(normalized);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const int newsId = m.captured(1).toInt();
            const QString sectorsText = m.captured(2);
            const QString keyEvent = m.captured(3);

            QRegularExpressionMatchIterator sit = sectorRe.globalMatch(sectorsText);
            while (sit.hasNext()) {
                const QRegularExpressionMatch sm = sit.next();
                AINewsSignal signal;
                signal.sector = sm.captured(1).trimmed();
                if (signal.sector.isEmpty()) continue;
                const int d = sm.captured(2).isEmpty() ? 0 : sm.captured(2).toInt();
                const int impact = sm.captured(3).isEmpty() ? 3 : sm.captured(3).toInt();
                signal.sentiment = d * (impact / 5.0);
                signal.impactLevel = impact;
                signal.keyEvent = keyEvent;
                if (newsId >= 0) signal.originalHeadline = QString::number(newsId);
                result.newsSignals.push_back(signal);
            }
        }

        if (!result.newsSignals.isEmpty()) {
            result.valid = true;
            qDebug() << "[AIAnalyzer] Stage 1 recovered by regex parser, signals:"
                     << result.newsSignals.size();
            return result;
        }

        result.errorMessage = "Failed to parse JSON array";
        qDebug() << "[AIAnalyzer] Stage 1 parse raw content:" << normalized;
        return result;
    }

    for (const QJsonValue &v : arr) {
        QJsonObject obj = v.toObject();
        const int newsId = obj.value("id").toInt(-1);
        const QString keyEvent = obj.value("e").toString();
        const QJsonArray sectors = obj.value("s").toArray();

        for (const QJsonValue &sv : sectors) {
            QJsonObject so = sv.toObject();
            AINewsSignal signal;
            signal.sector = so.value("n").toString();
            if (signal.sector.isEmpty()) continue;

            const int d = so.value("d").toInt(0);
            const int impact = so.value("i").toInt(3);
            signal.sentiment = d * (impact / 5.0);
            signal.impactLevel = impact;
            signal.keyEvent = keyEvent;
            if (newsId >= 0) {
                signal.originalHeadline = QString::number(newsId);
            }
            result.newsSignals.push_back(signal);
        }
    }

    result.valid = !result.newsSignals.isEmpty();
    return result;
}

AICollaborationParseResult AIAnalyzer::parseCollaborationResponse(const QString &content) const
{
    AICollaborationParseResult result;

    QString normalized = content.trimmed();
    normalized.remove("```json");
    normalized.remove("```JSON");
    normalized.remove("```");

    const int braceStart = normalized.indexOf('{');
    const int braceEnd = normalized.lastIndexOf('}');
    if (braceStart < 0 || braceEnd <= braceStart) {
        result.errorMessage = QStringLiteral("AI collaboration response has no JSON object");
        return result;
    }
    normalized = normalized.mid(braceStart, braceEnd - braceStart + 1);

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(normalized.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.errorMessage = QStringLiteral("AI collaboration JSON parse failed: ")
            + parseError.errorString();
        return result;
    }

    const QJsonObject root = doc.object();
    const QJsonObject sectors = root.value("sectors").toObject();
    if (sectors.isEmpty()) {
        result.errorMessage = QStringLiteral("AI collaboration JSON has no sectors object");
        return result;
    }

    for (const QString &sectorName : sectors.keys()) {
        const AIReadableInsight insight = readableInsightFromJson(sectors.value(sectorName).toObject());
        if (insight.valid) result.sectors.insert(sectorName, insight);
    }

    result.valid = !result.sectors.isEmpty();
    if (!result.valid) {
        result.errorMessage = QStringLiteral("AI collaboration JSON has no readable sector insights");
    }
    return result;
}

static QString extractContentFromResponse(const QJsonObject &responseObj, const AIProvider &provider)
{
    const bool isClaude = provider.name.contains("Claude", Qt::CaseInsensitive)
                       || provider.apiUrl.contains("anthropic");
    if (isClaude) {
        // Anthropic Messages API: { "content": [{"type":"text","text":"..."}] }
        const QJsonArray contentArr = responseObj.value("content").toArray();
        if (!contentArr.isEmpty())
            return contentArr[0].toObject().value("text").toString();
        return {};
    }
    // OpenAI-compatible: { "choices": [{"message":{"content":"..."}}] }
    const QJsonArray choices = responseObj.value("choices").toArray();
    if (!choices.isEmpty())
        return choices[0].toObject().value("message").toObject().value("content").toString();
    return {};
}

// 为不同 provider 构建请求 body 和 headers
static QByteArray buildRequestBody(const AIProvider &provider, const QString &systemMsg,
                                    const QString &userMsg, int maxTokens, double temperature,
                                    QMap<QString, QString> &headers)
{
    const bool isClaude = provider.name.contains("Claude", Qt::CaseInsensitive)
                       || provider.apiUrl.contains("anthropic");
    const bool isGemini = provider.name.contains("Gemini", Qt::CaseInsensitive)
                       || provider.apiUrl.contains("generativelanguage.googleapis");

    QJsonObject body;

    if (isClaude) {
        // Anthropic Messages API
        body["model"] = provider.model;
        body["max_tokens"] = maxTokens;
        body["temperature"] = temperature;
        if (!systemMsg.isEmpty()) body["system"] = systemMsg;
        QJsonArray messages;
        QJsonObject userObj; userObj["role"] = "user"; userObj["content"] = userMsg;
        messages.append(userObj);
        body["messages"] = messages;

        headers["x-api-key"] = provider.apiKey;
        headers["anthropic-version"] = "2023-06-01";
        headers["Content-Type"] = "application/json";
    } else {
        // OpenAI-compatible (DeepSeek, OpenAI, Qwen, Gemini via OpenAI proxy)
        QJsonObject msgSystem; msgSystem["role"] = "system"; msgSystem["content"] = systemMsg;
        QJsonObject msgUser;   msgUser["role"] = "user";   msgUser["content"] = userMsg;
        QJsonArray messages;
        messages.append(msgSystem);
        messages.append(msgUser);
        body["model"] = provider.model;
        body["messages"] = messages;
        body["temperature"] = temperature;

        const int effectiveMax = provider.apiUrl.contains("deepseek")
            ? qBound(1, maxTokens, 3000) : qMax(1, maxTokens);
        body["max_tokens"] = effectiveMax;

        if (isGemini) {
            // Gemini OpenAI-compat uses API key as query param, not Bearer
            headers["Content-Type"] = "application/json";
        } else {
            headers["Authorization"] = "Bearer " + provider.apiKey;
        }
    }

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

// 构建实际 URL（Gemini 需要附加 key query param）
static QString buildRequestUrl(const AIProvider &provider)
{
    if ((provider.name.contains("Gemini", Qt::CaseInsensitive)
         || provider.apiUrl.contains("generativelanguage.googleapis"))
        && !provider.apiKey.isEmpty())
    {
        QString url = provider.apiUrl;
        url += (url.contains('?') ? "&" : "?");
        url += "key=" + provider.apiKey;
        return url;
    }
    return provider.apiUrl;
}


AINewsDigestResult AIAnalyzer::digestNews(const QList<RawHeadline> &headlines,
                                           const QStringList &sectorNames) const
{
    AINewsDigestResult result;
    if (m_providers.isEmpty() || headlines.isEmpty()) return result;

    // 限制发送给 AI 的新闻数量，优先最新的
    const int kMaxHeadlines = 200;
    const QList<RawHeadline> effectiveHeadlines = headlines.size() > kMaxHeadlines
        ? headlines.mid(0, kMaxHeadlines) : headlines;

    qDebug() << "[AIAnalyzer] Stage 1: Digesting" << effectiveHeadlines.size()
             << "headlines (of" << headlines.size() << ") across" << sectorNames.size() << "sectors";

    // 增大批次 + 并行处理所有批次
    const int batchSize = 100;
    QList<QList<RawHeadline>> batches;
    for (int start = 0; start < effectiveHeadlines.size(); start += batchSize)
        batches.push_back(effectiveHeadlines.mid(start, qMin(batchSize, effectiveHeadlines.size() - start)));

    qDebug() << "[AIAnalyzer] Stage 1: split into" << batches.size() << "batches";

    struct BatchResult {
        QList<AINewsSignal> items;
        QString error;
        bool ok = false;
    };

    QMutex resultMutex;
    QVector<BatchResult> batchResults(batches.size());

    QList<QFuture<void>> futures;
    for (int bi = 0; bi < batches.size(); ++bi) {
        futures.push_back(QtConcurrent::run([this, bi, &batches, &sectorNames,
                                              &batchResults, &resultMutex]() {
            const QList<RawHeadline> &batch = batches[bi];
            const QString prompt = buildDigestPrompt(batch, sectorNames);
            BatchResult br;

            for (const AIProvider &provider : m_providers) {
                const QString sysMsg = QString::fromUtf8(
                    "你是专业金融新闻分析师，擅长判断新闻对A股各行业板块的影响方向和程度。"
                    "回复必须是纯JSON数组，不要包含任何Markdown标记或其他文字。");
                const int s1MaxTokens = provider.apiUrl.contains("deepseek") ? 2500 : 3500;

                QMap<QString, QString> headers;
                const QByteArray jsonBody = buildRequestBody(provider, sysMsg, prompt, s1MaxTokens, 0.1, headers);
                const QString requestUrl = buildRequestUrl(provider);
                const HttpClient::HttpResult httpResult = HttpClient::postJson(requestUrl, jsonBody, headers, 90000);

                if (!httpResult.ok) {
                    QString errorDetail = httpResult.errorMessage;
                    if (!httpResult.body.isEmpty()) {
                        const QJsonDocument errDoc = QJsonDocument::fromJson(httpResult.body.toUtf8());
                        if (errDoc.isObject()) {
                            const QJsonObject errObj = errDoc.object().value("error").toObject();
                            if (!errObj.isEmpty()) errorDetail = errObj.value("message").toString();
                        }
                    }
                    br.error = QString("Stage1 %1: HTTP %2 - %3").arg(provider.name).arg(httpResult.statusCode).arg(errorDetail);
                    continue;
                }

                const QJsonDocument respDoc = QJsonDocument::fromJson(httpResult.body.toUtf8());
                const QString content = extractContentFromResponse(respDoc.object(), provider);
                if (content.isEmpty()) { br.error = "Stage1 empty content from " + provider.name; continue; }

                AINewsDigestResult parsed = parseDigestResponse(content);
                if (!parsed.valid) { br.error = "Stage1 parse failed: " + parsed.errorMessage; continue; }

                br.items = parsed.newsSignals;
                br.ok = true;
                qDebug() << "[AIAnalyzer] Stage 1 batch" << bi << "succeeded via" << provider.name
                         << "signals:" << br.items.size();
                break;
            }

            QMutexLocker lock(&resultMutex);
            batchResults[bi] = br;
        }));
    }

    for (QFuture<void> &f : futures) f.waitForFinished();

    QStringList allErrors;
    for (const BatchResult &br : batchResults) {
        if (br.ok) result.newsSignals.append(br.items);
        else if (!br.error.isEmpty()) allErrors.push_back(br.error);
    }

    result.valid = !result.newsSignals.isEmpty();
    if (!result.valid && !allErrors.isEmpty()) result.errorMessage = allErrors.join(" | ");
    qDebug() << "[AIAnalyzer] Stage 1 total:" << result.newsSignals.size() << "signals, valid:" << result.valid;
    return result;
}

// ===================== Stage 2: Deep Analysis =====================

QString AIAnalyzer::buildDeepPrompt(const AnalysisResult &result, int maxSectors) const
{
    QList<const SectorSnapshot *> targetSectors;
    for (const SectorSnapshot &s : result.sectors) {
        targetSectors.push_back(&s);
    }

    if (maxSectors > 0 && targetSectors.size() > maxSectors) {
        std::sort(targetSectors.begin(), targetSectors.end(),
            [](const SectorSnapshot *a, const SectorSnapshot *b) {
                double scoreA = qAbs(a->forecastScore) * 2.0 + a->newsHitCount * 0.5 + a->sectorHotScore * 0.02;
                double scoreB = qAbs(b->forecastScore) * 2.0 + b->newsHitCount * 0.5 + b->sectorHotScore * 0.02;
                return scoreA > scoreB;
            });
        targetSectors = targetSectors.mid(0, maxSectors);
    }

    QString prompt;
    prompt += QString::fromUtf8(
        "你是一位拥有20年A股投研经验的首席行业分析师。"
        "请基于以下实时行情数据和新闻，为每个板块撰写**专业、深度、客观**的研究报告。\n"
        "你的分析应当体现出专业金融从业者的水准，用词精准，逻辑严密，避免空洞的套话。\n\n");

    prompt += QString::fromUtf8("## 核心分析原则\n");
    prompt += QString::fromUtf8(
        "1. **客观平衡**：不能全部推荐增配，必须根据数据合理分布增配/持有/减配。\n"
        "2. 涨幅较大的板块需警惕回调风险，考虑「持有」或「减配」。\n"
        "3. 跌幅较大的板块需判断是超跌反弹机会还是趋势性下跌。\n"
        "4. 每个板块必须同时列出至少3个利好因素和3个利空因素。\n"
        "5. 建议标准：基本面+资金面+政策面至少两项支撑才推荐「增配」。\n\n");

    // 注入大盘上下文和风险评估
    const MarketContext &mctx = result.marketCtx;
    if (mctx.valid) {
        prompt += "## 当前市场环境\n\n";
        prompt += QString("- 上证指数：%1 (%2%)\n").arg(mctx.shanghai.lastClose, 0, 'f', 2).arg(mctx.shanghai.changePct, 0, 'f', 2);
        prompt += QString("- 深证成指：%1 (%2%)\n").arg(mctx.shenzhen.lastClose, 0, 'f', 2).arg(mctx.shenzhen.changePct, 0, 'f', 2);
        prompt += QString("- 创业板指：%1 (%2%)\n").arg(mctx.chinext.lastClose, 0, 'f', 2).arg(mctx.chinext.changePct, 0, 'f', 2);
        if (mctx.northboundFlowValid)
            prompt += QString("- 北向资金今日净买入：%1 亿元（近5日均值 %2 亿元）\n")
                .arg(mctx.northboundNetBuy, 0, 'f', 2).arg(mctx.northbound5dAvg, 0, 'f', 2);
        prompt += QString("- 涨跌家数比：%1 / %2 =%3\n")
            .arg(mctx.advanceCount).arg(mctx.declineCount).arg(mctx.advanceDeclineRatio, 0, 'f', 2);
        prompt += QString("- 涨停/跌停：%1 / %2\n").arg(mctx.limitUpCount).arg(mctx.limitDownCount);
        prompt += QString("- 市场情绪评分：%1/100（%2）\n\n").arg(mctx.marketRiskScore, 0, 'f', 0).arg(mctx.riskLevel);
    }

    // 注入风险雷达
    const MarketRiskRadar &radar = result.riskRadar;
    prompt += "## 风险雷达\n";
    prompt += QString("- 综合风险：%1/100，估值风险 %2，动量风险 %3，资金面 %4，波动性 %5\n")
        .arg(radar.compositeRisk, 0, 'f', 0)
        .arg(radar.valuationRisk, 0, 'f', 0)
        .arg(radar.momentumRisk, 0, 'f', 0)
        .arg(radar.capitalFlowRisk, 0, 'f', 0)
        .arg(radar.volatilityRisk, 0, 'f', 0);
    prompt += "- " + radar.riskAdvice + "\n\n";

    // 注入轮动信号
    if (!result.rotationSignals.isEmpty()) {
        prompt += "## 板块轮动信号（前10）\n";
        int cnt = 0;
        for (const RotationSignal &rs : result.rotationSignals) {
            if (++cnt > 10) break;
            QString tag = rs.isRotatingIn ? "[轮入]" : (rs.isRotatingOut ? "[轮出]" : "[观察]");
            prompt += QString("- %1 %2：轮动分 %3，动量加速 %4%\n")
                .arg(tag).arg(rs.sector).arg(rs.rotationScore, 0, 'f', 2).arg(rs.momentumDelta, 0, 'f', 2);
        }
        prompt += "\n";
    }

    // 注入市场状态
    prompt += QString::fromUtf8("## 市场状态\n- 当前市场状态：%1（置信度 %2%）\n")
        .arg(result.marketRegime.regimeName).arg(result.marketRegime.regimeConfidence, 0, 'f', 0);
    prompt += QString::fromUtf8("- 动态因子权重偏移：动量 %1% / 估值 %2% / 情绪 %3% / 风险 %4% / 宽度 %5%\n\n")
        .arg(result.marketRegime.weights.momentumWeight * 100, 0, 'f', 0)
        .arg(result.marketRegime.weights.valuationWeight * 100, 0, 'f', 0)
        .arg(result.marketRegime.weights.sentimentWeight * 100, 0, 'f', 0)
        .arg(result.marketRegime.weights.riskWeight * 100, 0, 'f', 0)
        .arg(result.marketRegime.weights.breadthWeight * 100, 0, 'f', 0);

    // 注入趋势生命周期（核心新增：让AI理解每个板块的趋势阶段）
    prompt += QString::fromUtf8("## 趋势生命周期分析\n\n");
    prompt += QString::fromUtf8("**重要提示**：请结合以下趋势阶段和健康度数据进行分析。"
        "\"已经涨很多\"不等于\"不能买\"，关键看趋势健康度和阶段。"
        "同样\"已经跌很多\"不等于\"要抄底\"，需看是否企稳。\n\n");
    prompt += QString::fromUtf8("| 板块 | 趋势阶段 | 投资状态 | 健康度 | 过热 | 耗竭风险 | 资金模式 | 关键判断 |\n");
    prompt += "|------|---------|---------|--------|------|---------|---------|--------|\n";
    for (const SectorSnapshot *s : targetSectors) {
        prompt += QString("| %1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 |\n")
            .arg(s->industry)
            .arg(s->trendStageResult.stageName)
            .arg(s->explanation.stateName)
            .arg(s->trendHealth.sustainability, 0, 'f', 0)
            .arg(s->overheat.compositeOverheat, 0, 'f', 0)
            .arg(s->explanation.exhaustionRisk, 0, 'f', 0)
            .arg(s->flowStructure.flowPattern)
            .arg(s->explanation.conclusion.left(40));
    }
    prompt += "\n";

    prompt += "## 重点分析板块数据\n\n";
    prompt += "| 板块 | 今日涨跌 | 5日动量 | 20日动量 | 新闻情绪 | 正/负新闻 | 热度 | MACD | RSI6 | KDJ-J | 均线 | 量比 | 技术分 |\n";
    prompt += "|------|---------|---------|---------|---------|----------|------|------|------|-------|------|------|-------|\n";

    for (const SectorSnapshot *s : targetSectors) {
        const TechSignals &t = s->tech;
        QString macdTag = t.macdGoldenCross ? "金叉" : (t.macdDeadCross ? "死叉" : (t.macdHist > 0 ? "多" : "空"));
        QString maTag = t.maLongArrange ? "多头" : (t.maShortArrange ? "空头" : "交织");
        prompt += QString("| %1 | %2% | %3% | %4% | %5 | %6/%7 | %8 | %9 | %10 | %11 | %12 | %13 | %14 |\n")
            .arg(s->industry)
            .arg(s->todayChangePct, 0, 'f', 2)
            .arg(s->fiveDayMomentum, 0, 'f', 2)
            .arg(s->twentyDayMomentum, 0, 'f', 2)
            .arg(s->newsSentiment, 0, 'f', 2)
            .arg(s->positiveNewsCount)
            .arg(s->negativeNewsCount)
            .arg(s->sectorHotScore, 0, 'f', 1)
            .arg(macdTag)
            .arg(t.rsi6, 0, 'f', 1)
            .arg(t.kdjJ, 0, 'f', 1)
            .arg(maTag)
            .arg(t.volRatio, 0, 'f', 2)
            .arg(t.techScore, 0, 'f', 2);
    }

    prompt += "\n## 各板块相关新闻\n\n";
    for (const SectorSnapshot *s : targetSectors) {
        if (s->newsHeadlines.isEmpty()) continue;
        prompt += "### " + s->industry + "\n";
        for (const QString &headline : s->newsHeadlines) {
            prompt += "- " + headline + "\n";
        }
        prompt += "\n";
    }

    prompt += QString::fromUtf8(
        "## 输出要求\n\n"
        "请严格按照以下JSON格式输出（纯JSON，不要Markdown代码块标记）。\n\n"
        "### 顶层字段\n"
        "- **overall_summary**（400-600字）：全市场宏观研判综述。**必须结合上方提供的大盘指数走势、"
        "北向资金动向、涨跌家数比、风险雷达评分、板块轮动信号等全市场数据**，"
        "分析当日市场环境、资金流向特征、板块轮动态势、主要驱动因素、潜在风险点。"
        "要体现专业分析师的视角，必须引用具体的大盘数据和北向资金数据说话。\n"
        "- **methodology_note**（150-250字）：解释本报告使用的分析方法论——我们综合了大盘指数走势、"
        "北向资金流向、涨跌家数比等市场宏观数据，以及短期动量（5日/20日）、当日涨跌、新闻情绪分析、"
        "板块热度、MACD/RSI/KDJ等技术指标、板块轮动检测、风险雷达评估等多维度数据，"
        "配合均值回归修正构建预测模型。请用专业但通俗的语言向投资者解释这套方法的原理和局限性。\n\n"
        "### 每个板块字段\n"
        "- **analysis**（300-500字）：深度行业分析——当前走势解读、核心驱动因素、政策面研判、"
        "资金面特征、技术面形态、行业基本面变化。**必须结合大盘环境和北向资金方向来分析板块表现**。"
        "需要有具体的逻辑链条，不能泛泛而谈。\n"
        "- **positive_factors**：3-5个具体的看多因素，每条30-60字，包含具体数据或事件支撑\n"
        "- **negative_factors**：3-5个具体的看空/风险因素，每条30-60字\n"
        "- **action**：必须是 \"增配\"、\"持有\" 或 \"减配\" 三选一\n"
        "- **prediction_reason**（80-150字）：对该板块未来1-2周走势的研判理由，需考虑市场整体风险水平\n"
        "- **trend_summary**（4-8字）：对该板块当前态势的精炼概括，"
        "如「强势突破」「缩量盘整」「放量下探」「超跌反弹」「高位震荡」「加速赶顶」等专业术语\n"
        "- **investment_strategy**（100-200字）：针对该板块的投资策略建议，包括适合的投资者类型、"
        "建仓时机、仓位建议、止盈止损参考位、需要关注的风险事件等\n"
        "- **future_events**：一个字符串数组，列出未来1-3个月内该板块可能面临的重要事件或催化剂，"
        "每条40-80字，包含预计时间和预期影响。例如：\"4月下旬科技类公司一季报密集披露，业绩超预期"
        "个股或带动板块估值修复\"、\"5月国产大飞机商业首航一周年，航空制造板块关注度将提升\"。"
        "至少给出2-4个前瞻事件。\n\n"
        "### AI 协同可读性字段（只用于 UI 展示，不得覆盖规则评分或最终动作）\n"
        "- **readable_title**：16-30字，用普通投资者能理解的话概括核心事件或判断。\n"
        "- **summary**：一句话说明这件事是什么，避免堆关键词。\n"
        "- **why_it_matters**：一句话说明它为什么会影响市场或板块。\n"
        "- **impact_path**：用「事件 -> 传导变量 -> 板块影响 -> 验证点」写清楚路径。\n"
        "- **affected_sectors**：数组，列出受影响板块，可带方向，例如「半导体设备：正向」。\n"
        "- **primary_reason**：该板块最重要的一条机会理由，必须具体、可核对。\n"
        "- **primary_risk**：该板块最重要的一条风险或失效条件，必须具体、可核对。\n"
        "- **next_checkpoint**：下一步需要跟踪的数据、会议、财报或价格/资金验证点。\n"
        "- **confidence**：0-1的解释置信度。\n"
        "- **source_refs**：数组，引用输入新闻标题、来源或证据编号，不要编造不存在的来源。\n"
        "- **disagreement_notes**：当 AI 判断与规则动作或评分方向不一致时，用一句话说明分歧。\n\n");

    prompt += QString::fromUtf8(
        "JSON格式：\n"
        "{\n"
        "  \"overall_summary\": \"全市场研判...\",\n"
        "  \"methodology_note\": \"分析方法论说明...\",\n"
        "  \"sectors\": {\n"
        "    \"板块名\": {\n"
        "      \"analysis\": \"深度分析...\",\n"
        "      \"positive_factors\": [\"因素1\", \"因素2\", \"因素3\"],\n"
        "      \"negative_factors\": [\"因素1\", \"因素2\", \"因素3\"],\n"
        "      \"action\": \"增配/持有/减配\",\n"
        "      \"prediction_reason\": \"预测研判...\",\n"
        "      \"trend_summary\": \"4-8字趋势概括\",\n"
        "      \"investment_strategy\": \"投资策略建议...\",\n"
        "      \"future_events\": [\"未来事件1\", \"未来事件2\"],\n"
        "      \"readable_title\": \"用户可读事件标题\",\n"
        "      \"summary\": \"一句话说明事件是什么\",\n"
        "      \"why_it_matters\": \"一句话说明为什么重要\",\n"
        "      \"impact_path\": \"事件 -> 传导变量 -> 板块影响 -> 验证点\",\n"
        "      \"affected_sectors\": [\"板块A：正向\", \"板块B：负向\"],\n"
        "      \"primary_reason\": \"首要机会理由\",\n"
        "      \"primary_risk\": \"首要风险或失效条件\",\n"
        "      \"next_checkpoint\": \"下一观察点\",\n"
        "      \"confidence\": 0.78,\n"
        "      \"source_refs\": [\"新闻标题或来源\"],\n"
        "      \"disagreement_notes\": \"规则与 AI 分歧说明，可为空\"\n"
        "    }\n"
        "  }\n"
        "}\n");

    return prompt;
}

// 从 AI 提供商 HTTP 响应中提取 content 文本（处理不同 API 格式差异）
AIAnalyzer::ProviderResult AIAnalyzer::callProvider(const AIProvider &provider,
                                                     const QString &prompt,
                                                     int maxTokens) const
{
    ProviderResult pr;
    pr.providerName = provider.name;

    qDebug() << "[AIAnalyzer] Calling provider:" << provider.name
             << "model:" << provider.model
             << "url:" << provider.apiUrl;

    const QString systemMsg = QString::fromUtf8(
        "你是一位拥有20年经验、CFA持证的资深首席金融分析师，服务于顶级投研机构。"
        "你的分析以客观审慎、数据驱动著称。"
        "你的回复必须体现专业深度，用词精准，避免空洞套话。"
        "回复必须是纯JSON格式，不要包含任何Markdown标记。");

    QMap<QString, QString> headers;
    const QByteArray jsonBody = buildRequestBody(provider, systemMsg, prompt, maxTokens, 0.3, headers);
    const QString requestUrl = buildRequestUrl(provider);

    const int timeoutMs = provider.apiUrl.contains("deepseek") ? 300000 : 120000;
    const HttpClient::HttpResult httpResult = HttpClient::postJson(requestUrl, jsonBody, headers, timeoutMs);

    if (!httpResult.ok) {
        QString errorDetail = httpResult.errorMessage;
        if (!httpResult.body.isEmpty()) {
            QJsonDocument errDoc = QJsonDocument::fromJson(httpResult.body.toUtf8());
            if (errDoc.isObject()) {
                QJsonObject errObj = errDoc.object().value("error").toObject();
                if (!errObj.isEmpty()) {
                    errorDetail = errObj.value("message").toString();
                }
            }
        }
        qDebug() << "[AIAnalyzer] API call failed for" << provider.name
                 << "HTTP" << httpResult.statusCode << ":" << errorDetail;
        pr.errorMessage = QString("%1: HTTP %2 - %3")
            .arg(provider.name).arg(httpResult.statusCode).arg(errorDetail);
        return pr;
    }

    qDebug() << "[AIAnalyzer] HTTP response OK from" << provider.name
             << "body length:" << httpResult.body.size();

    QJsonDocument doc = QJsonDocument::fromJson(httpResult.body.toUtf8());
    if (!doc.isObject()) {
        qDebug() << "[AIAnalyzer] Invalid JSON response from" << provider.name;
        return pr;
    }

    QJsonObject responseObj = doc.object();
    if (responseObj.contains("error")) {
        const QString errMsg = responseObj.value("error").isObject()
            ? responseObj.value("error").toObject().value("message").toString()
            : responseObj.value("error").toString();
        qDebug() << "[AIAnalyzer] API error from" << provider.name << ":" << errMsg;
        pr.errorMessage = provider.name + ": " + errMsg;
        return pr;
    }

    QString content = extractContentFromResponse(responseObj, provider);
    if (content.isEmpty()) {
        qDebug() << "[AIAnalyzer] Empty content from" << provider.name;
        pr.errorMessage = provider.name + ": empty response content";
        return pr;
    }
    qDebug() << "[AIAnalyzer] Raw AI content length:" << content.size();

    int braceStart = content.indexOf('{');
    int braceEnd = content.lastIndexOf('}');
    if (braceStart < 0 || braceEnd <= braceStart) {
        qDebug() << "[AIAnalyzer] No valid JSON braces in response from" << provider.name;
        return pr;
    }
    content = content.mid(braceStart, braceEnd - braceStart + 1);

    QJsonDocument aiDoc = QJsonDocument::fromJson(content.toUtf8());
    if (!aiDoc.isObject()) {
        qDebug() << "[AIAnalyzer] Failed to parse AI JSON from" << provider.name;
        return pr;
    }

    QJsonObject aiObj = aiDoc.object();
    pr.overallSummary = aiObj.value("overall_summary").toString();
    pr.methodologyNote = aiObj.value("methodology_note").toString();
    pr.valid = true;

    QJsonObject sectorsObj = aiObj.value("sectors").toObject();
    qDebug() << "[AIAnalyzer] Parsed" << sectorsObj.keys().size() << "sectors from" << provider.name;

    for (const QString &key : sectorsObj.keys()) {
        QJsonObject so = sectorsObj.value(key).toObject();
        SectorAIResult sr;
        sr.analysis = so.value("analysis").toString();
        sr.predictionReason = so.value("prediction_reason").toString();
        sr.suggestedAction = so.value("action").toString();
        sr.trendSummary = so.value("trend_summary").toString();
        sr.investmentStrategy = so.value("investment_strategy").toString();
        for (const QJsonValue &v : so.value("positive_factors").toArray())
            sr.positiveFactors.push_back(v.toString());
        for (const QJsonValue &v : so.value("negative_factors").toArray())
            sr.negativeFactors.push_back(v.toString());
        for (const QJsonValue &v : so.value("future_events").toArray())
            sr.futureEvents.push_back(v.toString());
        sr.readable = readableInsightFromJson(so);

        pr.sectors[key] = sr;
    }

    qDebug() << "[AIAnalyzer] Provider" << provider.name << "analysis complete, valid:" << pr.valid;
    return pr;
}

void AIAnalyzer::aggregateResults(const QList<ProviderResult> &results,
                                   AnalysisResult &output,
                                   int maxSectors) const
{
    QList<ProviderResult> validResults;
    for (const ProviderResult &r : results) {
        if (r.valid) {
            validResults.push_back(r);
        } else if (!r.errorMessage.isEmpty()) {
            output.aiErrors.push_back(r.errorMessage);
        }
    }
    if (validResults.isEmpty()) {
        qDebug() << "[AIAnalyzer] No valid results from any provider";
        return;
    }

    output.aiAvailable = true;
    const bool multiProvider = validResults.size() > 1;

    qDebug() << "[AIAnalyzer] Aggregating results from" << validResults.size() << "providers";

    if (multiProvider) {
        output.aiOverallSummary = QString::fromUtf8("【多模型共识分析】\n\n");
        for (const ProviderResult &r : validResults) {
            output.aiOverallSummary += QString::fromUtf8("▎") + r.providerName
                + QString::fromUtf8(" 观点：\n") + r.overallSummary + "\n\n";
        }
        QStringList methodNotes;
        for (const ProviderResult &r : validResults) {
            if (!r.methodologyNote.isEmpty()) methodNotes.push_back(r.methodologyNote);
        }
        output.aiMethodologyNote = methodNotes.isEmpty() ? QString() : methodNotes.first();
    } else {
        output.aiOverallSummary = validResults.first().overallSummary;
        output.aiMethodologyNote = validResults.first().methodologyNote;
    }

    QSet<QString> deepAnalyzedSectors;
    for (const ProviderResult &r : validResults) {
        for (const QString &k : r.sectors.keys()) deepAnalyzedSectors.insert(k);
    }

    for (SectorSnapshot &s : output.sectors) {
        if (!deepAnalyzedSectors.contains(s.industry)) continue;

        QList<SectorAIResult> sectorResults;
        for (const ProviderResult &r : validResults) {
            if (r.sectors.contains(s.industry)) {
                sectorResults.push_back(r.sectors[s.industry]);
            }
        }
        if (sectorResults.isEmpty()) continue;

        s.aiInsight = mergeReadableInsights(sectorResults);
        for (const SectorAIResult &sr : sectorResults) {
            if (sr.suggestedAction.trimmed().isEmpty()) continue;
            const AdviceAction aiAction = actionFromAiLabel(sr.suggestedAction);
            if (aiAction != s.action) {
                if (s.aiInsight.disagreementNotes.isEmpty()) {
                    s.aiInsight.disagreementNotes = QString::fromUtf8("AI 倾向「%1」，规则动作保持「%2」；请以规则评分、事件证据和技术指标共同验证。")
                        .arg(sr.suggestedAction, ruleActionLabel(s.action));
                }
                s.aiInsight.valid = true;
                break;
            }
        }

        if (multiProvider && sectorResults.size() > 1) {
            s.aiAnalysis = QString::fromUtf8("【多模型对比分析】\n\n");
            int agreeIncrease = 0, agreeHold = 0, agreeDecrease = 0;

            for (int i = 0; i < sectorResults.size(); ++i) {
                const SectorAIResult &sr = sectorResults[i];
                const QString provName = validResults[i].providerName;
                s.aiAnalysis += QString::fromUtf8("▎") + provName
                    + QString::fromUtf8("（建议：") + sr.suggestedAction
                    + QString::fromUtf8("）\n") + sr.analysis + "\n\n";

                if (sr.suggestedAction == QString::fromUtf8("增配")) ++agreeIncrease;
                else if (sr.suggestedAction == QString::fromUtf8("减配")) ++agreeDecrease;
                else ++agreeHold;
            }

            int total = sectorResults.size();
            QString consensus;
            if (agreeIncrease == total)
                consensus = QString::fromUtf8("全部模型一致看多，建议增配");
            else if (agreeDecrease == total)
                consensus = QString::fromUtf8("全部模型一致看空，建议减配");
            else if (agreeHold == total)
                consensus = QString::fromUtf8("全部模型一致观望，建议持有");
            else {
                consensus = QString::fromUtf8("模型意见分歧（增配 %1/%4，持有 %2/%4，减配 %3/%4），建议谨慎对待")
                    .arg(agreeIncrease).arg(agreeHold).arg(agreeDecrease).arg(total);
            }
            s.aiAnalysis += QString::fromUtf8("▎共识结论：") + consensus;
            AdviceAction consensusAction = AdviceAction::Hold;
            if (agreeIncrease > agreeDecrease && agreeIncrease > agreeHold)
                consensusAction = AdviceAction::Increase;
            else if (agreeDecrease > agreeIncrease && agreeDecrease > agreeHold)
                consensusAction = AdviceAction::Decrease;
            if (consensusAction != s.action && s.aiInsight.disagreementNotes.isEmpty()) {
                s.aiInsight.disagreementNotes = consensus + QString::fromUtf8("；规则动作仍保持「%1」。")
                    .arg(ruleActionLabel(s.action));
                s.aiInsight.valid = true;
            }

            QList<QStringList> allPos, allNeg;
            QStringList allReasons, allTrends, allStrategies, allFutureEvents;
            for (const SectorAIResult &sr : sectorResults) {
                allPos.push_back(sr.positiveFactors);
                allNeg.push_back(sr.negativeFactors);
                allReasons.push_back(sr.predictionReason);
                if (!sr.trendSummary.isEmpty()) allTrends.push_back(sr.trendSummary);
                if (!sr.investmentStrategy.isEmpty()) allStrategies.push_back(sr.investmentStrategy);
                allFutureEvents.append(sr.futureEvents);
            }
            s.positiveFactors = deduplicateFactors(allPos);
            s.negativeFactors = deduplicateFactors(allNeg);
            s.aiPredictionReason = allReasons.join("\n\n");
            s.aiTrendSummary = allTrends.isEmpty() ? QString() : allTrends.first();
            if (!allStrategies.isEmpty())
                s.strategy.operationAdvice = allStrategies.join("\n\n");
            allFutureEvents.removeDuplicates();
            s.futureEventsAI = allFutureEvents;
        } else {
            const SectorAIResult &sr = sectorResults.first();
            s.aiAnalysis = sr.analysis;
            s.aiPredictionReason = sr.predictionReason;
            s.aiTrendSummary = sr.trendSummary;

            if (!sr.positiveFactors.isEmpty()) s.positiveFactors = sr.positiveFactors;
            if (!sr.negativeFactors.isEmpty()) s.negativeFactors = sr.negativeFactors;
            if (!sr.investmentStrategy.isEmpty())
                s.strategy.operationAdvice = sr.investmentStrategy;
            s.futureEventsAI = sr.futureEvents;
        }
    }
}

void AIAnalyzer::enhance(AnalysisResult &result, int maxSectors) const
{
    if (!isAvailable() || result.sectors.isEmpty()) return;

    const int effectiveMax = maxSectors > 0 ? maxSectors : m_deepAnalysisTopN;

    qDebug() << "[AIAnalyzer] Stage 2: Deep analysis with" << m_providers.size() << "providers"
             << "for top" << effectiveMax << "of" << result.sectors.size() << "sectors";

    const QString prompt = buildDeepPrompt(result, effectiveMax);
    qDebug() << "[AIAnalyzer] Stage 2 prompt length:" << prompt.size() << "chars";

    // 多 Provider 并行调用
    QList<QFuture<ProviderResult>> futures;
    for (const AIProvider &provider : m_providers) {
        futures.push_back(QtConcurrent::run([this, &provider, &prompt]() {
            return callProvider(provider, prompt);
        }));
    }

    QList<ProviderResult> allResults;
    for (QFuture<ProviderResult> &f : futures) {
        f.waitForFinished();
        allResults.push_back(f.result());
    }

    aggregateResults(allResults, result, effectiveMax);

    qDebug() << "[AIAnalyzer] Stage 2 complete. aiAvailable:" << result.aiAvailable
             << "overallSummary length:" << result.aiOverallSummary.size();
}

QString AIAnalyzer::chatQuery(const QString &userQuestion, const AnalysisResult &context) const
{
    if (!isAvailable()) return QString::fromUtf8("AI 未配置或不可用，无法回答问题。");

    QString contextBlock;
    contextBlock += QString::fromUtf8("## 当前市场数据（实时分析结果）\n\n");

    const MarketContext &m = context.marketCtx;
    if (m.valid) {
        contextBlock += QString("上证指数 %1 (%2%), 深证成指 %3 (%4%), 创业板指 %5 (%6%)\n")
            .arg(m.shanghai.lastClose, 0, 'f', 2).arg(m.shanghai.changePct, 0, 'f', 2)
            .arg(m.shenzhen.lastClose, 0, 'f', 2).arg(m.shenzhen.changePct, 0, 'f', 2)
            .arg(m.chinext.lastClose, 0, 'f', 2).arg(m.chinext.changePct, 0, 'f', 2);
        if (m.northboundFlowValid)
            contextBlock += QString("北向资金净买入: %1亿\n").arg(m.northboundNetBuy, 0, 'f', 2);
    }

    contextBlock += "\n## 各板块实时分析\n\n";
    int cnt = 0;
    for (const SectorSnapshot &s : context.sectors) {
        if (++cnt > 30) break;
        contextBlock += QString("### %1\n").arg(s.industry);
        contextBlock += QString("- 今日涨跌: %1%, 5日动量: %2%, 20日动量: %3%\n")
            .arg(s.todayChangePct, 0, 'f', 2).arg(s.fiveDayMomentum, 0, 'f', 2).arg(s.twentyDayMomentum, 0, 'f', 2);
        contextBlock += QString("- 预测评分: %1, 建议: %2\n")
            .arg(s.forecastScore, 0, 'f', 3)
            .arg(s.action == AdviceAction::Increase ? "增配" : (s.action == AdviceAction::Decrease ? "减配" : "持有"));
        if (s.tech.rsi6 > 0)
            contextBlock += QString("- 技术指标: RSI6=%1, MACD=%2, KDJ-J=%3\n")
                .arg(s.tech.rsi6, 0, 'f', 1).arg(s.tech.macdHist > 0 ? "多" : "空").arg(s.tech.kdjJ, 0, 'f', 1);
        if (!s.investmentRecommendation.isEmpty())
            contextBlock += "- 策略信号: " + s.investmentRecommendation + "\n";
        if (!s.newsHeadlines.isEmpty()) {
            contextBlock += "- 相关新闻: ";
            int nc = 0;
            for (const QString &h : s.newsHeadlines) {
                if (++nc > 3) break;
                contextBlock += h + "; ";
            }
            contextBlock += "\n";
        }
        contextBlock += "\n";
    }

    if (!context.aiOverallSummary.isEmpty()) {
        contextBlock += "\n## AI总结\n" + context.aiOverallSummary + "\n\n";
    }

    const AIProvider &provider = m_providers.first();
    const QString systemMsg = QString::fromUtf8(
        "你是InvestInsight智能投资助手，拥有CFA资格和20年A股投研经验。"
        "你可以访问到用户app中实时采集的行情数据、新闻、技术指标、策略回测结果等信息。"
        "请基于提供的数据回答用户问题，给出专业、客观、有数据支撑的回答。"
        "如果用户问的问题超出了提供的数据范围，请诚实告知。"
        "回复使用简洁清晰的中文，直接回答问题，不要输出JSON格式。");

    const QString fullPrompt = contextBlock + "\n---\n\n用户提问：" + userQuestion;

    QMap<QString, QString> headers;
    const QByteArray jsonBody = buildRequestBody(provider, systemMsg, fullPrompt, 2000, 0.5, headers);
    const QString requestUrl = buildRequestUrl(provider);
    const int timeoutMs = provider.apiUrl.contains("deepseek") ? 120000 : 60000;
    const HttpClient::HttpResult httpResult = HttpClient::postJson(requestUrl, jsonBody, headers, timeoutMs);

    if (!httpResult.ok) {
        return QString::fromUtf8("AI 请求失败：") + httpResult.errorMessage;
    }

    QJsonDocument doc = QJsonDocument::fromJson(httpResult.body.toUtf8());
    if (!doc.isObject()) return QString::fromUtf8("AI 返回格式异常");

    return extractContentFromResponse(doc.object(), provider);
}
