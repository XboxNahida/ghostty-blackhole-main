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

QString detailsWithLogTail(const QString &technicalDetails,
                           const QByteArray &logTail)
{
    if (logTail.isEmpty()) {
        return cappedUtf8(technicalDetails);
    }

    QString technicalPrefix = technicalDetails;
    while (technicalPrefix.toUtf8().size() > 2048) {
        technicalPrefix.chop(1);
    }

    const QString separator = QStringLiteral("\n\n--- 日志尾部 ---\n");
    QString tail = QString::fromUtf8(logTail);
    QString details = technicalPrefix + separator + tail;
    while (details.toUtf8().size() > kMaxDiagnosticLogBytes && !tail.isEmpty()) {
        tail.remove(0, 1);
        details = technicalPrefix + separator + tail;
    }
    return cappedUtf8(details);
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
    m_incompleteLogLine.clear();
    m_logTail.clear();
    Q_UNUSED(previousLogModified);
    return m_attemptId;
}

RendererDiagnostic RendererStartupDiagnostics::validateRequiredFiles(
    const QString &exePath,
    const QString &workingDirectory,
    const QStringList &relativeRequiredFiles)
{
    QStringList missingPaths;
    const bool rendererMissing = !QFileInfo(exePath).isFile();
    if (rendererMissing) {
        missingPaths.append(exePath);
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

    if (rendererMissing) {
        return fail(
            RendererFailureKind::MissingFile,
            QStringLiteral("blackhole.exe 缺失，可能未完整解压或被安全软件隔离"),
            QStringLiteral(
                "请从官方发布包完整解压所有文件。若安全软件已隔离该程序，"
                "只在确认官方来源和 SHA-256 后恢复或单独放行；不得关闭全部防护。"
                "\n\n未找到以下文件：\n%1")
                .arg(missingPaths.join(QLatin1Char('\n'))));
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
    const RendererDiagnostic diagnostic = consumeLogChunk(
        addedContent, startOffset, fileReplacedOrTruncated || nonNegativeFileSize < m_logOffset);
    m_logOffset = nonNegativeFileSize;
    return diagnostic;
}

RendererDiagnostic RendererStartupDiagnostics::consumeLogChunk(
    const QByteArray &content,
    qint64 chunkOffset,
    bool fileReplacedOrTruncated)
{
    if (m_state != RendererStartupState::Starting) {
        return {};
    }

    const qint64 nonNegativeOffset = std::max<qint64>(0, chunkOffset);
    if (fileReplacedOrTruncated || nonNegativeOffset != m_logOffset) {
        m_incompleteLogLine.clear();
        m_logTail.clear();
    }

    m_logTail.append(content);
    if (m_logTail.size() > kMaxDiagnosticLogBytes) {
        m_logTail = m_logTail.right(kMaxDiagnosticLogBytes);
    }

    const QByteArray contentToParse = m_incompleteLogLine + content;
    m_logOffset = nonNegativeOffset + content.size();

    const QString failureLine = lastFailureLine(contentToParse);
    if (!failureLine.isEmpty()) {
        m_incompleteLogLine.clear();
        return fail(RendererFailureKind::InitializationFailed,
                    QStringLiteral("渲染器初始化失败"),
                    failureLine);
    }

    if (contentToParse.contains(kReadyMarker)) {
        m_incompleteLogLine.clear();
        m_state = RendererStartupState::Ready;
        return {};
    }

    const qsizetype lastLineBreak = contentToParse.lastIndexOf('\n');
    m_incompleteLogLine = lastLineBreak >= 0
        ? contentToParse.mid(lastLineBreak + 1)
        : contentToParse;
    if (m_incompleteLogLine.size() > kMaxDiagnosticLogBytes) {
        m_incompleteLogLine = m_incompleteLogLine.right(kMaxDiagnosticLogBytes);
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

qint64 RendererStartupDiagnostics::logOffset() const
{
    return m_logOffset;
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
    const bool includeLogTail = kind == RendererFailureKind::InitializationFailed
                                || kind == RendererFailureKind::ExitedBeforeReady
                                || kind == RendererFailureKind::ReadyTimeout;
    diagnostic.details = includeLogTail
        ? detailsWithLogTail(technicalDetails, m_logTail)
        : cappedUtf8(technicalDetails);
    diagnostic.logPath = m_logPath;
    return diagnostic;
}
