/*
 * SwanOS — C++ Qt6 GUI Frontend
 *
 * Usage:
 *   swanos_gui --demo                     Standalone demo (no VM)
 *   swanos_gui --pipe \\.\pipe\swanos     VirtualBox pipe mode
 */

#include "swanos_window.h"
#include "colors.h"

#include <QApplication>
#include <QPalette>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SwanOS"));

    // Parse arguments
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SwanOS GUI Frontend"));
    parser.addHelpOption();

    QCommandLineOption demoOpt(QStringLiteral("demo"),
                                QStringLiteral("Run in standalone demo mode"));
    parser.addOption(demoOpt);

    QCommandLineOption pipeOpt(QStringLiteral("pipe"),
                                QStringLiteral("Named pipe path (e.g. \\\\.\\pipe\\swanos)"),
                                QStringLiteral("path"));
    parser.addOption(pipeOpt);

    parser.process(app);

    bool demo = parser.isSet(demoOpt) || !parser.isSet(pipeOpt);
    QString pipePath = parser.value(pipeOpt);

    // Set dark palette
    QPalette pal;
    pal.setColor(QPalette::Window,          Colors::BG0);
    pal.setColor(QPalette::WindowText,      Colors::T1);
    pal.setColor(QPalette::Base,            Colors::BG1);
    pal.setColor(QPalette::Text,            Colors::T1);
    pal.setColor(QPalette::Highlight,       Colors::CYAN);
    pal.setColor(QPalette::HighlightedText, Colors::BG0);
    pal.setColor(QPalette::Button,          Colors::BG2);
    pal.setColor(QPalette::ButtonText,      Colors::T1);
    app.setPalette(pal);

    // Create and show main window
    SwanOSWindow window(demo, pipePath);
    window.show();

    return app.exec();
}
