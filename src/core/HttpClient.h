#pragma once

#include <QMap>
#include <QString>

class HttpClient
{
public:
    struct HttpResult
    {
        bool ok = false;
        int statusCode = 0;
        QString body;
        QString errorMessage;
    };

    static HttpResult get(const QString &url, int timeoutMs = 8000, int retries = 1);
    static HttpResult getGbk(const QString &url, int timeoutMs = 8000, int retries = 1);
    static HttpResult postJson(const QString &url, const QByteArray &jsonBody,
        const QMap<QString, QString> &headers = {}, int timeoutMs = 60000);
};
