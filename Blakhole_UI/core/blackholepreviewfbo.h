// blackholepreviewfbo.h — QQuickFramebufferObject 黑洞实时预览
// 在 QML 界面中嵌入真实 blackhole.glsl Shader 渲染
#pragma once

#include <QQuickFramebufferObject>
#include <QElapsedTimer>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions_3_3_Core>

// QML 可见的 FBO Item
class BlackholePreviewFBO : public QQuickFramebufferObject {
    Q_OBJECT

    // 14 个可调参数 — 从 QML 绑定
    Q_PROPERTY(float diskTemp  READ diskTemp  WRITE setDiskTemp  NOTIFY diskTempChanged)
    Q_PROPERTY(float diskIncl  READ diskIncl  WRITE setDiskIncl  NOTIFY diskInclChanged)
    Q_PROPERTY(float diskRoll  READ diskRoll  WRITE setDiskRoll  NOTIFY diskRollChanged)
    Q_PROPERTY(float diskInner READ diskInner WRITE setDiskInner NOTIFY diskInnerChanged)
    Q_PROPERTY(float diskOuter READ diskOuter WRITE setDiskOuter NOTIFY diskOuterChanged)
    Q_PROPERTY(float diskOpac  READ diskOpac  WRITE setDiskOpac  NOTIFY diskOpacChanged)
    Q_PROPERTY(float diskDopp  READ diskDopp  WRITE setDiskDopp  NOTIFY diskDoppChanged)
    Q_PROPERTY(float diskBeam  READ diskBeam  WRITE setDiskBeam  NOTIFY diskBeamChanged)
    Q_PROPERTY(float diskGain  READ diskGain  WRITE setDiskGain  NOTIFY diskGainChanged)
    Q_PROPERTY(float diskContr READ diskContr WRITE setDiskContr NOTIFY diskContrChanged)
    Q_PROPERTY(float diskWind  READ diskWind  WRITE setDiskWind  NOTIFY diskWindChanged)
    Q_PROPERTY(float diskSpeed READ diskSpeed WRITE setDiskSpeed NOTIFY diskSpeedChanged)
    Q_PROPERTY(float diskExpo  READ diskExpo  WRITE setDiskExpo  NOTIFY diskExpoChanged)
    Q_PROPERTY(float diskStar  READ diskStar  WRITE setDiskStar  NOTIFY diskStarChanged)
    Q_PROPERTY(float holeSize READ holeSize WRITE setHoleSize NOTIFY holeSizeChanged)
    Q_PROPERTY(float movementSpeed READ movementSpeed WRITE setMovementSpeed NOTIFY movementSpeedChanged)
    Q_PROPERTY(float animationSpeed READ animationSpeed WRITE setAnimationSpeed NOTIFY animationSpeedChanged)
    Q_PROPERTY(bool fixedSize READ fixedSize WRITE setFixedSize NOTIFY fixedSizeChanged)
    Q_PROPERTY(float fixedLevel READ fixedLevel WRITE setFixedLevel NOTIFY fixedLevelChanged)

    // 动画控制
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)

public:
    explicit BlackholePreviewFBO(QQuickItem *parent = nullptr);

    float diskTemp()  const;
    float diskIncl()  const;
    float diskRoll()  const;
    float diskInner() const;
    float diskOuter() const;
    float diskOpac()  const;
    float diskDopp()  const;
    float diskBeam()  const;
    float diskGain()  const;
    float diskContr() const;
    float diskWind()  const;
    float diskSpeed() const;
    float diskExpo()  const;
    float diskStar()  const;
    float holeSize() const;
    float movementSpeed() const;
    float animationSpeed() const;
    bool fixedSize() const;
    float fixedLevel() const;
    bool  running()   const;

    void setDiskTemp(float v);
    void setDiskIncl(float v);
    void setDiskRoll(float v);
    void setDiskInner(float v);
    void setDiskOuter(float v);
    void setDiskOpac(float v);
    void setDiskDopp(float v);
    void setDiskBeam(float v);
    void setDiskGain(float v);
    void setDiskContr(float v);
    void setDiskWind(float v);
    void setDiskSpeed(float v);
    void setDiskExpo(float v);
    void setDiskStar(float v);
    void setHoleSize(float v);
    void setMovementSpeed(float v);
    void setAnimationSpeed(float v);
    void setFixedSize(bool v);
    void setFixedLevel(float v);
    void setRunning(bool v);

    // 从 C++ 批量设置 (用于 BlackHoleCore 的信号连接)
    Q_INVOKABLE void syncFromCore(QObject *core);

