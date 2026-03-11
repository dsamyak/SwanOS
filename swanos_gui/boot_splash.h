#ifndef BOOT_SPLASH_H
#define BOOT_SPLASH_H

#include <QWidget>
#include <QTimer>
#include <vector>
#include "particle.h"

/*
 * SwanOS — Animated Boot Splash
 * 4-phase: fade in → hold → loading bar → fade out
 */

class BootSplash : public QWidget {
    Q_OBJECT

public:
    explicit BootSplash(QWidget *parent = nullptr);

signals:
    void bootFinished();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void tick();

private:
    void drawSwan(QPainter &p, int cx, int cy);

    QTimer m_timer;
    std::vector<Particle> m_particles;
    int   m_phase   = 0;
    double m_alpha  = 0.0;
    double m_progress = 0.0;
    int   m_msgIdx  = 0;
    qint64 m_phaseStart;

    static constexpr const char *MSGS[] = {
        "Initializing kernel...",
        "Loading GDT...",
        "Remapping PIC...",
        "Initializing IDT...",
        "Starting PIT timer...",
        "Loading keyboard driver...",
        "Initializing COM1 serial...",
        "Setting up memory (4 MB)...",
        "Mounting filesystem...",
        "Starting user manager...",
        "Connecting to AI bridge...",
        "All systems online."
    };
    static constexpr int MSG_COUNT = 12;
};

#endif // BOOT_SPLASH_H
