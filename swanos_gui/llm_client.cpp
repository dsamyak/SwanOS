#include "llm_client.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>

LlmClient::LlmClient(QObject *parent)
    : QObject(parent)
    , m_apiUrl(QStringLiteral("https://api.groq.com/openai/v1/chat/completions"))
    , m_model(QStringLiteral("llama-3.3-70b-versatile"))
    , m_systemPrompt(QStringLiteral(
          "You are SwanOS AI, the intelligence inside a bare-metal operating system. "
          "Be concise and helpful. Keep responses under 200 words. "
          "You are running directly on x86 hardware with no other OS underneath."))
{
    loadApiKey();
}

void LlmClient::loadApiKey()
{
    // Try environment variable first
    m_apiKey = QString::fromUtf8(qgetenv("GROQ_API_KEY"));
    if (!m_apiKey.isEmpty()) return;

    // Try .env file next to executable
    QString envPath = QCoreApplication::applicationDirPath() + QStringLiteral("/../.env");
    QFile envFile(envPath);
    if (!envFile.exists()) {
        envPath = QCoreApplication::applicationDirPath() + QStringLiteral("/.env");
        envFile.setFileName(envPath);
    }

    if (envFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&envFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith(QStringLiteral("GROQ_API_KEY="))) {
                m_apiKey = line.mid(13).trimmed();
                break;
            }
        }
        envFile.close();
    }
}

QString LlmClient::query(const QString &prompt)
{
    if (m_apiKey.isEmpty()) {
        return QStringLiteral("Error: GROQ_API_KEY not set. Set it in .env file or environment.");
    }

    // Build JSON body
    QJsonObject sysMsg;
    sysMsg[QStringLiteral("role")]    = QStringLiteral("system");
    sysMsg[QStringLiteral("content")] = m_systemPrompt;

    QJsonObject userMsg;
    userMsg[QStringLiteral("role")]    = QStringLiteral("user");
    userMsg[QStringLiteral("content")] = prompt;

    QJsonArray messages;
    messages.append(sysMsg);
    messages.append(userMsg);

    QJsonObject body;
    body[QStringLiteral("model")]       = m_model;
    body[QStringLiteral("messages")]    = messages;
    body[QStringLiteral("max_tokens")]  = 512;
    body[QStringLiteral("temperature")] = 0.7;

    QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    // Make synchronous HTTP POST
    QNetworkAccessManager mgr;
    QNetworkRequest req(QUrl(m_apiUrl));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());

    QNetworkReply *reply = mgr.post(req, payload);

    // Block until complete (30s timeout)
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(30000);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        return QStringLiteral("Error: API request timed out.");
    }

    QByteArray responseData = reply->readAll();
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (statusCode != 200) {
        return QStringLiteral("API error %1: %2")
            .arg(statusCode)
            .arg(QString::fromUtf8(responseData.left(200)));
    }

    // Parse JSON response
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull()) {
        return QStringLiteral("Error: Failed to parse API response.");
    }

    QJsonObject obj = doc.object();
    QJsonArray choices = obj[QStringLiteral("choices")].toArray();
    if (choices.isEmpty()) {
        return QStringLiteral("Error: No response from AI.");
    }

    return choices[0].toObject()
        [QStringLiteral("message")].toObject()
        [QStringLiteral("content")].toString();
}
