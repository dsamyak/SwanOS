#include "sidebar.h"
#include "colors.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QDateTime>

Sidebar::Sidebar(const QString &user, QWidget *parent)
    : QWidget(parent), m_user(user)
{
    m_startTime = QDateTime::currentSecsSinceEpoch();
    setMinimumWidth(240);
    setMaximumWidth(260);
    buildUi();

    connect(&m_uptimeTimer, &QTimer::timeout, this, &Sidebar::updateUptime);
    m_uptimeTimer.start(1000);
}

void Sidebar::buildUi()
{
    using namespace Colors;

    setStyleSheet(
        QStringLiteral("QWidget{background:%1;border-right:1px solid %2;}")
            .arg(hex(BG1), hex(BRD)));

    auto *lo = new QVBoxLayout(this);
    lo->setContentsMargins(0, 0, 0, 0);
    lo->setSpacing(0);

    // Header
    auto *header = new QFrame();
    header->setFixedHeight(56);
    header->setStyleSheet(
        QStringLiteral("QFrame{background:%1;border-bottom:1px solid %2;border-right:none;}")
            .arg(hex(BG2), hex(BRD)));

    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 0, 16, 0);

    auto *icon = new QLabel(QStringLiteral("\xF0\x9F\xA6\xA2")); // 🦢
    icon->setStyleSheet(QStringLiteral("font-size:22px;background:transparent;border:none;"));
    hl->addWidget(icon);

    auto *brand = new QLabel(QStringLiteral("Swan OS"));
    brand->setStyleSheet(
        QStringLiteral("color:%1;font-size:16px;font-weight:300;"
                        "letter-spacing:2px;background:transparent;border:none;")
            .arg(hex(T1)));
    hl->addWidget(brand);
    hl->addStretch();

    auto *ver = new QLabel(QStringLiteral("v2.0"));
    ver->setStyleSheet(
        QStringLiteral("color:%1;font-size:10px;background:transparent;border:none;")
            .arg(hex(T3)));
    hl->addWidget(ver);
    lo->addWidget(header);

    // Content
    auto *content = new QWidget();
    content->setStyleSheet(QStringLiteral("border:none;background:transparent;"));
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 16, 16, 16);
    cl->setSpacing(16);

    // SYSTEM section
    cl->addWidget(makeSection(QStringLiteral("SYSTEM")));
    makeRow(QStringLiteral("\xF0\x9F\x91\xA4"), QStringLiteral("User"), m_user, cl); // 👤
    makeRow(QStringLiteral("\xF0\x9F\xA4\x96"), QStringLiteral("AI"), QStringLiteral("Groq LLM"), cl); // 🤖
    m_uptimeVal = makeRow(QStringLiteral("\xE2\x8F\xB1\xEF\xB8\x8F"), QStringLiteral("Uptime"), QStringLiteral("0h 0m 0s"), cl); // ⏱️

    // Status row
    auto *statusWidget = new QWidget();
    statusWidget->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    auto *sh = new QHBoxLayout(statusWidget);
    sh->setContentsMargins(0, 0, 0, 0);

    m_statusDot = new QLabel(QStringLiteral("\xE2\x97\x8F")); // ●
    m_statusDot->setStyleSheet(
        QStringLiteral("color:%1;font-size:10px;background:transparent;border:none;")
            .arg(hex(GREEN)));
    sh->addWidget(m_statusDot);

    m_statusText = new QLabel(QStringLiteral("Online"));
    m_statusText->setStyleSheet(
        QStringLiteral("color:%1;font-size:11px;background:transparent;border:none;")
            .arg(hex(GREEN)));
    sh->addWidget(m_statusText);
    sh->addStretch();
    cl->addWidget(statusWidget);

    // QUICK ACTIONS section
    cl->addWidget(makeSection(QStringLiteral("QUICK ACTIONS")));

    struct Action { const char *text; QColor color; };
    Action actions[] = {
        {"ask <question>", CYAN},
        {"help",           GREEN},
        {"ls",             ORANGE},
        {"status",         PURPLE},
    };
    for (const auto &a : actions) {
        auto *lbl = new QLabel(QStringLiteral("  \u276F  %1").arg(QString::fromUtf8(a.text)));
        lbl->setStyleSheet(
            QStringLiteral("color:%1;font-size:11px;font-family:%2;"
                            "background:transparent;border:none;padding:2px 0;")
                .arg(hex(a.color), FONT_MONO));
        cl->addWidget(lbl);
    }

    cl->addStretch();
    lo->addWidget(content);
}

QLabel *Sidebar::makeSection(const QString &title)
{
    auto *lbl = new QLabel(title);
    lbl->setStyleSheet(
        QStringLiteral("color:%1;font-size:10px;font-weight:700;letter-spacing:2px;"
                        "padding-bottom:6px;border-bottom:1px solid %2;"
                        "background:transparent;")
            .arg(Colors::hex(Colors::T3), Colors::hex(Colors::BRD)));
    return lbl;
}

QLabel *Sidebar::makeRow(const QString &icon, const QString &key,
                          const QString &val, QLayout *parent)
{
    auto *w = new QWidget();
    w->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);

    auto *ic = new QLabel(icon);
    ic->setStyleSheet(QStringLiteral("font-size:13px;background:transparent;border:none;"));
    h->addWidget(ic);

    auto *kl = new QLabel(key);
    kl->setStyleSheet(
        QStringLiteral("color:%1;font-size:11px;background:transparent;border:none;")
            .arg(Colors::hex(Colors::T3)));
    h->addWidget(kl);

    auto *vl = new QLabel(val);
    vl->setStyleSheet(
        QStringLiteral("color:%1;font-size:11px;background:transparent;border:none;")
            .arg(Colors::hex(Colors::T1)));
    h->addWidget(vl);
    h->addStretch();

    parent->addWidget(w);
    return vl;
}

void Sidebar::updateUptime()
{
    qint64 elapsed = QDateTime::currentSecsSinceEpoch() - m_startTime;
    int hrs  = static_cast<int>(elapsed / 3600);
    int mins = static_cast<int>((elapsed % 3600) / 60);
    int secs = static_cast<int>(elapsed % 60);
    m_uptimeVal->setText(QStringLiteral("%1h %2m %3s").arg(hrs).arg(mins).arg(secs));
}

void Sidebar::setConnection(bool online)
{
    using namespace Colors;
    if (online) {
        m_statusDot->setStyleSheet(
            QStringLiteral("color:%1;font-size:10px;background:transparent;border:none;")
                .arg(hex(GREEN)));
        m_statusText->setText(QStringLiteral("Connected"));
        m_statusText->setStyleSheet(
            QStringLiteral("color:%1;font-size:11px;background:transparent;border:none;")
                .arg(hex(GREEN)));
    } else {
        m_statusDot->setStyleSheet(
            QStringLiteral("color:%1;font-size:10px;background:transparent;border:none;")
                .arg(hex(ORANGE)));
        m_statusText->setText(QStringLiteral("Demo"));
        m_statusText->setStyleSheet(
            QStringLiteral("color:%1;font-size:11px;background:transparent;border:none;")
                .arg(hex(ORANGE)));
    }
}
