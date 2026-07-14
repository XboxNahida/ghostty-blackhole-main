#include "portal_capture.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusUnixFileDescriptor>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QDebug>
#include <QSize>
#include <fcntl.h>
#include <unistd.h>

#include <pipewire/pipewire.h>
#include <spa/param/video/raw-utils.h>

namespace {
const QString service = QStringLiteral("org.freedesktop.portal.Desktop");
const QString desktopPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString screenCast = QStringLiteral("org.freedesktop.portal.ScreenCast");
const QString requestIface = QStringLiteral("org.freedesktop.portal.Request");
const QString sessionIface = QStringLiteral("org.freedesktop.portal.Session");

QString token()
{
    return QStringLiteral("blackhole_%1").arg(QRandomGenerator::global()->generate64(), 0, 16);
}
}

PortalCapture::PortalCapture(QObject *parent) : QObject(parent)
{
    pw_init(nullptr, nullptr);
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        fail(QStringLiteral("Portal request timed out"));
    });
}

PortalCapture::~PortalCapture() { stopCapture(); }

QString PortalCapture::requestPathFor(const QString &requestToken) const
{
    QString sender = QDBusConnection::sessionBus().baseService();
    sender.remove(0, 1).replace('.', '_');
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, requestToken);
}

void PortalCapture::setStage(Stage stage, int timeoutMs)
{
    m_stage = stage;
    m_timeout.stop();
    if (timeoutMs > 0) m_timeout.start(timeoutMs);
}

void PortalCapture::startCapture(const QString &parentWindow)
{
    if (m_capturing) return;
    ++m_generation;
    m_capturing = true;
    m_parentWindow = parentWindow;
    checkCapabilities();
}

QString PortalCapture::capabilityError(uint sourceTypes, uint cursorModes)
{
    if (sourceTypes == 0)
        return QStringLiteral("Portal ScreenCast has no available source types (AvailableSourceTypes=0)");
    if (cursorModes == 0)
        return QStringLiteral("Portal ScreenCast has no available cursor modes (AvailableCursorModes=0)");
    if ((sourceTypes & 1u) == 0)
        return QStringLiteral("Portal ScreenCast does not support monitor capture");
    if ((cursorModes & 2u) == 0)
        return QStringLiteral("Portal ScreenCast does not support embedded cursor mode");
    return {};
}

void PortalCapture::checkCapabilities()
{
    setStage(Stage::Checking, 10000);
    QDBusMessage message = QDBusMessage::createMethodCall(service, desktopPath,
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
    message << screenCast;
    const quint64 generation = m_generation;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *done) {
        const QDBusMessage reply = done->reply(); done->deleteLater();
        if (!m_capturing || generation != m_generation || m_stage != Stage::Checking) return;
        m_timeout.stop();
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
            fail(QStringLiteral("Cannot read Portal ScreenCast capabilities: %1").arg(reply.errorMessage()));
            return;
        }
        const QVariantMap properties = qdbus_cast<QVariantMap>(reply.arguments().constFirst());
        const uint sources = properties.value(QStringLiteral("AvailableSourceTypes")).toUInt();
        const uint cursors = properties.value(QStringLiteral("AvailableCursorModes")).toUInt();
        const QString error = capabilityError(sources, cursors);
        if (!error.isEmpty()) { fail(error); return; }
        requestSession();
    });
}

void PortalCapture::fail(const QString &reason)
{
    if (!m_capturing) return;
    emit captureFailed(reason);
    stopCapture();
}

void PortalCapture::beginRequest(const QString &method, const QVariantList &arguments,
                                 const QString &requestToken, int timeoutMs, Stage stage,
                                 const char *slot)
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, desktopPath, screenCast, method);
    message.setArguments(arguments);
    setStage(stage, timeoutMs);
    // The portal may emit Response immediately after returning the request
    // handle. Subscribe to the deterministic handle_token path before making
    // the method call so that an early response cannot be lost.
    m_currentRequestPath = requestPathFor(requestToken);
    if (!QDBusConnection::sessionBus().connect(service, m_currentRequestPath, requestIface,
                                                QStringLiteral("Response"), this, slot)) {
        fail(QStringLiteral("Cannot subscribe to the Portal request response"));
        return;
    }
    const quint64 generation = m_generation;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, slot](QDBusPendingCallWatcher *done) {
        const QDBusMessage reply = done->reply(); done->deleteLater();
        if (!m_capturing || generation != m_generation) return;
        if (reply.type() == QDBusMessage::ErrorMessage) {
            fail(QStringLiteral("Portal method failed: %1").arg(reply.errorMessage())); return;
        }
        const QString returnedPath = reply.arguments().value(0).value<QDBusObjectPath>().path();
        if (returnedPath.isEmpty() || returnedPath != m_currentRequestPath) {
            fail(QStringLiteral("Portal returned an invalid request handle"));
        }
    });
}

