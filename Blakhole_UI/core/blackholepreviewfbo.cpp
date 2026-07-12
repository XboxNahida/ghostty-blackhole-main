// blackholepreviewfbo.cpp ── QQuickFramebufferObject 黑洞实时预览 实现
#include "blackholepreviewfbo.h"
#include "blackholecore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QQuickWindow>
#include <QImage>
#include <QTextStream>
#include <QtMath>
#include <QDebug>

// ========== BlackholePreviewFBO (QML 可见层) ==========

BlackholePreviewFBO::BlackholePreviewFBO(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    setFlag(ItemHasContents, true);
    m_running = true;
}

// 属性 getters
float BlackholePreviewFBO::diskTemp()  const { return m_diskTemp; }
float BlackholePreviewFBO::diskIncl()  const { return m_diskIncl; }
float BlackholePreviewFBO::diskRoll()  const { return m_diskRoll; }
float BlackholePreviewFBO::diskInner() const { return m_diskInner; }
float BlackholePreviewFBO::diskOuter() const { return m_diskOuter; }
float BlackholePreviewFBO::diskOpac()  const { return m_diskOpac; }
float BlackholePreviewFBO::diskDopp()  const { return m_diskDopp; }
float BlackholePreviewFBO::diskBeam()  const { return m_diskBeam; }
float BlackholePreviewFBO::diskGain()  const { return m_diskGain; }
float BlackholePreviewFBO::diskContr() const { return m_diskContr; }
float BlackholePreviewFBO::diskWind()  const { return m_diskWind; }
float BlackholePreviewFBO::diskSpeed() const { return m_diskSpeed; }
float BlackholePreviewFBO::diskExpo()  const { return m_diskExpo; }
float BlackholePreviewFBO::diskStar()  const { return m_diskStar; }
bool  BlackholePreviewFBO::running()   const { return m_running; }

void BlackholePreviewFBO::setDiskTemp(float v)  { if (m_diskTemp  != v) { m_diskTemp  = v; emit diskTempChanged();  update(); } }
void BlackholePreviewFBO::setDiskIncl(float v)  { if (m_diskIncl  != v) { m_diskIncl  = v; emit diskInclChanged();  update(); } }
void BlackholePreviewFBO::setDiskRoll(float v)  { if (m_diskRoll  != v) { m_diskRoll  = v; emit diskRollChanged();  update(); } }
void BlackholePreviewFBO::setDiskInner(float v) { if (m_diskInner != v) { m_diskInner = v; emit diskInnerChanged(); update(); } }
void BlackholePreviewFBO::setDiskOuter(float v) { if (m_diskOuter != v) { m_diskOuter = v; emit diskOuterChanged(); update(); } }
void BlackholePreviewFBO::setDiskOpac(float v)  { if (m_diskOpac  != v) { m_diskOpac  = v; emit diskOpacChanged();  update(); } }
void BlackholePreviewFBO::setDiskDopp(float v)  { if (m_diskDopp  != v) { m_diskDopp  = v; emit diskDoppChanged();  update(); } }
void BlackholePreviewFBO::setDiskBeam(float v)  { if (m_diskBeam  != v) { m_diskBeam  = v; emit diskBeamChanged();  update(); } }
void BlackholePreviewFBO::setDiskGain(float v)  { if (m_diskGain  != v) { m_diskGain  = v; emit diskGainChanged();  update(); } }
void BlackholePreviewFBO::setDiskContr(float v) { if (m_diskContr != v) { m_diskContr = v; emit diskContrChanged(); update(); } }
void BlackholePreviewFBO::setDiskWind(float v)  { if (m_diskWind  != v) { m_diskWind  = v; emit diskWindChanged();  update(); } }
void BlackholePreviewFBO::setDiskSpeed(float v) { if (m_diskSpeed != v) { m_diskSpeed = v; emit diskSpeedChanged(); update(); } }
void BlackholePreviewFBO::setDiskExpo(float v)  { if (m_diskExpo  != v) { m_diskExpo  = v; emit diskExpoChanged();  update(); } }
void BlackholePreviewFBO::setDiskStar(float v)  { if (m_diskStar  != v) { m_diskStar  = v; emit diskStarChanged();  update(); } }

void BlackholePreviewFBO::setRunning(bool v)
{
    if (m_running != v) {
        m_running = v;
        emit runningChanged();
        update();
    }
}

