#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QKeyEvent>
#include <QTimer>

class TerminalModel;

class TerminalWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
public:
    explicit TerminalWidget(TerminalModel* model, QWidget* parent = nullptr);
    ~TerminalWidget() override;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    TerminalModel* m_model = nullptr;
    QOpenGLShaderProgram program;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint textTexture = 0;
    GLuint glyphTexture = 0;
    
    QTimer* cursorTimer;
    bool cursorVisible = true;

    int glyphW = 7;
    int glyphH = 14;
    int glyphCols = 16;
    int glyphRows = 6;
};