void PortalCapture::requestSession()
{
    const QString requestToken = token();
    const QVariantMap options{{QStringLiteral("handle_token"), requestToken},
                              {QStringLiteral("session_handle_token"), token()}};
    beginRequest(QStringLiteral("CreateSession"), {options}, requestToken, 10000, Stage::Creating,
                 SLOT(onSessionResponse(uint,QVariantMap)));
}

void PortalCapture::onSessionResponse(uint code, const QVariantMap &results)
{
    if (!m_capturing || m_stage != Stage::Creating) return;
    QDBusConnection::sessionBus().disconnect(service, m_currentRequestPath, requestIface,
                                              QStringLiteral("Response"), this,
                                              SLOT(onSessionResponse(uint,QVariantMap)));
    m_timeout.stop();
    m_sessionHandle = results.value(QStringLiteral("session_handle")).toString();
    if (code != 0 || m_sessionHandle.isEmpty()) {
        fail(code == 1 ? QStringLiteral("Screen selection cancelled")
                       : QStringLiteral("CreateSession failed (%1)").arg(code));
        return;
    }
    selectSources();
}

void PortalCapture::selectSources()
{
    const QString t = token();
    const QVariantMap options{{QStringLiteral("handle_token"), t},
                              {QStringLiteral("types"), uint(1)},
                              {QStringLiteral("multiple"), false},
                              {QStringLiteral("cursor_mode"), uint(2)}};
    beginRequest(QStringLiteral("SelectSources"), {QVariant::fromValue(QDBusObjectPath(m_sessionHandle)), options}, t, 30000,
                 Stage::Selecting, SLOT(onSelectSourcesResponse(uint,QVariantMap)));
}

void PortalCapture::onSelectSourcesResponse(uint code, const QVariantMap &)
{
    if (!m_capturing || m_stage != Stage::Selecting) return;
    QDBusConnection::sessionBus().disconnect(service, m_currentRequestPath, requestIface,
                                              QStringLiteral("Response"), this,
                                              SLOT(onSelectSourcesResponse(uint,QVariantMap)));
    m_timeout.stop();
    if (code != 0) {
        fail(code == 1 ? QStringLiteral("Screen selection cancelled")
                       : QStringLiteral("SelectSources failed (%1)").arg(code));
        return;
    }
    startSession();
}

void PortalCapture::startSession()
{
    const QString t = token();
    const QVariantMap options{{QStringLiteral("handle_token"), t}};
    beginRequest(QStringLiteral("Start"), startArguments(m_sessionHandle, m_parentWindow, options), t, 30000,
                 Stage::Starting, SLOT(onStartResponse(uint,QVariantMap)));
}

QVariantList PortalCapture::startArguments(const QString &sessionHandle,
                                           const QString &parentWindow,
                                           const QVariantMap &options)
{
    return {QVariant::fromValue(QDBusObjectPath(sessionHandle)), parentWindow, options};
}

bool PortalCapture::parseFirstStream(const QVariant &value, StreamInfo &stream, QString *error)
{
    auto bad = [&](const QString &why) { if (error) *error = why; return false; };
    if (!value.isValid()) return bad(QStringLiteral("missing streams"));
    const QDBusArgument argument = value.value<QDBusArgument>();
    if (argument.currentType() == QDBusArgument::UnknownType) return bad(QStringLiteral("invalid streams type"));
    argument.beginArray();
    if (argument.atEnd()) { argument.endArray(); return bad(QStringLiteral("no streams")); }
    uint node = 0; QVariantMap properties;
    argument.beginStructure(); argument >> node >> properties; argument.endStructure();
    argument.endArray();
    if (node == 0) return bad(QStringLiteral("invalid stream node"));
    stream.nodeId = node;
    const QSize size = properties.value(QStringLiteral("size")).toSize();
    stream.width = size.width(); stream.height = size.height();
    return true;
}

