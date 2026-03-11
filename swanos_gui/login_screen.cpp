#include "login_screen.h"
#include "colors.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QLinearGradient>

LoginScreen::LoginScreen(QWidget *parent)
    : QWidget(parent)
{
    m_particles.reserve(40);
    for (int i = 0; i < 40; ++i)
        m_particles.emplace_back(900, 600);

    buildUi();

    connect(&m_particleTimer, &QTimer::timeout, this, [this]() {
        for (auto &p : m_particles) p.update();
        update();
    });
    m_particleTimer.start(30);
}

void LoginScreen::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int w = width(), h = height();

    QLinearGradient bg(0, 0, 0, h);
    bg.setColorAt(0, QColor(8, 12, 22));
    bg.setColorAt(1, QColor(12, 18, 35));
    p.fillRect(rect(), bg);

    for (const auto &pt : m_particles) {
        int a = static_cast<int>(qMax(0.0, pt.life) * 200);
        p.setBrush(QColor(0, 200, 255, a));
        p.setPen(Qt::NoPen);
        p.drawEllipse(static_cast<int>(pt.x), static_cast<int>(pt.y),
                       static_cast<int>(pt.size), static_cast<int>(pt.size));
    }
}

void LoginScreen::buildUi()
{
    using namespace Colors;

    auto *lo = new QVBoxLayout(this);
    lo->setAlignment(Qt::AlignCenter);

    auto *card = new QFrame();
    card->setFixedSize(380, 400);
    card->setStyleSheet(
        QStringLiteral("QFrame{background:rgba(15,20,35,0.88);"
                        "border:1px solid rgba(42,53,85,0.6);"
                        "border-radius:20px;}"));

    auto *cl = new QVBoxLayout(card);
    cl->setContentsMargins(40, 30, 40, 30);
    cl->setSpacing(14);

    // Swan icon
    auto *icon = new QLabel(QStringLiteral("\xF0\x9F\xA6\xA2")); // 🦢
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QStringLiteral("font-size:42px;background:transparent;border:none;"));
    cl->addWidget(icon);

    // Title
    auto *title = new QLabel(QStringLiteral("Swan OS"));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QStringLiteral("color:%1;font-size:26px;font-weight:300;"
                        "letter-spacing:3px;background:transparent;border:none;")
            .arg(hex(T1)));
    cl->addWidget(title);

    // Subtitle
    auto *sub = new QLabel(QStringLiteral("LLM-Powered Operating System"));
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet(
        QStringLiteral("color:%1;font-size:10px;letter-spacing:2px;"
                        "margin-bottom:12px;background:transparent;border:none;")
            .arg(hex(CYAN)));
    cl->addWidget(sub);

    // Input style
    QString inputSS = QStringLiteral(
        "QLineEdit{background:rgba(21,27,46,0.9);"
        "border:1px solid %1;border-radius:10px;padding:10px 16px;"
        "color:%2;font-size:13px;}"
        "QLineEdit:focus{border:1px solid %3;}"
        "QLineEdit::placeholder{color:%4;}")
        .arg(hex(BRD), hex(T1), hex(CYAN), hex(T3));

    // Username
    m_userInput = new QLineEdit();
    m_userInput->setPlaceholderText(QStringLiteral("Username"));
    m_userInput->setStyleSheet(inputSS);
    m_userInput->setMinimumHeight(44);
    cl->addWidget(m_userInput);

    // Password
    m_passInput = new QLineEdit();
    m_passInput->setPlaceholderText(QStringLiteral("Password"));
    m_passInput->setEchoMode(QLineEdit::Password);
    m_passInput->setStyleSheet(inputSS);
    m_passInput->setMinimumHeight(44);
    connect(m_passInput, &QLineEdit::returnPressed, this, &LoginScreen::onLogin);
    cl->addWidget(m_passInput);

    // Error label
    m_errorLabel = new QLabel();
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setStyleSheet(
        QStringLiteral("color:%1;font-size:11px;background:transparent;border:none;")
            .arg(hex(RED)));
    cl->addWidget(m_errorLabel);

    // Sign in button
    auto *btn = new QPushButton(QStringLiteral("Sign In"));
    btn->setMinimumHeight(44);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 %1,stop:1 %2);"
            "color:#0a0e17;border:none;border-radius:10px;"
            "font-size:14px;font-weight:600;letter-spacing:1px;}"
            "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 #33ddff,stop:1 #6690ff);}")
            .arg(hex(CYAN), hex(BLUE)));
    connect(btn, &QPushButton::clicked, this, &LoginScreen::onLogin);
    cl->addWidget(btn);

    // Hint
    auto *hint = new QLabel(QStringLiteral("Default: admin / admin"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet(
        QStringLiteral("color:%1;font-size:10px;background:transparent;border:none;")
            .arg(hex(T3)));
    cl->addWidget(hint);

    // Drop shadow on card
    auto *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(60);
    shadow->setColor(QColor(0, 150, 220, 40));
    shadow->setOffset(0, 10);
    card->setGraphicsEffect(shadow);

    lo->addWidget(card);
}

void LoginScreen::onLogin()
{
    QString user = m_userInput->text().trimmed();
    if (user.isEmpty()) {
        m_errorLabel->setText(QStringLiteral("Enter a username"));
        return;
    }
    emit loginSuccess(user);
}
