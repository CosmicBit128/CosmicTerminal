#pragma once

#include "TerminalParser.h"
#include "TerminalTab.h"
#include "GlyphCache.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QKeyEvent>
#include <QTimer>
#include <QSocketNotifier>
#include <QStringDecoder>

#include <fontconfig/fontconfig.h>
#include <pty.h>



static std::string find_font(const std::vector<std::string> fonts, int weight) {
    FcInit();

    for (const auto& name : fonts) {
        FcPattern* pattern = FcNameParse((FcChar8*)name.c_str());
        FcPatternAddInteger(pattern, FC_WEIGHT, weight);
        FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
        FcDefaultSubstitute(pattern);

        FcResult result;
        FcPattern* match = FcFontMatch(nullptr, pattern, &result);

        if (match) {
            FcChar8* file = nullptr;
            if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch) {
                std::string path = (char*)file;
                FcPatternDestroy(pattern);
                FcPatternDestroy(match);
                return path;
            }
            FcPatternDestroy(match);
        }

        FcPatternDestroy(pattern);
    }

    return "";
}

class TerminalModel;

class TerminalWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget* parent = nullptr);
    ~TerminalWidget() override;

    void setTab(TerminalTab* tab);

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    int charWidth() const { return m_charWidth; }
    int charHeight() const { return m_charHeight; }

signals:
    void requestNewTab();
    void requestNewWindow();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    QPoint getCellPos(QPointF position);

private slots:
    void onPtyReadable();

private:
    void copySelection();
    void paste();

    TerminalTab* m_tab = nullptr;

    GlyphCache m_glyphCache;
    QOpenGLShaderProgram program;
    GLuint vao = 0, vbo = 0;
    GLuint textTexture = 0;
    GLuint attrFgTexture = 0;
    GLuint attrBgTexture = 0;
    GLuint m_glyphUVTexture = 0;
    
    FT_Library ft;
    FT_Face face;
    FT_Face bface;
    
    std::vector<uint32_t> screenCodepoints;
    std::vector<GLubyte> screenFg;
    std::vector<GLubyte> screenBg;
    std::vector<GLfloat> m_glyphUVs;
    
    QTimer* cursorTimer;
    bool cursorVisible = true;

    int m_charWidth  = 9;
    int m_charHeight = 18;
    int m_cols       = 128;
    int m_rows       = 32;
    
    int glyphW = 7;
    int glyphH = 14;
    int glyphCols = 16;

    QPoint m_selStart;
    QPoint m_selEnd;
    bool m_hasSelection = false;
};