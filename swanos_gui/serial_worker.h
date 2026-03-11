#ifndef SERIAL_WORKER_H
#define SERIAL_WORKER_H

#include <QObject>
#include "pipe_connection.h"

/*
 * SwanOS — Serial Worker
 * Reads from VirtualBox pipe in a background thread,
 * emits signals for GUI updates.
 */

class SerialWorker : public QObject {
    Q_OBJECT

public:
    explicit SerialWorker(const QString &pipePath, QObject *parent = nullptr);

signals:
    void dataReceived(const QString &data);
    void llmQueryReceived(const QString &query);
    void connectionStatus(bool connected);

public slots:
    void startReading();
    void sendChar(char ch);
    void sendResponse(const QString &text);
    void stop();

private:
    void readLoop();

    PipeConnection m_pipe;
    bool m_running = true;
};

#endif // SERIAL_WORKER_H