void BlackholePreviewFBO::syncFromCore(QObject *core)
{
    BlackHoleCore *bh = qobject_cast<BlackHoleCore*>(core);
    if (!bh) return;
    setDiskTemp(bh->diskTemp());
    setDiskIncl(bh->diskIncl());
    setDiskRoll(bh->diskRoll());
    setDiskInner(bh->diskInner());
    setDiskOuter(bh->diskOuter());
    setDiskOpac(bh->diskOpac());
    setDiskDopp(bh->diskDopp());
    setDiskBeam(bh->diskBeam());
    setDiskGain(bh->diskGain());
    setDiskContr(bh->diskContr());
    setDiskWind(bh->diskWind());
    setDiskSpeed(bh->diskSpeed());
    setDiskExpo(bh->diskExpo());
    setDiskStar(bh->diskStar());
}

QQuickFramebufferObject::Renderer *BlackholePreviewFBO::createRenderer() const
{
    return new BlackholePreviewRenderer();
}

// ========== BlackholePreviewRenderer (渲染线程) ==========

BlackholePreviewRenderer::BlackholePreviewRenderer()
{
    m_timer.start();
}

BlackholePreviewRenderer::~BlackholePreviewRenderer()
{
    if (m_program) { delete m_program; m_program = nullptr; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_blackTex) { glDeleteTextures(1, &m_blackTex); m_blackTex = 0; }
}

QOpenGLFramebufferObject *BlackholePreviewRenderer::createFramebufferObject(const QSize &size)
{
    if (size.isEmpty()) return nullptr;
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    fmt.setSamples(0);
    return new QOpenGLFramebufferObject(size, fmt);
}

void BlackholePreviewRenderer::synchronize(QQuickFramebufferObject *item)
{
    BlackholePreviewFBO *fboItem = static_cast<BlackholePreviewFBO*>(item);
    m_diskTemp  = fboItem->m_diskTemp;
    m_diskIncl  = fboItem->m_diskIncl;
    m_diskRoll  = fboItem->m_diskRoll;
    m_diskInner = fboItem->m_diskInner;
    m_diskOuter = fboItem->m_diskOuter;
    m_diskOpac  = fboItem->m_diskOpac;
    m_diskDopp  = fboItem->m_diskDopp;
    m_diskBeam  = fboItem->m_diskBeam;
    m_diskGain  = fboItem->m_diskGain;
    m_diskContr = fboItem->m_diskContr;
    m_diskWind  = fboItem->m_diskWind;
    m_diskSpeed = fboItem->m_diskSpeed;
    m_diskExpo  = fboItem->m_diskExpo;
    m_diskStar  = fboItem->m_diskStar;
    m_running   = fboItem->m_running;
    m_viewSize  = QSize(fboItem->width(), fboItem->height());
}

void BlackholePreviewRenderer::resolveShaderPath(QString &vertPath, QString &fragHeaderPath, QString &fragBodyPath)
{
    QStringList searchDirs;
    QString appDir = QCoreApplication::applicationDirPath();
    searchDirs << appDir;
    searchDirs << QDir(appDir).filePath("..");
    searchDirs << QDir::currentPath();

    for (const QString &dir : searchDirs) {
        QString vp  = dir + "/shaders/vert.glsl";
        QString fhp = dir + "/shaders/frag_preview_header.glsl";
        QString fbp = dir + "/blackhole_preview.glsl";
        if (QFileInfo::exists(vp) && QFileInfo::exists(fhp) && QFileInfo::exists(fbp)) {
            vertPath = vp; fragHeaderPath = fhp; fragBodyPath = fbp;
            qDebug() << "BlackholePreviewFBO: shaders found in" << dir;
            return;
        }
        vp  = dir + "/release/shaders/vert.glsl";
        fhp = dir + "/release/shaders/frag_preview_header.glsl";
        if (QFileInfo::exists(vp) && QFileInfo::exists(fhp) && QFileInfo::exists(fbp)) {
            vertPath = vp; fragHeaderPath = fhp; fragBodyPath = fbp;
            qDebug() << "BlackholePreviewFBO: shaders found in" << dir << "/release";
            return;
        }
    }
    vertPath = "shaders/vert.glsl";
    fragHeaderPath = "shaders/frag_preview_header.glsl";
    fragBodyPath = "blackhole.glsl";
}

