#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>

int main(int argc, char *argv[]) {
    essentia::init();
    QApplication app(argc, argv);

    // Apply dark studio theme
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(9, 9, 11));
    darkPalette.setColor(QPalette::WindowText, QColor(244, 244, 245));
    darkPalette.setColor(QPalette::Base, QColor(24, 24, 27));
    darkPalette.setColor(QPalette::AlternateBase, QColor(39, 39, 42));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, QColor(244, 244, 245));
    darkPalette.setColor(QPalette::Button, QColor(24, 24, 27));
    darkPalette.setColor(QPalette::ButtonText, QColor(244, 244, 245));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(217, 119, 6));
    darkPalette.setColor(QPalette::Highlight, QColor(217, 119, 6));
    darkPalette.setColor(QPalette::HighlightedText, QColor(9, 9, 11));
    app.setPalette(darkPalette);

    MainWindow w;
    w.resize(1280, 800);
    w.show();

    int ir = app.exec();

    essentia::shutdown();
    return ir;
}