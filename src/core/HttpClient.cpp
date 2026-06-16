#include "core/HttpClient.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringDecoder>
#else
#include <QTextCodec>
#endif
#include <QThread>
#include <QTimer>

static QString urlTag(const QString &url)
{
    const int qPos = url.indexOf('?');
    if (qPos < 0) return url.right(60);
    QString path = url.left(qPos);
    const int sPos = path.lastIndexOf('/');
    if (sPos >= 0) path = path.mid(sPos + 1);
    QString params = url.mid(qPos + 1);
    if (params.length() > 80) params = params.left(80) + "...";
    return path + "?" + params;
}

HttpClient::HttpResult HttpClient::get(const QString &url, int timeoutMs, int retries)
{
    HttpResult lastResult;
    for (int attempt = 0; attempt <= retries; ++attempt) {
        QElapsedTimer elapsed;
        elapsed.start();

        QNetworkAccessManager manager;
        QNetworkRequest request{QUrl(url)};
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36");
        request.setRawHeader("Accept", "*/*");
        request.setRawHeader("Connection", "keep-alive");
        request.setRawHeader("Referer", "https://quote.eastmoney.com/");

        QNetworkReply *reply = manager.get(request);
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
            if (reply->isRunning()) {
                reply->abort();
            }
        });

        timeoutTimer.start(timeoutMs);
        loop.exec();

        const qint64 ms = elapsed.elapsed();
        const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        lastResult.statusCode = status.isValid() ? status.toInt() : 0;

        if (reply->error() == QNetworkReply::NoError && lastResult.statusCode >= 200 && lastResult.statusCode < 300) {
            lastResult.ok = true;
            lastResult.body = QString::fromUtf8(reply->readAll());
            lastResult.errorMessage.clear();
            qDebug() << "[HTTP OK]" << urlTag(url) << "attempt=" << attempt
                     << "status=" << lastResult.statusCode
                     << "body=" << lastResult.body.size() << "chars"
                     << "time=" << ms << "ms";
            reply->deleteLater();
            return lastResult;
        }

        lastResult.ok = false;
        lastResult.errorMessage = reply->errorString();
        const QByteArray rawBody = reply->readAll();
        lastResult.body.clear();
        qDebug() << "[HTTP FAIL]" << urlTag(url) << "attempt=" << attempt
                 << "status=" << lastResult.statusCode
                 << "error=" << reply->error() << reply->errorString()
                 << "rawBody=" << rawBody.size() << "bytes"
                 << "time=" << ms << "ms";
        reply->deleteLater();

        if (attempt < retries) {
            QThread::msleep(200 * (attempt + 1));
        }
    }

    qDebug() << "[HTTP EXHAUSTED]" << urlTag(url) << "所有重试均失败";
    return lastResult;
}

HttpClient::HttpResult HttpClient::postJson(const QString &url, const QByteArray &jsonBody,
    const QMap<QString, QString> &headers, int timeoutMs)
{
    HttpResult lastResult;
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("User-Agent", "InvestInsight/1.0 (Qt)");
    for (auto it = headers.cbegin(); it != headers.cend(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    QNetworkReply *reply = manager.post(request, jsonBody);
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
        if (reply->isRunning()) reply->abort();
    });

    timeoutTimer.start(timeoutMs);
    loop.exec();

    const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    lastResult.statusCode = status.isValid() ? status.toInt() : 0;

    const QByteArray rawBody = reply->readAll();
    if (reply->error() == QNetworkReply::NoError && lastResult.statusCode >= 200 && lastResult.statusCode < 300) {
        lastResult.ok = true;
        lastResult.body = QString::fromUtf8(rawBody);
        lastResult.errorMessage.clear();
    } else {
        lastResult.ok = false;
        lastResult.body = QString::fromUtf8(rawBody);
        lastResult.errorMessage = reply->errorString();
    }
    reply->deleteLater();
    return lastResult;
}

HttpClient::HttpResult HttpClient::getGbk(const QString &url, int timeoutMs, int retries)
{
    HttpResult lastResult;
    for (int attempt = 0; attempt <= retries; ++attempt) {
        QNetworkAccessManager manager;
        QNetworkRequest request{QUrl(url)};
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36");
        request.setRawHeader("Accept", "*/*");
        request.setRawHeader("Referer", "https://finance.sina.com.cn/");

        QNetworkReply *reply = manager.get(request);
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
            if (reply->isRunning()) reply->abort();
        });

        timeoutTimer.start(timeoutMs);
        loop.exec();

        const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        lastResult.statusCode = status.isValid() ? status.toInt() : 0;

        if (reply->error() == QNetworkReply::NoError && lastResult.statusCode >= 200 && lastResult.statusCode < 300) {
            const QByteArray raw = reply->readAll();
            lastResult.ok = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QStringDecoder decoder("GBK");
            lastResult.body = decoder(raw);
#else
            QTextCodec *codec = QTextCodec::codecForName("GBK");
            lastResult.body = codec ? codec->toUnicode(raw) : QString::fromUtf8(raw);
#endif
            lastResult.errorMessage.clear();
            reply->deleteLater();
            return lastResult;
        }

        lastResult.ok = false;
        lastResult.body.clear();
        lastResult.errorMessage = reply->errorString();
        reply->deleteLater();
    }
    return lastResult;
}
