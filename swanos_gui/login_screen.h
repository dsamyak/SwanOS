#ifndef LOGIN_SCREEN_H
#define LOGIN_SCREEN_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <vector>
#include "particle.h"

/*
 * SwanOS — Glassmorphic Login Screen
 */

class LoginScreen : public QWidget {
    Q_OBJECT

public:
    explicit LoginScreen(QWidget *parent = nullptr);

signals:
    void loginSuccess(const QString &username);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onLogin();

private:
    void buildUi();

    QLineEdit *m_userInput  = nullptr;
    QLineEdit *m_passInput  = nullptr;
    QLabel    *m_errorLabel = nullptr;
    QTimer     m_particleTimer;
    std::vector<Particle> m_particles;
};

#endif // LOGIN_SCREEN_H
