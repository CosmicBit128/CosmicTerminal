#include <QApplication>
#include "TerminalWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("cosmic");
    QCoreApplication::setApplicationName("terminal");
    
    TerminalWindow window;
    window.setWindowTitle("Cosmic Terminal");
    window.show();

    return app.exec();
}