QString BlackholePreviewRenderer::readShaderFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "BlackholePreviewFBO: cannot open shader:" << path;
        return QString();
    }
    QTextStream in(&file);
    QString content = in.readAll();
    if (content.size() >= 3) {
        unsigned char c0 = static_cast<unsigned char>(content[0].toLatin1());
        unsigned char c1 = static_cast<unsigned char>(content[1].toLatin1());
        unsigned char c2 = static_cast<unsigned char>(content[2].toLatin1());
        if (c0 == 0xEF && c1 == 0xBB && c2 == 0xBF)
            content = content.mid(3);
    }
    return content;
}

bool BlackholePreviewRenderer::initShaders()
{
    QString vertPath, fragHeaderPath, fragBodyPath;
    resolveShaderPath(vertPath, fragHeaderPath, fragBodyPath);

    QString vertSrc = readShaderFile(vertPath);
    QString fragHeader = readShaderFile(fragHeaderPath);
    QString fragBody = readShaderFile(fragBodyPath);

    if (vertSrc.isEmpty() || fragHeader.isEmpty() || fragBody.isEmpty()) {
        qWarning() << "BlackholePreviewFBO: failed to read shader files";
        return false;
    }

    // === Shader 预处理: 固定预览尺寸 (不受 Pomodoro 周期影响) ===
    // 强制 I=1.0, sz=1.0, center=vec2(0.5, 0.5), dil=1.0
    {
        // 在 Pomodoro 块末尾注入固定覆盖
        fragBody.replace(
            QStringLiteral("center += I * vec2(0.040 * sin(t * 0.83) + 0.020 * sin(t * 1.31),\\n"
                           "                           0.030 * sin(t * 1.03 + 1.0));"),
            QStringLiteral("center += I * vec2(0.040 * sin(t * 0.83) + 0.020 * sin(t * 1.31),\\n"
                           "                           0.030 * sin(t * 1.03 + 1.0));\\n"
                           "        // PREVIEW_OVERRIDE: fixed full size, centered\\n"
                           "        I = 1.0; sz = 1.0; center = vec2(0.5, 0.5);"));
        // 强制 dil=1.0 (满速盘面旋转)
        fragBody.replace(
            QStringLiteral("float dil = mix(1.0, DILATION_MIN, I);"),
            QStringLiteral("float dil = 1.0; // PREVIEW_OVERRIDE: fixed full speed"));
    }

    QString fragSrc = fragHeader + "\n" + fragBody;
    // 追加 main() 入口: blackhole.glsl 使用 Shadertoy 风格的 mainImage()
    fragSrc += "\nvoid main() { vec4 c; mainImage(c, gl_FragCoord.xy); fragColor = c; }\n";
    m_program = new QOpenGLShaderProgram();

    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertSrc)) {
        qWarning() << "BlackholePreviewFBO: vertex shader error:" << m_program->log();
        return false;
    }
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragSrc)) {
        qWarning() << "BlackholePreviewFBO: fragment shader error:" << m_program->log();
        return false;
    }
    if (!m_program->link()) {
        qWarning() << "BlackholePreviewFBO: link error:" << m_program->log();
        return false;
    }
    qDebug() << "BlackholePreviewFBO: shader compiled OK";
    return true;
}

