#include <QApplication>
#include "TerminalWindow.h"

int main(int argc, char *argv[]) {
    int width = 128;
    int height = 32;
    int char_width = 9;
    int char_height = 18;
    int buf_height = 8192;
    int tab_bar_height = 32;
    
    int window_width = width * char_width;
    int window_height = height * char_height;
    
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("cosmic");
    QCoreApplication::setApplicationName("terminal");

    TerminalWindow window;
    window.setWindowTitle("Cosmic Terminal");
    window.resize(window.sizeHint());
    window.show();

    return app.exec();
}