void PortalCapture::onStartResponse(uint code, const QVariantMap &results)
{
    if (!m_capturing || m_stage != Stage::Starting) return;
    QDBusConnection::sessionBus().disconnect(service, m_currentRequestPath, requestIface,
                                              QStringLiteral("Response"), this,
                                              SLOT(onStartResponse(uint,QVariantMap)));
    m_timeout.stop();
    if (code != 0) { fail(QStringLiteral("Start failed (%1)").arg(code)); return; }
    StreamInfo stream; QString error;
    if (!parseFirstStream(results.value(QStringLiteral("streams")), stream, &error)) {
        fail(QStringLiteral("Start response: %1").arg(error)); return;
    }
    m_pwNodeId = stream.nodeId; m_streamWidth = stream.width; m_streamHeight = stream.height;
    openPipeWireRemote();
}

void PortalCapture::openPipeWireRemote()
{
    setStage(Stage::Opening, 10000);
    QDBusMessage message = QDBusMessage::createMethodCall(service, desktopPath, screenCast,
                                                           QStringLiteral("OpenPipeWireRemote"));
    message << QVariant::fromValue(QDBusObjectPath(m_sessionHandle)) << QVariantMap{};
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *done) {
        const QDBusMessage reply = done->reply(); done->deleteLater();
        if (!m_capturing || m_stage != Stage::Opening) return;
        m_timeout.stop();
        if (reply.type() == QDBusMessage::ErrorMessage) { fail(reply.errorMessage()); return; }
        const QDBusUnixFileDescriptor descriptor = reply.arguments().value(0).value<QDBusUnixFileDescriptor>();
        const int fd = descriptor.isValid() ? fcntl(descriptor.fileDescriptor(), F_DUPFD_CLOEXEC, 3) : -1;
        if (fd < 0) { fail(QStringLiteral("Portal returned an invalid PipeWire fd")); return; }
        connectPipeWire(fd);
    });
}

bool PortalCapture::copyCompleteBgraFrame(const uchar *source, qsizetype available, int stride,
                                           int width, int height, QByteArray &destination)
{
    if (!source || width <= 0 || height <= 0 || width > 7680 || height > 4320) return false;
    const qsizetype row = qsizetype(width) * 4;
    if (stride < row || available < qsizetype(stride) * (height - 1) + row) return false;
    QByteArray copy(row * height, Qt::Uninitialized);
    for (int y = 0; y < height; ++y) memcpy(copy.data() + y * row, source + y * stride, row);
    destination.swap(copy);
    return true;
}

void portalPwParamChanged(void *data, uint32_t id, const spa_pod *param)
{
    auto *self = static_cast<PortalCapture *>(data);
    if (!self || id != SPA_PARAM_Format || !param) return;
    spa_video_info_raw format{};
    if (spa_format_video_raw_parse(param, &format) < 0 || format.format != SPA_VIDEO_FORMAT_BGRA) return;
    QMutexLocker lock(&self->m_frameMutex);
    self->m_streamWidth = int(format.size.width); self->m_streamHeight = int(format.size.height);
}

void portalPwProcess(void *data)
{
    auto *self = static_cast<PortalCapture *>(data);
    pw_buffer *buffer = self ? pw_stream_dequeue_buffer(self->m_pwStream) : nullptr;
    if (!buffer) return;
    spa_data &plane = buffer->buffer->datas[0];
    QByteArray pixels; int width, height;
    { QMutexLocker lock(&self->m_frameMutex); width = self->m_streamWidth; height = self->m_streamHeight; }
    const int stride = plane.chunk && plane.chunk->stride > 0 ? plane.chunk->stride : width * 4;
    const uchar *source = plane.data && plane.chunk ? static_cast<const uchar *>(plane.data) + plane.chunk->offset : nullptr;
    const qsizetype available = plane.chunk ? qMin<qsizetype>(plane.chunk->size, plane.maxsize - qMin(plane.chunk->offset, plane.maxsize)) : 0;
    if (PortalCapture::copyCompleteBgraFrame(source, available, stride, width, height, pixels)) {
        QMutexLocker lock(&self->m_frameMutex);
        self->m_latestFrame = {pixels, width, height, self->m_latestFrame.serial + 1};
        QMetaObject::invokeMethod(self, [self, width, height] { emit self->frameReceived(width, height); }, Qt::QueuedConnection);
    }
    pw_stream_queue_buffer(self->m_pwStream, buffer);
}

