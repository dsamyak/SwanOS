#include "serial_worker.h"
#include <QThread>
#include <QDebug>

SerialWorker::SerialWorker(const QString &pipePath, QObject *parent)
    : QObject(parent), m_pipe(pipePath)
{
}

void SerialWorker::startReading()
{
    while (m_running) {
        qDebug() << "[Serial] Connecting to pipe...";
        if (m_pipe.connect()) {
            qDebug() << "[Serial] Connected!";
            emit connectionStatus(true);
            readLoop();
            emit connectionStatus(false);
        } else {
            QThread::msleep(2000);
        }
    }
}

void SerialWorker::readLoop()
{
    enum State { Normal, Marker, Query, Ansi };
    State state = Normal;
    QString queryBuf;
    QString ansiBuf;

    while (m_running) {
        QByteArray data = m_pipe.read(1);
        if (data.isEmpty()) {
            QThread::msleep(10);
            continue;
        }

        char ch = data.at(0);

        switch (state) {
        case Normal:
            if (ch == '\x01') {
                state = Marker;
            } else if (ch == '\x1b') {
                state = Ansi;
                ansiBuf = QStringLiteral("\x1b");
            } else {
                emit dataReceived(QString(QChar::fromLatin1(ch)));
            }
            break;

        case Marker:
            if (ch == 'Q') {
                state = Query;
                queryBuf.clear();
            } else {
                state = Normal;
            }
            break;

        case Query:
            if (ch == '\x04') {
                emit llmQueryReceived(queryBuf);
                state = Normal;
            } else {
                queryBuf += QChar::fromLatin1(ch);
            }
            break;

        case Ansi:
            ansiBuf += QChar::fromLatin1(ch);
            if (ch == 'm' || ansiBuf.length() > 10) {
                emit dataReceived(ansiBuf);
                state = Normal;
            }
            break;
        }
    }
}

void SerialWorker::sendChar(char ch)
{
    m_pipe.write(QByteArray(1, ch));
}

void SerialWorker::sendResponse(const QString &text)
{
    QByteArray payload = text.toLatin1();
    payload.append('\x04');
    m_pipe.write(payload);
}

void SerialWorker::stop()
{
    m_running = false;
    m_pipe.close();
}
