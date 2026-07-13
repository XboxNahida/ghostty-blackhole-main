#include "renderer_startup_diagnostics.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace {

static constexpr qsizetype kMaxDiagnosticLogBytes = 8192;
static constexpr auto kReadyMarker = "[OK] Ready, entering main loop";
static constexpr auto kFailureMarker = "[FAIL]";

QString cappedUtf8(const QString &text)
{
    if (text.toUtf8().size() <= kMaxDiagnosticLogBytes) {
        return text;
    }

    QString capped = QString::fromUtf8(text.toUtf8().left(kMaxDiagnosticLogBytes));
    while (capped.toUtf8().size() > kMaxDiagnosticLogBytes) {
        capped.chop(1);
    }
    return capped;
}

QString lastFailureLine(const QByteArray &content)
{
    const qsizetype markerPosition = content.lastIndexOf(kFailureMarker);
    if (markerPosition < 0) {
        return {};
    }

    qsizetype lineEnd = content.indexOf('\n', markerPosition);
    if (lineEnd < 0) {
        lineEnd = content.size();
    }
    QByteArray line = content.mid(markerPosition, lineEnd - markerPosition);
    if (line.endsWith('\r')) {
        line.chop(1);
    }
    return QString::fromUtf8(line);
}

} // namespace

quint64 RendererStartupDiagnostics::beginAttempt(
    const QString &logPath,
    qint64 previousLogSize,
    const QDateTime &previousLogModified)
{
    ++m_attemptId;
    m_state = RendererStartupState::Starting;
    m_reportedCurrentAttempt = false;
    m_logPath = logPath;
    m_logOffset = std::max<qint64>(0, previousLogSize);
    m_previousLogModified = previousLogModified;
    return m_attemptId;
}

RendererDiagnostic RendererStartupDiagnostics::validateRequiredFiles(
    const QString &exePath,
    const QString &workingDirectory,
    const QStringList &relativeRequiredFiles)
{
    QStringList missingPaths;
    if (!QFileInfo(exePath).isFile()) {
        missingPaths.append(QDir::toNativeSeparators(exePath));
    }

    const QDir workingDir(workingDirectory);
    for (const QString &relativePath : relativeRequiredFiles) {
        const QString absolutePath = workingDir.filePath(relativePath);
        if (!QFileInfo(absolutePath).isFile()) {
            missingPaths.append(QDir::toNativeSeparators(absolutePath));
        }
    }

    if (missingPaths.isEmpty()) {
        return {};
    }

    return fail(RendererFailureKind::MissingFile,
                QStringLiteral("渲染器所需文件缺失"),
                QStringLiteral("未找到以下文件：\n%1")
                    .arg(missingPaths.join(QLatin1Char('\n'))));
}

RendererDiagnostic RendererStartupDiagnostics::consumeLogSnapshot(
    const QByteArray &content,
    qint64 fileSize,
    bool fileReplacedOrTruncated)
{
    if (m_state != RendererStartupState::Starting) {
        return {};
    }

    const qint64 nonNegativeFileSize = std::max<qint64>(0, fileSize);
    qint64 startOffset = m_logOffset;
    if (fileReplacedOrTruncated || nonNegativeFileSize < m_logOffset) {
        startOffset = 0;
    }

    QByteArray addedContent;
    if (startOffset < content.size()) {
        addedContent = content.mid(static_cast<qsizetype>(startOffset));
    }
    m_logOffset = nonNegativeFileSize;

    const QString failureLine = lastFailureLine(addedContent);
    if (!failureLine.isEmpty()) {
        return fail(RendererFailureKind::InitializationFailed,
                    QStringLiteral("渲染器初始化失败"),
                    failureLine);
    }

    if (addedContent.contains(kReadyMarker)) {
        m_state = RendererStartupState::Ready;
    }
    return {};
}

RendererDiagnostic RendererStartupDiagnostics::processFailedToStart(
    const QString &processError)
{
    return fail(RendererFailureKind::FailedToStart,
                QStringLiteral("无法启动渲染器进程"),
                processError);
}

RendererDiagnostic RendererStartupDiagnostics::processFinished(int exitCode,
                                                                bool crashed)
{
    if (m_state == RendererStartupState::Stopping) {
        m_state = RendererStartupState::Stopped;
        return {};
    }

    if (m_state == RendererStartupState::Starting) {
        const QString details = crashed
            ? QStringLiteral("Renderer crashed before Ready (exit code: %1).")
                  .arg(exitCode)
            : QStringLiteral("Renderer exited before Ready (exit code: %1).")
                  .arg(exitCode);
        return fail(RendererFailureKind::ExitedBeforeReady,
                    QStringLiteral("渲染器在就绪前退出"),
                    details);
    }

    if (m_state == RendererStartupState::Ready) {
        m_state = RendererStartupState::Stopped;
    }
    return {};
}

RendererDiagnostic RendererStartupDiagnostics::timeout()
{
    return fail(RendererFailureKind::ReadyTimeout,
                QStringLiteral("渲染器启动超时"),
                QStringLiteral("Renderer did not become Ready within 8000 ms."));
}

void RendererStartupDiagnostics::beginStopping()
{
    m_state = RendererStartupState::Stopping;
}

RendererStartupState RendererStartupDiagnostics::state() const
{
    return m_state;
}

quint64 RendererStartupDiagnostics::attemptId() const
{
    return m_attemptId;
}

RendererDiagnostic RendererStartupDiagnostics::fail(
    RendererFailureKind kind,
    const QString &summary,
    const QString &technicalDetails)
{
    if (m_state != RendererStartupState::Starting || m_reportedCurrentAttempt) {
        return {};
    }

    m_reportedCurrentAttempt = true;
    m_state = RendererStartupState::Failed;

    RendererDiagnostic diagnostic;
    diagnostic.valid = true;
    diagnostic.attemptId = m_attemptId;
    diagnostic.kind = kind;
    diagnostic.title = QStringLiteral("渲染器启动失败");
    diagnostic.summary = summary;
    diagnostic.details = cappedUtf8(technicalDetails);
    diagnostic.logPath = m_logPath;
    return diagnostic;
}