static const pw_stream_events streamEvents = {
    .version = PW_VERSION_STREAM_EVENTS,
    .param_changed = portalPwParamChanged,
    .process = portalPwProcess,
};

void PortalCapture::connectPipeWire(int ownedFd)
{
    m_pwLoop = pw_thread_loop_new("blackhole-portal", nullptr);
    if (!m_pwLoop) { close(ownedFd); fail(QStringLiteral("Cannot create PipeWire loop")); return; }
    m_pwContext = pw_context_new(pw_thread_loop_get_loop(m_pwLoop), nullptr, 0);
    if (!m_pwContext) { close(ownedFd); fail(QStringLiteral("Cannot create PipeWire context")); return; }
    m_pwCore = pw_context_connect_fd(m_pwContext, ownedFd, nullptr, 0); // takes fd ownership on success
    if (!m_pwCore) { close(ownedFd); fail(QStringLiteral("Cannot connect PipeWire remote")); return; }
    m_pwStream = pw_stream_new(m_pwCore, "blackhole-desktop-capture",
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Screen", nullptr));
    if (!m_pwStream) { fail(QStringLiteral("Cannot create PipeWire stream")); return; }
    m_pwStreamListener = new spa_hook{};
    pw_stream_add_listener(m_pwStream, m_pwStreamListener, &streamEvents, this);
    uint8_t buffer[1024]; spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    spa_video_info_raw info{}; info.format = SPA_VIDEO_FORMAT_BGRA;
    const spa_pod *params[] = {spa_format_video_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
    if (pw_stream_connect(m_pwStream, PW_DIRECTION_INPUT, m_pwNodeId,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS), params, 1) < 0 ||
        pw_thread_loop_start(m_pwLoop) < 0) {
        fail(QStringLiteral("Cannot start PipeWire stream")); return;
    }
    m_active = true; setStage(Stage::Streaming); emit captureStarted();
}

bool PortalCapture::takeLatestFrame(Frame &frame)
{
    QMutexLocker lock(&m_frameMutex);
    if (m_latestFrame.serial == 0 || m_latestFrame.serial == m_takenSerial) return false;
    frame = m_latestFrame; m_takenSerial = frame.serial; return true;
}

void PortalCapture::teardownPipeWire()
{
    if (m_pwLoop) pw_thread_loop_stop(m_pwLoop);
    if (m_pwStream) { pw_stream_disconnect(m_pwStream); if (m_pwStreamListener) spa_hook_remove(m_pwStreamListener); pw_stream_destroy(m_pwStream); }
    delete m_pwStreamListener; m_pwStreamListener = nullptr; m_pwStream = nullptr;
    if (m_pwCore) pw_core_disconnect(m_pwCore); m_pwCore = nullptr;
    if (m_pwContext) pw_context_destroy(m_pwContext); m_pwContext = nullptr;
    if (m_pwLoop) pw_thread_loop_destroy(m_pwLoop); m_pwLoop = nullptr;
}

void PortalCapture::closeSession()
{
    if (m_sessionHandle.isEmpty()) return;
    QDBusMessage close = QDBusMessage::createMethodCall(service, m_sessionHandle, sessionIface, QStringLiteral("Close"));
    QDBusConnection::sessionBus().call(close, QDBus::NoBlock);
    m_sessionHandle.clear();
}

void PortalCapture::stopCapture()
{
    if (!m_capturing && m_stage == Stage::Idle) return;
    ++m_generation; m_timeout.stop(); m_capturing = false; m_active = false;
    teardownPipeWire(); closeSession(); setStage(Stage::Idle);
    { QMutexLocker lock(&m_frameMutex); m_latestFrame = {}; m_takenSerial = 0; }
    emit captureStopped();
}
