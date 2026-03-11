#include "terminal.h"
#include "colors.h"

#include <QKeyEvent>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QScrollBar>

Terminal::Terminal(const QString &user, QWidget *parent)
    : QTextEdit(parent), m_user(user), m_serialColor(Colors::T1)
{
    setStyleSheet(
        QStringLiteral("QTextEdit{background:%1;color:%2;border:none;padding:12px;"
                        "font-family:%3;font-size:13px;}")
            .arg(Colors::hex(Colors::BG0),
                 Colors::hex(Colors::T1),
                 Colors::FONT_MONO));
    setAcceptRichText(false);
}

void Terminal::prompt()
{
    if (m_serial) return;

    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);

    QTextCharFormat fUser;
    fUser.setForeground(Colors::GREEN);
    fUser.setFontFamily(QStringLiteral("Cascadia Code"));
    c.insertText(QStringLiteral("  %1").arg(m_user), fUser);

    QTextCharFormat fChev;
    fChev.setForeground(Colors::CYAN);
    c.insertText(QStringLiteral(" \u276F "), fChev); // ❯

    setTextCursor(c);
    m_promptPos = c.position();
}

void Terminal::out(const QString &text, const QColor &color)
{
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);

    QTextCharFormat fmt;
    fmt.setFontFamily(QStringLiteral("Cascadia Code"));
    fmt.setForeground(color.isValid() ? color : Colors::T2);
    c.insertText(text, fmt);

    setTextCursor(c);
    ensureCursorVisible();
}

void Terminal::serialChar(const QString &ch)
{
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);

    QTextCharFormat fmt;
    fmt.setFontFamily(QStringLiteral("Cascadia Code"));
    fmt.setForeground(m_serialColor);
    c.insertText(ch, fmt);

    setTextCursor(c);
    ensureCursorVisible();
    m_promptPos = c.position();
}

void Terminal::setAnsi(int code)
{
    static const QMap<int, QColor> map = {
        {30, Colors::T3}, {31, Colors::RED}, {32, Colors::GREEN},
        {33, Colors::ORANGE}, {34, Colors::BLUE}, {35, Colors::PURPLE},
        {36, Colors::CYAN}, {37, Colors::T1},
        {90, Colors::T3}, {91, Colors::RED}, {92, Colors::GREEN},
        {93, Colors::ORANGE}, {94, Colors::BLUE}, {95, Colors::PURPLE},
        {96, Colors::CYAN}, {97, Colors::T1}
    };
    m_serialColor = map.value(code, Colors::T1);
}

void Terminal::aiOut(const QString &text)
{
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);

    QTextCharFormat fLabel;
    fLabel.setForeground(Colors::CYAN);
    fLabel.setFontFamily(QStringLiteral("Cascadia Code"));
    c.insertText(QStringLiteral("\n  SwanOS AI \u276F "), fLabel);

    QTextCharFormat fText;
    fText.setForeground(Colors::T1);
    fText.setFontFamily(QStringLiteral("Cascadia Code"));
    c.insertText(text + QStringLiteral("\n"), fText);

    setTextCursor(c);
    ensureCursorVisible();
}

void Terminal::keyPressEvent(QKeyEvent *event)
{
    QTextCursor c = textCursor();

    // Prevent editing before prompt
    if (c.position() < m_promptPos) {
        c.movePosition(QTextCursor::End);
        setTextCursor(c);
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        c.movePosition(QTextCursor::End);
        setTextCursor(c);

        if (m_serial) {
            c.setPosition(m_promptPos);
            c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            emit commandEntered(c.selectedText());
            m_promptPos = textCursor().position();
        } else {
            c.setPosition(m_promptPos);
            c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            QString cmd = c.selectedText().trimmed();
            out(QStringLiteral("\n"));

            if (!cmd.isEmpty()) {
                m_history.append(cmd);
                m_histIdx = m_history.size();
                emit commandEntered(cmd);
            } else {
                prompt();
            }
        }
    }
    else if (event->key() == Qt::Key_Backspace) {
        if (c.position() > m_promptPos) {
            if (m_serial) emit commandEntered(QStringLiteral("\b"));
            QTextEdit::keyPressEvent(event);
        }
    }
    else if (event->key() == Qt::Key_Up && !m_serial) {
        if (!m_history.isEmpty() && m_histIdx > 0) {
            --m_histIdx;
            replaceInput(m_history[m_histIdx]);
        }
    }
    else if (event->key() == Qt::Key_Down && !m_serial) {
        if (m_histIdx < m_history.size() - 1) {
            ++m_histIdx;
            replaceInput(m_history[m_histIdx]);
        } else {
            m_histIdx = m_history.size();
            replaceInput(QString());
        }
    }
    else {
        if (m_serial && !event->text().isEmpty()) {
            for (const QChar &ch : event->text())
                emit commandEntered(QString(ch));
        }
        QTextEdit::keyPressEvent(event);
    }
}

void Terminal::replaceInput(const QString &text)
{
    QTextCursor c = textCursor();
    c.setPosition(m_promptPos);
    c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    QTextCharFormat fmt;
    fmt.setForeground(Colors::T1);
    fmt.setFontFamily(QStringLiteral("Cascadia Code"));
    c.removeSelectedText();
    c.insertText(text, fmt);
    setTextCursor(c);
}