bool BlackholePreviewRenderer::initBlackTexture()
{
    // 尝试加载星空背景 PNG
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList searchPaths;
    searchPaths << ":/new/prefix1/fonts/pic/Starry_sky_background.png";
    searchPaths << appDir + "/fonts/pic/Starry_sky_background.png";
    searchPaths << appDir + "/../fonts/pic/Starry_sky_background.png";
    searchPaths << appDir + "/../../fonts/pic/Starry_sky_background.png";  // 回到源代码目录
    searchPaths << QDir::currentPath() + "/fonts/pic/Starry_sky_background.png";

    QImage img;
    for (const QString &p : searchPaths) {
        if (QFileInfo::exists(p)) {
            img = QImage(p);
            if (!img.isNull()) {
                qDebug() << "BlackholePreviewFBO: loaded background texture:" << p
                         << "size:" << img.width() << "x" << img.height();
                break;
            }
        }
    }

    glGenTextures(1, &m_blackTex);
    glBindTexture(GL_TEXTURE_2D, m_blackTex);

    if (!img.isNull()) {
        // 上传 PNG 纹理
        img = img.convertToFormat(QImage::Format_RGB888).mirrored(false, true); // OpenGL 期望原点在左下
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width(), img.height(),
                     0, GL_RGB, GL_UNSIGNED_BYTE, img.bits());
    } else {
        // 回退: 暗色纯色纹理
        qDebug() << "BlackholePreviewFBO: background PNG not found, using solid color fallback";
        unsigned char darkPixel[3] = {30, 30, 45};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, darkPixel);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void BlackholePreviewRenderer::render()
{
    static int renderCount = 0;
    if (++renderCount <= 3) qDebug() << "BlackholePreviewFBO: render() frame" << renderCount;
    if (!m_initialized) {
        initializeOpenGLFunctions();
        if (!initShaders()) { m_shaderError = true; m_initialized = true; return; }
        initBlackTexture();

        const float vertices[] = { -1,-1, 1,-1, -1,1, 1,1 };
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        m_initialized = true;
        qDebug() << "BlackholePreviewFBO: initialized";
    }

    QOpenGLFramebufferObject *fbo = framebufferObject();
    if (!fbo) return;

    if (m_shaderError) {
        glClearColor(1,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT);
        if (m_running) update();
        return;
    }

    int fbW = fbo->width();
    int fbH = fbo->height();
    glViewport(0, 0, fbW, fbH);
    glClearColor(1.0f, 0.5f, 0.0f, 1.0f);  // DIAGNOSTIC: bright orange
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_program || !m_program->isLinked()) { if (m_running) update(); return; }

    m_program->bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_blackTex);
    m_program->setUniformValue("iChannel0", 0);

    float t = m_timer.elapsed() / 1000.0f;
    m_program->setUniformValue("iResolution", QVector3D((float)fbW, (float)fbH, 0));
    m_program->setUniformValue("iTime", t);
    m_program->setUniformValue("iDate", QVector4D(0,0,0,(float)time(nullptr)));
    m_program->setUniformValue("iMouse", QVector4D(0,0,0,0));
    m_program->setUniformValue("iCurrentCursorColor", QVector4D(0,0,0,0));
    m_program->setUniformValue("iPreviousCursorColor", QVector4D(0,0,0,0));
    m_program->setUniformValue("iTimeCursorChange", 0.0f);

    m_program->setUniformValue("uHoleRadius", -1.0f);
    m_program->setUniformValue("uDiskGain",   m_diskGain);
    m_program->setUniformValue("uDiskTemp",   m_diskTemp);
    m_program->setUniformValue("uExposure",   m_diskExpo);
    m_program->setUniformValue("uSpeed",      m_diskSpeed);
    m_program->setUniformValue("uStarGain",   m_diskStar);
    m_program->setUniformValue("uDiskIncl",   m_diskIncl);
    m_program->setUniformValue("uBornProgress", 1.0f);
    m_program->setUniformValue("uPlayMode",    0);
    m_program->setUniformValue("uSlotSec",     5.25f);
    m_program->setUniformValue("uPresetCount", 1);

    auto setArr = [&](const char *name, float v) {
        float arr[64] = {}; arr[0] = v;
        m_program->setUniformValueArray(name, arr, 64, 1);
    };
    setArr("uPresetTemp",    m_diskTemp);
    setArr("uPresetIncl",    m_diskIncl);
    setArr("uPresetRoll",    m_diskRoll);
    setArr("uPresetInner",   m_diskInner);
    setArr("uPresetOuter",   m_diskOuter);
    setArr("uPresetOpac",    m_diskOpac);
    setArr("uPresetDopp",    m_diskDopp);
    setArr("uPresetBeam",    m_diskBeam);
    setArr("uPresetGain",    m_diskGain);
    setArr("uPresetContr",   m_diskContr);
    setArr("uPresetWind",    m_diskWind);
    setArr("uPresetSpd",     m_diskSpeed);
    setArr("uPresetExpo",    m_diskExpo);
    setArr("uPresetStar",    m_diskStar);

    m_program->setUniformValue("uHomeX",        0.5f);
    m_program->setUniformValue("uHomeY",        0.5f);
    m_program->setUniformValue("uRandPhase",    0.0f);
    m_program->setUniformValue("uPresetOffset", 0.0f);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    m_program->release();

    if (m_running) update();
}
