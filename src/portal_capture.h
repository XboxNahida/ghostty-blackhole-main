#pragma once

#include <QObject>
#include <QByteArray>
#include <QMutex>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <functional>

struct pw_thread_loop;
struct pw_context;
struct pw_core;
struct pw_stream;
struct spa_hook;
struct spa_pod;

class PortalCapture final : public QObject
{
    Q_OBJECT
public:
    struct Frame { QByteArray pixels; int width = 0; int height = 0; quint64 serial = 0; };
    struct StreamInfo { uint nodeId = 0; int width = 0; int height = 0; };

    explicit PortalCapture(QObject *parent = nullptr);
    ~PortalCapture() override;

    void startCapture(const QString &parentWindow = {});
    void stopCapture();
    bool isCapturing() const { return m_capturing; }
    bool isActive() const { return m_active; }
    bool takeLatestFrame(Frame &frame);

    // Pure protocol/frame boundaries used by the production callbacks and tests.
    static bool parseFirstStream(const QVariant &value, StreamInfo &stream, QString *error = nullptr);
    static bool copyCompleteBgraFrame(const uchar *source, qsizetype available, int stride,
                                      int width, int height, QByteArray &destination);
    static QString capabilityError(uint sourceTypes, uint cursorModes);
    static QVariantList startArguments(const QString &sessionHandle,
                                       const QString &parentWindow,
                                       const QVariantMap &options);

signals:
    void captureStarted();
    void captureStopped();
    void captureFailed(const QString &reason);
    void frameReceived(int width, int height);

private slots:
    void onSessionResponse(uint code, const QVariantMap &results);
    void onSelectSourcesResponse(uint code, const QVariantMap &results);
    void onStartResponse(uint code, const QVariantMap &results);

private:
    enum class Stage { Idle, Checking, Creating, Selecting, Starting, Opening, Streaming };
    void checkCapabilities();
    void fail(const QString &reason);
    void beginRequest(const QString &method, const QVariantList &arguments,
                      const QString &requestToken, int timeoutMs, Stage stage,
                      const char *slot);
    void requestSession();
    void selectSources();
    void startSession();
    void openPipeWireRemote();
    void connectPipeWire(int ownedFd);
    void teardownPipeWire();
    void closeSession();
    void setStage(Stage stage, int timeoutMs = 0);
    QString requestPathFor(const QString &token) const;

    QString m_sessionHandle;
    QString m_currentRequestPath;
    QString m_parentWindow;
    uint m_pwNodeId = 0;
    int m_streamWidth = 0;
    int m_streamHeight = 0;
    bool m_capturing = false;
    bool m_active = false;
    Stage m_stage = Stage::Idle;
    quint64 m_generation = 0;
    QTimer m_timeout;

    pw_thread_loop *m_pwLoop = nullptr;
    pw_context *m_pwContext = nullptr;
    pw_core *m_pwCore = nullptr;
    pw_stream *m_pwStream = nullptr;
    spa_hook *m_pwStreamListener = nullptr;

    QMutex m_frameMutex;
    Frame m_latestFrame;
    quint64 m_takenSerial = 0;

    friend void portalPwParamChanged(void *, uint32_t, const spa_pod *);
    friend void portalPwProcess(void *);
};
