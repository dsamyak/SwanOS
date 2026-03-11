#ifndef TERMINAL_H
#define TERMINAL_H

#include <QTextEdit>
#include <QStringList>

/*
 * SwanOS — Rich Terminal Widget
 * Custom text editor with prompt, command history, and serial mode.
 */

class Terminal : public QTextEdit {
    Q_OBJECT

public:
    explicit Terminal(const QString &user = QStringLiteral("admin"),
                      QWidget *parent = nullptr);

    void setSerial(bool on) { m_serial = on; }
    void prompt();
    void out(const QString &text, const QColor &color = QColor());
    void serialChar(const QString &ch);
    void setAnsi(int code);
    void aiOut(const QString &text);
    void setUser(const QString &user) { m_user = user; }

signals:
    void commandEntered(const QString &cmd);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void replaceInput(const QString &text);

    QString     m_user;
    bool        m_serial = false;
    QStringList m_history;
    int         m_histIdx  = -1;
    int         m_promptPos = 0;
    QColor      m_serialColor;
};

#endif // TERMINAL_H
