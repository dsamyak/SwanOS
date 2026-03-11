#ifndef PIPE_CONNECTION_H
#define PIPE_CONNECTION_H

#include <QByteArray>
#include <QString>

#ifdef _WIN32
#include <windows.h>
#endif

/*
 * SwanOS — Windows Named Pipe Connection
 * Handles I/O with VirtualBox serial port via named pipe.
 */

class PipeConnection {
public:
    explicit PipeConnection(const QString &pipePath);
    ~PipeConnection();

    bool    connect();
    QByteArray read(int size = 1);
    void    write(const QByteArray &data);
    void    write(const QString &text);
    void    close();
    bool    isConnected() const;

private:
    QString m_pipePath;
#ifdef _WIN32
    HANDLE  m_handle = INVALID_HANDLE_VALUE;
#endif
};

#endif // PIPE_CONNECTION_H
