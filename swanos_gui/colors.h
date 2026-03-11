#ifndef COLORS_H
#define COLORS_H

#include <QColor>
#include <QString>

/*
 * SwanOS — Centralized Color Palette
 * Matches the Python GUI colour scheme for visual consistency.
 */

namespace Colors {

// ── Backgrounds ─────────────────────────────
inline const QColor BG0   {10,  14,  23};   // #0a0e17
inline const QColor BG1   {15,  20,  35};   // #0f1423
inline const QColor BG2   {21,  27,  46};   // #151b2e
inline const QColor BG3   {26,  33,  57};   // #1a2139
inline const QColor BG4   {33,  42,  69};   // #212a45

// ── Borders ─────────────────────────────────
inline const QColor BRD   {42,  53,  85};   // #2a3555
inline const QColor BRD2  {58,  74, 112};   // #3a4a70

// ── Text ────────────────────────────────────
inline const QColor T1    {232, 236, 244};  // #e8ecf4
inline const QColor T2    {136, 146, 168};  // #8892a8
inline const QColor T3    {85,  96,  128};  // #556080

// ── Accents ─────────────────────────────────
inline const QColor CYAN  {0,   212, 255};  // #00d4ff
inline const QColor BLUE  {77,  124, 255};  // #4d7cff
inline const QColor PURPLE{168, 85,  247};  // #a855f7
inline const QColor GREEN {34,  197, 94};   // #22c55e
inline const QColor ORANGE{245, 158, 11};   // #f59e0b
inline const QColor RED   {239, 68,  68};   // #ef4444

// ── Stylesheet helpers ──────────────────────
inline QString hex(const QColor &c) {
    return c.name(QColor::HexRgb);
}

inline const QString FONT_MONO =
    QStringLiteral("Cascadia Code, JetBrains Mono, Consolas, monospace");

} // namespace Colors

#endif // COLORS_H
