#include "boot_splash.h"
#include "colors.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QFont>
#include <QElapsedTimer>
#include <cmath>

static QElapsedTimer s_elapsed;

BootSplash::BootSplash(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(900, 600);

    m_particles.reserve(60);
    for (int i = 0; i < 60; ++i)
        m_particles.emplace_back(900, 600);

    connect(&m_timer, &QTimer::timeout, this, &BootSplash::tick);
    m_timer.start(16); // ~60fps

    s_elapsed.start();
    m_phaseStart = s_elapsed.elapsed();
}

void BootSplash::tick()
{
    qint64 now = s_elapsed.elapsed();
    double dt = (now - m_phaseStart) / 1000.0;

    switch (m_phase) {
    case 0: // Fade in
        m_alpha = qMin(1.0, dt);
        if (m_alpha >= 1.0) {
            m_phase = 1;
            m_phaseStart = now;
        }
        break;
    case 1: // Hold
        if (dt > 1.0) {
            m_phase = 2;
            m_phaseStart = now;
        }
        break;
    case 2: // Loading bar
        m_progress = qMin(1.0, dt / 4.0);
        m_msgIdx = qMin(MSG_COUNT - 1, static_cast<int>(m_progress * MSG_COUNT));
        if (m_progress >= 1.0) {
            m_phase = 3;
            m_phaseStart = now;
        }
        break;
    case 3: // Fade out
        m_alpha = qMax(0.0, 1.0 - dt / 0.8);
        if (m_alpha <= 0.0) {
            m_timer.stop();
            emit bootFinished();
            return;
        }
        break;
    }

    for (auto &p : m_particles)
        p.update();

    update();
}

void BootSplash::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int w = width(), h = height();

    // Background gradient
    QLinearGradient bg(0, 0, 0, h);
    bg.setColorAt(0, QColor(8, 12, 22));
    bg.setColorAt(1, QColor(6, 10, 18));
    p.fillRect(rect(), bg);

    // Radial glow
    QRadialGradient gl(w / 2, h * 0.35, 300);
    gl.setColorAt(0, QColor(0, 120, 180, 25));
    gl.setColorAt(1, QColor(0, 0, 0, 0));
    p.setBrush(QBrush(gl));
    p.setPen(Qt::NoPen);
    p.drawEllipse(w / 2 - 300, static_cast<int>(h * 0.35) - 300, 600, 600);

    // Particles
    for (const auto &pt : m_particles) {
        int a = static_cast<int>(qMax(0.0, pt.life) * 200 * m_alpha);
        p.setBrush(QColor(0, 200, 255, a));
        p.setPen(Qt::NoPen);
        p.drawEllipse(static_cast<int>(pt.x), static_cast<int>(pt.y),
                       static_cast<int>(pt.size), static_cast<int>(pt.size));
    }

    // Swan icon
    int cx = w / 2, cy = static_cast<int>(h * 0.32);
    drawSwan(p, cx, cy);

    // Title: "SWAN OS"
    p.setOpacity(m_alpha);
    QFont titleFont(QStringLiteral("Segoe UI"), 36, QFont::Light);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 8);
    p.setFont(titleFont);
    p.setPen(Colors::T1);
    p.drawText(0, cy + 75, w, 50, Qt::AlignCenter, QStringLiteral("SWAN OS"));

    // Subtitle
    QFont subFont(QStringLiteral("Segoe UI"), 12);
    subFont.setLetterSpacing(QFont::AbsoluteSpacing, 4);
    p.setFont(subFont);
    p.setPen(QColor(0, 212, 255, static_cast<int>(180 * m_alpha)));
    p.drawText(0, cy + 115, w, 30, Qt::AlignCenter,
               QStringLiteral("LLM-POWERED OPERATING SYSTEM"));

    // Loading bar (phase 2+)
    if (m_phase >= 2) {
        int bw = 320, bx = (w - bw) / 2, by = cy + 170;

        // Track background
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(30, 40, 60, static_cast<int>(200 * m_alpha)));
        p.drawRoundedRect(bx, by, bw, 4, 2, 2);

        // Filled portion
        int fw = static_cast<int>(bw * m_progress);
        if (fw > 0) {
            QLinearGradient g2(bx, 0, bx + bw, 0);
            g2.setColorAt(0, QColor(0, 180, 255, static_cast<int>(255 * m_alpha)));
            g2.setColorAt(1, QColor(34, 197, 94, static_cast<int>(255 * m_alpha)));
            p.setBrush(QBrush(g2));
            p.drawRoundedRect(bx, by, fw, 4, 2, 2);
        }

        // Status message
        QFont msgFont(Colors::FONT_MONO, 9);
        p.setFont(msgFont);
        p.setPen(QColor(136, 146, 168, static_cast<int>(200 * m_alpha)));
        if (m_msgIdx < MSG_COUNT)
            p.drawText(0, by + 20, w, 25, Qt::AlignCenter,
                       QString::fromUtf8(MSGS[m_msgIdx]));
    }

    p.setOpacity(1.0);
}

void BootSplash::drawSwan(QPainter &p, int cx, int cy)
{
    p.setOpacity(m_alpha);

    // Ring
    p.setPen(QPen(QColor(0, 212, 255, static_cast<int>(180 * m_alpha)), 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(cx - 45, cy - 45, 90, 90);

    // Body
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(232, 236, 244, static_cast<int>(230 * m_alpha)));
    QPainterPath body;
    body.addEllipse(cx - 18, cy - 5, 36, 22);
    p.drawPath(body);

    // Neck segments
    int neckPts[][2] = {{-10, 0}, {-16, -10}, {-18, -20}, {-14, -28}, {-8, -32}};
    for (int i = 0; i < 5; ++i) {
        double r = 5.0 - i * 0.5;
        p.drawEllipse(static_cast<int>(cx + neckPts[i][0] - r),
                       static_cast<int>(cy + neckPts[i][1] - r),
                       static_cast<int>(r * 2), static_cast<int>(r * 2));
    }

    // Head
    p.drawEllipse(cx - 11, cy - 37, 14, 12);

    // Eye
    p.setBrush(QColor(10, 14, 25, static_cast<int>(255 * m_alpha)));
    p.drawEllipse(cx - 5, cy - 33, 3, 3);

    // Beak
    p.setBrush(QColor(245, 158, 11, static_cast<int>(230 * m_alpha)));
    QPainterPath beak;
    beak.moveTo(cx + 2, cy - 33);
    beak.lineTo(cx + 12, cy - 30);
    beak.lineTo(cx + 2, cy - 28);
    beak.closeSubpath();
    p.drawPath(beak);

    p.setOpacity(1.0);
}
