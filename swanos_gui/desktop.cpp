#include "desktop.h"
#include "serial_worker.h"
#include "colors.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include <QApplication>
#include <QThread>

Desktop::Desktop(const QString &user, bool demo,
                 SerialWorker *worker, QWidget *parent)
    : QWidget(parent), m_user(user), m_demo(demo), m_worker(worker)
{
    buildUi();

    if (m_worker) {
        connect(m_worker, &SerialWorker::dataReceived,
                this, &Desktop::onSerialData);
        connect(m_worker, &SerialWorker::llmQueryReceived,
                this, &Desktop::onLlmQuery);
        connect(m_worker, &SerialWorker::connectionStatus,
                m_sidebar, &Sidebar::setConnection);
    }
}

void Desktop::buildUi()
{
    using namespace Colors;

    auto *lo = new QHBoxLayout(this);
    lo->setContentsMargins(0, 0, 0, 0);
    lo->setSpacing(0);

    // Sidebar
    m_sidebar = new Sidebar(m_user);
    if (m_demo) m_sidebar->setConnection(false);
    lo->addWidget(m_sidebar);

    // Main panel
    auto *mainPanel = new QWidget();
    mainPanel->setStyleSheet(QStringLiteral("background:%1;").arg(hex(BG0)));
    auto *ml = new QVBoxLayout(mainPanel);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(0);

    // Tab bar
    auto *tabBar = new QFrame();
    tabBar->setFixedHeight(40);
    tabBar->setStyleSheet(
        QStringLiteral("QFrame{background:%1;border-bottom:1px solid %2;}")
            .arg(hex(BG2), hex(BRD)));
    auto *tl = new QHBoxLayout(tabBar);
    tl->setContentsMargins(16, 0, 16, 0);

    auto *tab = new QLabel(QStringLiteral("\u25C9  Terminal")); // ◉
    tab->setStyleSheet(
        QStringLiteral("color:%1;font-size:12px;font-weight:600;padding:4px 12px;"
                        "background:%2;border-radius:6px;border:1px solid %3;")
            .arg(hex(CYAN), hex(BG0), hex(BRD)));
    tl->addWidget(tab);
    tl->addStretch();

    // Connection indicator
    auto *connDot = new QLabel(QStringLiteral("\xE2\x97\x8F")); // ●
    connDot->setStyleSheet(
        QStringLiteral("color:%1;font-size:8px;")
            .arg(hex(m_demo ? ORANGE : GREEN)));
    tl->addWidget(connDot);

    auto *connLabel = new QLabel(m_demo ? QStringLiteral("Demo Mode")
                                        : QStringLiteral("VirtualBox"));
    connLabel->setStyleSheet(
        QStringLiteral("color:%1;font-size:10px;").arg(hex(T3)));
    tl->addWidget(connLabel);
    ml->addWidget(tabBar);

    // Terminal
    m_terminal = new Terminal(m_user);
    if (!m_demo && m_worker) m_terminal->setSerial(true);
    ml->addWidget(m_terminal);

    // Status bar
    auto *statusBar = new QFrame();
    statusBar->setFixedHeight(28);
    statusBar->setStyleSheet(
        QStringLiteral("QFrame{background:%1;border-top:1px solid %2;}")
            .arg(hex(BG2), hex(BRD)));
    auto *sl = new QHBoxLayout(statusBar);
    sl->setContentsMargins(12, 0, 12, 0);

    struct StatusItem { QString text; QColor color; };
    StatusItem items[] = {
        {QStringLiteral("\xF0\x9F\xA6\xA2 SwanOS"), T1}, // 🦢
        {QStringLiteral("\xE2\x94\x82"),              BRD}, // │
        {QStringLiteral("Groq LLM"),                  T3},
        {QStringLiteral("\xE2\x94\x82"),              BRD}, // │
        {QStringLiteral("x86"),                       T3},
    };
    for (const auto &item : items) {
        auto *lbl = new QLabel(item.text);
        lbl->setStyleSheet(QStringLiteral("color:%1;font-size:10px;").arg(hex(item.color)));
        sl->addWidget(lbl);
    }
    sl->addStretch();

    m_clock = new QLabel();
    m_clock->setStyleSheet(QStringLiteral("color:%1;font-size:10px;").arg(hex(T3)));
    sl->addWidget(m_clock);

    auto *clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, [this]() {
        m_clock->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
    });
    clockTimer->start(1000);
    m_clock->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));

    ml->addWidget(statusBar);
    lo->addWidget(mainPanel);

    // Connect terminal commands
    connect(m_terminal, &Terminal::commandEntered, this, &Desktop::onCommand);

    // Demo welcome
    if (m_demo) {
        m_terminal->out(QStringLiteral("\n  Welcome to Swan OS, %1!\n").arg(m_user), CYAN);
        m_terminal->out(QStringLiteral("  Type help for commands, ask <question> to talk to AI.\n\n"), T2);
        m_terminal->prompt();
    }
}

