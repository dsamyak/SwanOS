#include "swanos_window.h"
#include "boot_splash.h"
#include "login_screen.h"
#include "desktop.h"
#include "serial_worker.h"
#include "colors.h"

#include <QThread>
#include <QCloseEvent>
#include <QTimer>

SwanOSWindow::SwanOSWindow(bool demo, const QString &pipePath, QWidget *parent)
    : QMainWindow(parent), m_demo(demo), m_pipePath(pipePath)
{
    setWindowTitle(QStringLiteral("Swan OS"));
    setMinimumSize(1100, 700);
    resize(1280, 780);
    setStyleSheet(QStringLiteral("QMainWindow{background:%1;}")
                      .arg(Colors::hex(Colors::BG0)));

    m_stack = new QStackedWidget();
    setCentralWidget(m_stack);

    // Boot splash
    m_splash = new BootSplash();
    connect(m_splash, &BootSplash::bootFinished, this, &SwanOSWindow::onBootFinished);
    m_stack->addWidget(m_splash);

    // Login screen
    m_login = new LoginScreen();
    connect(m_login, &LoginScreen::loginSuccess, this, &SwanOSWindow::onLoginSuccess);
    m_stack->addWidget(m_login);

    // Start on splash
    m_stack->setCurrentWidget(m_splash);

    // Start serial if not demo
    if (!pipePath.isEmpty() && !demo) {
        startSerial(pipePath);
    }
}

SwanOSWindow::~SwanOSWindow()
{
    if (m_worker) m_worker->stop();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(2000);
    }
}

void SwanOSWindow::startSerial(const QString &pipePath)
{
    m_worker = new SerialWorker(pipePath);
    m_workerThread = new QThread(this);
    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::started, m_worker, &SerialWorker::startReading);
    m_workerThread->start();
}

void SwanOSWindow::onBootFinished()
{
    m_stack->setCurrentWidget(m_login);
    m_login->findChild<QLineEdit *>()->setFocus();
}

void SwanOSWindow::onLoginSuccess(const QString &user)
{
    m_desktop = new Desktop(user, m_demo, m_worker);
    m_stack->addWidget(m_desktop);
    m_stack->setCurrentWidget(m_desktop);
    m_desktop->terminal()->setFocus();

    // For serial mode, send login credentials after a short delay
    if (m_worker && !m_demo) {
        QTimer::singleShot(500, this, [this, user]() {
            for (const QChar &ch : user)
                m_worker->sendChar(ch.toLatin1());
            m_worker->sendChar('\n');

            QTimer::singleShot(300, this, [this]() {
                // Send default password
                const char *pass = "admin";
                for (int i = 0; pass[i]; ++i)
                    m_worker->sendChar(pass[i]);
                m_worker->sendChar('\n');

                QTimer::singleShot(300, this, [this]() {
                    // Select CLI mode (option 2)
                    m_worker->sendChar('2');
                });
            });
        });
    }
}

void SwanOSWindow::closeEvent(QCloseEvent *event)
{
    if (m_worker) m_worker->stop();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(2000);
    }
    event->accept();
}
