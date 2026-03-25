#pragma once

#include "TerminalParser.h"
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
    TerminalParser parser;
    TerminalModel* m_model = nullptr;
    GlyphCache m_glyphCache;
    QOpenGLShaderProgram program;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint textTexture = 0;
    GLuint attrFgTexture = 0;
    GLuint attrBgTexture = 0;

    GLuint m_glyphUVTexture = 0;
    std::vector<GLfloat> m_glyphUVs;

    FT_Library ft;
    FT_Face face;
    FT_Face bface;

    std::vector<uint32_t> screenCodepoints;
    std::vector<GLubyte> screenFg;
    std::vector<GLubyte> screenBg;
   
    QTimer* cursorTimer;
    bool cursorVisible = true;

    int glyphW = 7;
    int glyphH = 14;
    int glyphCols = 16;
    int glyphRows = 6;

    int m_masterFd = -1;
    QSocketNotifier* m_ptyNotifier = nullptr;
    QStringDecoder m_decoder{QStringDecoder::Utf8};
    pid_t m_shellPid = -1;
};