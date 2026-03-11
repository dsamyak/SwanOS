#ifndef SWANOS_WINDOW_H
#define SWANOS_WINDOW_H

#include <QMainWindow>
#include <QStackedWidget>

class BootSplash;
class LoginScreen;
class Desktop;
class SerialWorker;
class QThread;

/*
 * SwanOS — Top-level Window
 * Manages splash → login → desktop transitions.
 */

class SwanOSWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit SwanOSWindow(bool demo, const QString &pipePath = QString(),
                           QWidget *parent = nullptr);
    ~SwanOSWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onBootFinished();
    void onLoginSuccess(const QString &user);

private:
    void startSerial(const QString &pipePath);

    bool            m_demo;
    QString         m_pipePath;
    QStackedWidget *m_stack = nullptr;
    BootSplash     *m_splash = nullptr;
    LoginScreen    *m_login  = nullptr;
    Desktop        *m_desktop = nullptr;
    SerialWorker   *m_worker  = nullptr;
    QThread        *m_workerThread = nullptr;
};

#endif // SWANOS_WINDOW_H