void Desktop::onCommand(const QString &cmd)
{
    if (!m_demo && m_worker) {
        // Serial mode: forward keystrokes to kernel
        if (cmd == QStringLiteral("\b")) {
            m_worker->sendChar('\b');
        } else if (cmd.length() == 1) {
            m_worker->sendChar(cmd.at(0).toLatin1());
        } else {
            for (const QChar &ch : cmd)
                m_worker->sendChar(ch.toLatin1());
            m_worker->sendChar('\n');
        }
        return;
    }

    // Demo mode: process locally
    demoCommand(cmd);
}

void Desktop::onSerialData(const QString &data)
{
    if (data.startsWith(QStringLiteral("\x1b[")) && data.endsWith(QStringLiteral("m"))) {
        bool ok = false;
        int code = data.mid(2, data.length() - 3).toInt(&ok);
        if (ok) m_terminal->setAnsi(code);
    } else {
        m_terminal->serialChar(data);
    }
}

void Desktop::onLlmQuery(const QString &query)
{
    // Run LLM in background thread
    auto *thread = QThread::create([this, query]() {
        QString response = m_llm.query(query);
        if (m_worker) m_worker->sendResponse(response);
    });
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void Desktop::demoCommand(const QString &cmd)
{
    using namespace Colors;
    auto *t = m_terminal;

    QStringList parts = cmd.split(QRegularExpression(QStringLiteral("\\s+")),
                                  Qt::SkipEmptyParts);
    QString c = parts.isEmpty() ? QString() : parts[0].toLower();
    QString a = parts.size() > 1 ? parts.mid(1).join(QStringLiteral(" ")) : QString();

    if (c == QStringLiteral("help")) {
        t->out(QStringLiteral("\n  SwanOS Commands\n"), CYAN);
        t->out(QStringLiteral("  \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
                               "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
                               "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
                               "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
                               "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
                               "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
                               "\xE2\x94\x80\n"), BRD); // ─────────────────────────
        t->out(QStringLiteral("  ask <question>   Ask AI\n"), T1);
        t->out(QStringLiteral("  ls / cat / write / mkdir / rm\n"), T1);
        t->out(QStringLiteral("  calc <expr>  /  echo <text>\n"), T1);
        t->out(QStringLiteral("  help / status / whoami / time\n"), T1);
        t->out(QStringLiteral("  clear / shutdown\n\n"), T1);
    }
    else if (c == QStringLiteral("clear")) {
        t->clear();
        t->prompt();
        return;
    }
    else if (c == QStringLiteral("ask")) {
        if (a.isEmpty()) {
            t->out(QStringLiteral("  Usage: ask <question>\n"), RED);
        } else {
            t->out(QStringLiteral("  \xE2\x8F\xB3 Thinking...\n"), T3); // ⏳
            QApplication::processEvents();
            QString response = m_llm.query(a);
            t->aiOut(response);
        }
    }
    else if (c == QStringLiteral("status")) {
        qint64 elapsed = QDateTime::currentSecsSinceEpoch() - m_sidebar->startTime();
        int hrs  = static_cast<int>(elapsed / 3600);
        int mins = static_cast<int>((elapsed % 3600) / 60);
        int secs = static_cast<int>(elapsed % 60);
        t->out(QStringLiteral("\n  User: %1 | Arch: x86 | LLM: Groq\n").arg(m_user), T1);
        t->out(QStringLiteral("  Uptime: %1h %2m %3s | ONLINE\n\n")
                   .arg(hrs).arg(mins).arg(secs), GREEN);
    }
    else if (c == QStringLiteral("whoami")) {
        t->out(QStringLiteral("  %1 @ SwanOS v2.0\n").arg(m_user), GREEN);
    }
    else if (c == QStringLiteral("echo")) {
        t->out(QStringLiteral("  %1\n").arg(a), T1);
    }
    else if (c == QStringLiteral("ls")) {
        t->out(QStringLiteral("  \xF0\x9F\x93\x81 documents/  "
                               "\xF0\x9F\x93\x81 programs/  "
                               "\xF0\x9F\x93\x84 readme.txt\n"), CYAN);
    }
    else if (c == QStringLiteral("cat")) {
        if (a == QStringLiteral("readme.txt")) {
            t->out(QStringLiteral("  Welcome to SwanOS! A bare-metal AI-powered OS.\n"), T1);
        } else {
            t->out(QStringLiteral("  Not found: %1\n").arg(a), RED);
        }
    }
    else if (c == QStringLiteral("calc")) {
        if (a.isEmpty()) {
            t->out(QStringLiteral("  Usage: calc <expression>\n"), RED);
        } else {
            // Simple eval: only digits and +-*/
            bool valid = true;
            for (const QChar &ch : a) {
                if (!ch.isDigit() && ch != '+' && ch != '-' && ch != '*'
                    && ch != '/' && ch != ' ' && ch != '(' && ch != ')')
                    valid = false;
            }
            if (valid) {
                // Basic left-to-right eval
                int result = 0, num = 0, has = 0;
                char op = '+';
                for (int i = 0; i <= a.length(); ++i) {
                    QChar ch = (i < a.length()) ? a[i] : QChar('\0');
                    if (ch.isDigit()) {
                        num = num * 10 + ch.digitValue();
                        has = 1;
                    } else {
                        if (has) {
                            switch (op) {
                            case '+': result += num; break;
                            case '-': result -= num; break;
                            case '*': result *= num; break;
                            case '/': if (num) result /= num; break;
                            }
                        }
                        if (ch == '+' || ch == '-' || ch == '*' || ch == '/')
                            op = ch.toLatin1();
                        num = 0; has = 0;
                    }
                }
                t->out(QStringLiteral("  = %1\n").arg(result), CYAN);
            } else {
                t->out(QStringLiteral("  Invalid expression\n"), RED);
            }
        }
    }
    else if (c == QStringLiteral("time")) {
        qint64 elapsed = QDateTime::currentSecsSinceEpoch() - m_sidebar->startTime();
        t->out(QStringLiteral("  %1h %2m %3s\n")
                   .arg(elapsed / 3600)
                   .arg((elapsed % 3600) / 60)
                   .arg(elapsed % 60), T1);
    }
    else if (c == QStringLiteral("shutdown")) {
        t->out(QStringLiteral("\n  Goodbye.\n"), ORANGE);
        QTimer::singleShot(1000, qApp, &QApplication::quit);
        return;
    }
    else {
        t->out(QStringLiteral("  Unknown: %1. Type 'help'\n").arg(c), RED);
    }

    t->out(QStringLiteral("\n"));
    t->prompt();
}
