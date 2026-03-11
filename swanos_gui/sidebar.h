#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <QWidget>
#include <QLabel>
#include <QTimer>

/*
 * SwanOS — Sidebar Panel
 * System info, uptime, quick actions.
 */

class Sidebar : public QWidget {
    Q_OBJECT

public:
    explicit Sidebar(const QString &user = QStringLiteral("admin"),
                     QWidget *parent = nullptr);

    void setConnection(bool online);
    qint64 startTime() const { return m_startTime; }

private slots:
    void updateUptime();

private:
    void buildUi();
    QLabel *makeSection(const QString &title);
    QLabel *makeRow(const QString &icon, const QString &key,
                    const QString &val, QLayout *parent);

    QString m_user;
    QLabel *m_uptimeVal  = nullptr;
    QLabel *m_statusDot  = nullptr;
    QLabel *m_statusText = nullptr;
    QTimer  m_uptimeTimer;
    qint64  m_startTime;
};

#endif // SIDEBAR_H
