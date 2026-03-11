#ifndef DESKTOP_H
#define DESKTOP_H

#include <QWidget>
#include "terminal.h"
#include "sidebar.h"
#include "llm_client.h"

class SerialWorker;
class QLabel;

/*
 * SwanOS — Main Desktop
 * Composes sidebar + tab bar + terminal + status bar.
 */

class Desktop : public QWidget {
    Q_OBJECT

public:
    explicit Desktop(const QString &user, bool demo,
                     SerialWorker *worker = nullptr,
                     QWidget *parent = nullptr);

    Terminal *terminal() const { return m_terminal; }

private slots:
    void onCommand(const QString &cmd);
    void onSerialData(const QString &data);
    void onLlmQuery(const QString &query);

private:
    void buildUi();
    void demoCommand(const QString &cmd);

    QString       m_user;
    bool          m_demo;
    SerialWorker *m_worker = nullptr;
    Terminal     *m_terminal = nullptr;
    Sidebar      *m_sidebar  = nullptr;
    QLabel       *m_clock    = nullptr;
    LlmClient     m_llm;
};

#endif // DESKTOP_H
