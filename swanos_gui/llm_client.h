#ifndef LLM_CLIENT_H
#define LLM_CLIENT_H

#include <QObject>
#include <QString>

/*
 * SwanOS — LLM Client
 * Calls the Groq API to get AI responses.
 */

class LlmClient : public QObject {
    Q_OBJECT

public:
    explicit LlmClient(QObject *parent = nullptr);

    // Blocking call — use from worker threads or demo mode
    QString query(const QString &prompt);

    bool hasApiKey() const { return !m_apiKey.isEmpty(); }

private:
    void loadApiKey();

    QString m_apiKey;
    QString m_apiUrl;
    QString m_model;
    QString m_systemPrompt;
};

#endif // LLM_CLIENT_H
