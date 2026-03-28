#include <QApplication>
#include "TerminalWidget.h"
#include "TerminalModel.h"

int main(int argc, char *argv[]) {
    int width = 128;
    int height = 32;
    int char_width = 9;
    int char_height = 18;
    int buf_height = 8192;
    
    int window_width = width * char_width;
    int window_height = height * char_height;
    
    QApplication app(argc, argv);
    
    TerminalModel model(width, height, buf_height, char_width, char_height);

    TerminalWidget window(&model);
    window.setWindowTitle("Cosmic Terminal");
    window.resize(window_width, window_height);
    window.show();

    return app.exec();
}