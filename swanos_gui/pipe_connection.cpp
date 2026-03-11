#include "pipe_connection.h"
#include <QDebug>

PipeConnection::PipeConnection(const QString &pipePath)
    : m_pipePath(pipePath)
{
}

PipeConnection::~PipeConnection()
{
    close();
}

bool PipeConnection::connect()
{
#ifdef _WIN32
    m_handle = CreateFileW(
        reinterpret_cast<LPCWSTR>(m_pipePath.utf16()),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (m_handle == INVALID_HANDLE_VALUE) {
        qWarning() << "[Pipe] Connect failed, error:" << GetLastError();
        return false;
    }

    // Set pipe to byte mode
    DWORD mode = PIPE_READMODE_BYTE;
    SetNamedPipeHandleState(m_handle, &mode, nullptr, nullptr);

    qDebug() << "[Pipe] Connected to" << m_pipePath;
    return true;
#else
    qWarning() << "[Pipe] Named pipes only supported on Windows";
    return false;
#endif
}

QByteArray PipeConnection::read(int size)
{
#ifdef _WIN32
    if (m_handle == INVALID_HANDLE_VALUE) return {};

    QByteArray buf(size, '\0');
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(m_handle, buf.data(), size, &bytesRead, nullptr);
    if (!ok || bytesRead == 0) return {};
    buf.resize(bytesRead);
    return buf;
#else
    Q_UNUSED(size);
    return {};
#endif
}

void PipeConnection::write(const QByteArray &data)
{
#ifdef _WIN32
    if (m_handle == INVALID_HANDLE_VALUE) return;

    DWORD written = 0;
    WriteFile(m_handle, data.constData(), data.size(), &written, nullptr);
#else
    Q_UNUSED(data);
#endif
}

void PipeConnection::write(const QString &text)
{
    write(text.toLatin1());
}

void PipeConnection::close()
{
#ifdef _WIN32
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
#endif
}

bool PipeConnection::isConnected() const
{
#ifdef _WIN32
    return m_handle != INVALID_HANDLE_VALUE;
#else
    return false;
#endif
}