signals:
    void diskTempChanged();
    void diskInclChanged();
    void diskRollChanged();
    void diskInnerChanged();
    void diskOuterChanged();
    void diskOpacChanged();
    void diskDoppChanged();
    void diskBeamChanged();
    void diskGainChanged();
    void diskContrChanged();
    void diskWindChanged();
    void diskSpeedChanged();
    void diskExpoChanged();
    void diskStarChanged();
    void holeSizeChanged();
    void movementSpeedChanged();
    void animationSpeedChanged();
    void fixedSizeChanged();
    void fixedLevelChanged();
    void runningChanged();

protected:
    Renderer *createRenderer() const override;

private:
    float m_diskTemp  = 5500.0f;
    float m_diskIncl  = 1.50f;
    float m_diskRoll  = 0.35f;
    float m_diskInner = 1.8f;
    float m_diskOuter = 8.0f;
    float m_diskOpac  = 0.90f;
    float m_diskDopp  = 0.60f;
    float m_diskBeam  = 2.5f;
    float m_diskGain  = 2.2f;
    float m_diskContr = 1.6f;
    float m_diskWind  = 7.0f;
    float m_diskSpeed = 5.0f;
    float m_diskExpo  = 1.40f;
    float m_diskStar  = 0.0f;
    float m_holeSize = 1.0f;
    float m_movementSpeed = 1.0f;
    float m_animationSpeed = 1.0f;
    bool m_fixedSize = false;
    float m_fixedLevel = 1.0f;
    bool  m_running   = true;

    friend class BlackholePreviewRenderer;
};

// 内部渲染器 — 在 Scene Graph 渲染线程中执行
class BlackholePreviewRenderer : public QQuickFramebufferObject::Renderer,
                                  protected QOpenGLFunctions_3_3_Core {
public:
    BlackholePreviewRenderer();
    ~BlackholePreviewRenderer() override;

    void render() override;
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
    void synchronize(QQuickFramebufferObject *item) override;

private:
    bool initShaders();
    bool initBlackTexture();
    void resolveShaderPath(QString &vertPath, QString &fragHeaderPath, QString &fragBodyPath);
    QString readShaderFile(const QString &path);

    QOpenGLShaderProgram *m_program = nullptr;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_blackTex = 0;
    QElapsedTimer m_timer;
    bool m_initialized = false;
    bool m_shaderError = false;

    // 从 synchronize 拷贝的参数 (渲染线程使用)
    float m_diskTemp  = 5500.0f;
    float m_diskIncl  = 1.50f;
    float m_diskRoll  = 0.35f;
    float m_diskInner = 1.8f;
    float m_diskOuter = 8.0f;
    float m_diskOpac  = 0.90f;
    float m_diskDopp  = 0.60f;
    float m_diskBeam  = 2.5f;
    float m_diskGain  = 2.2f;
    float m_diskContr = 1.6f;
    float m_diskWind  = 7.0f;
    float m_diskSpeed = 5.0f;
    float m_diskExpo  = 1.40f;
    float m_diskStar  = 0.0f;
    float m_holeSize = 1.0f;
    float m_movementSpeed = 1.0f;
    float m_animationSpeed = 1.0f;
    bool m_fixedSize = false;
    float m_fixedLevel = 1.0f;
    bool  m_running   = true;
    QSize m_viewSize;
};
