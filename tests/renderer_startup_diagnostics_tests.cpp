#include "renderer_startup_diagnostics.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void createFile(const QString &path, const QByteArray &content = {})
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "temporary file opens for writing");
    require(file.write(content) == content.size(), "temporary file content is written");
}

void testAttemptAndMissingFiles(const QString &rootPath, const QString &logPath)
{
    RendererStartupDiagnostics diagnostics;
    const QDateTime oldModified = QDateTime::currentDateTime().addSecs(-60);
    const quint64 attempt = diagnostics.beginAttempt(logPath, 12, oldModified);
    require(attempt == 1, "first attempt id");
    require(diagnostics.attemptId() == attempt, "attempt id accessor");
    require(diagnostics.state() == RendererStartupState::Starting,
            "attempt enters Starting");

    const QString exePath = QDir(rootPath).filePath(QStringLiteral("blackhole.exe"));
    createFile(exePath);
    QDir(rootPath).mkpath(QStringLiteral("shaders"));
    createFile(QDir(rootPath).filePath(QStringLiteral("blackhole.glsl")));
    createFile(QDir(rootPath).filePath(QStringLiteral("shaders/vert.glsl")));

    const RendererDiagnostic missing = diagnostics.validateRequiredFiles(
        exePath,
        rootPath,
        {QStringLiteral("blackhole.glsl"),
         QStringLiteral("shaders/vert.glsl"),
         QStringLiteral("shaders/frag_desktop_header.glsl")});
    require(missing.valid && missing.kind == RendererFailureKind::MissingFile,
            "missing shader produces MissingFile");
    require(missing.attemptId == attempt, "diagnostic belongs to current attempt");
    require(missing.details.contains(QStringLiteral("frag_desktop_header.glsl")),
            "missing diagnostic names missing shader");
    require(diagnostics.state() == RendererStartupState::Failed,
            "missing file enters Failed");

    const RendererDiagnostic duplicate = diagnostics.processFailedToStart(
        QStringLiteral("should not be reported twice"));
    require(!duplicate.valid, "same attempt does not report twice");
}

void testReadyAndLogBaselines(const QString &logPath)
{
    RendererStartupDiagnostics diagnostics;
    diagnostics.beginAttempt(logPath, 0, {});
    const RendererDiagnostic ready = diagnostics.consumeLogSnapshot(
        QByteArrayLiteral("[OK] Ready, entering main loop\n"), 31, true);
    require(!ready.valid && diagnostics.state() == RendererStartupState::Ready,
            "current attempt Ready marker succeeds without diagnostic");

    const QByteArray oldReady = QByteArrayLiteral("[OK] Ready, entering main loop\n");
    diagnostics.beginAttempt(logPath, oldReady.size(), QDateTime::currentDateTime());
    const RendererDiagnostic ignored = diagnostics.consumeLogSnapshot(
        oldReady, oldReady.size(), false);
    require(!ignored.valid && diagnostics.state() == RendererStartupState::Starting,
            "old log Ready marker is ignored");

    const QByteArray truncated = QByteArrayLiteral("[OK] Ready, entering main loop\n");
    diagnostics.beginAttempt(logPath, oldReady.size() + 100, QDateTime::currentDateTime());
    const RendererDiagnostic afterTruncation = diagnostics.consumeLogSnapshot(
        truncated, truncated.size(), true);
    require(!afterTruncation.valid
                && diagnostics.state() == RendererStartupState::Ready,
            "truncated log is parsed from the beginning");
}

void testFailureMarker(const QString &logPath)
{
    RendererStartupDiagnostics diagnostics;
    diagnostics.beginAttempt(logPath, 0, {});
    const RendererDiagnostic failed = diagnostics.consumeLogSnapshot(
        QByteArrayLiteral("[FAIL] Program link failed\n"), 27, true);
    require(failed.valid
                && failed.kind == RendererFailureKind::InitializationFailed,
            "FAIL marker produces initialization diagnostic");
    require(failed.details.contains(QStringLiteral("Program link failed")),
            "diagnostic contains renderer failure line");

    diagnostics.beginAttempt(logPath, 0, {});
    const QByteArray mixed = QByteArrayLiteral(
        "[FAIL] first failure\n"
        "[OK] Ready, entering main loop\n"
        "[FAIL] final failure\n");
    const RendererDiagnostic lastFailure = diagnostics.consumeLogSnapshot(
        mixed, mixed.size(), true);
    require(lastFailure.valid
                && lastFailure.kind == RendererFailureKind::InitializationFailed,
            "failure marker has priority over Ready marker");
    require(lastFailure.details.contains(QStringLiteral("final failure")),
            "last failure line is reported");
    require(lastFailure.details.contains(QStringLiteral("[FAIL] first failure"))
                && lastFailure.details.contains(QStringLiteral("日志尾部")),
            "initialization failure includes the current attempt log tail");
}

void testReadyMarkerAcrossSnapshots(const QString &logPath)
{
    RendererStartupDiagnostics diagnostics;
    diagnostics.beginAttempt(logPath, 0, {});

    const QByteArray partialReady = QByteArrayLiteral(
        "Renderer initialized\n[OK] Ready, entering main ");
    const RendererDiagnostic beforeReady = diagnostics.consumeLogSnapshot(
        partialReady, partialReady.size(), false);
    require(!beforeReady.valid
                && diagnostics.state() == RendererStartupState::Starting,
            "partial Ready marker keeps attempt Starting");

    const QByteArray completeReady = partialReady + QByteArrayLiteral("loop\n");
    const RendererDiagnostic afterReady = diagnostics.consumeLogSnapshot(
        completeReady, completeReady.size(), false);
    require(!afterReady.valid
                && diagnostics.state() == RendererStartupState::Ready,
            "Ready marker split across snapshots is detected");
}

void testFailureMarkerAcrossSnapshots(const QString &logPath)
{
    RendererStartupDiagnostics diagnostics;
    diagnostics.beginAttempt(logPath, 0, {});
    const QByteArray partialFailure = QByteArrayLiteral("Renderer initialized\n[FA");
    const RendererDiagnostic beforeFailure = diagnostics.consumeLogSnapshot(
        partialFailure, partialFailure.size(), false);
    require(!beforeFailure.valid
                && diagnostics.state() == RendererStartupState::Starting,
            "partial failure marker keeps attempt Starting");

    const QByteArray completeFailure = partialFailure
                                       + QByteArrayLiteral("IL] Shader compile failed\n");
    const RendererDiagnostic afterFailure = diagnostics.consumeLogSnapshot(
        completeFailure, completeFailure.size(), false);
    require(afterFailure.valid
                && afterFailure.kind == RendererFailureKind::InitializationFailed,
            "failure marker split across snapshots is detected");
    require(afterFailure.details.contains(QStringLiteral("Shader compile failed")),
            "split failure diagnostic contains the complete failure line");
}

void testProcessFailures(const QString &logPath)
{
    RendererStartupDiagnostics diagnostics;
    diagnostics.beginAttempt(logPath, 0, {});
    const RendererDiagnostic failedToStart = diagnostics.processFailedToStart(
        QStringLiteral("系统找不到指定的文件"));
    require(failedToStart.valid
                && failedToStart.kind == RendererFailureKind::FailedToStart,
            "process start error produces FailedToStart");
    require(failedToStart.details.contains(QStringLiteral("系统找不到指定的文件")),
            "process start diagnostic contains system error");

    diagnostics.beginAttempt(logPath, 0, {});
    const QByteArray exitLog = QByteArrayLiteral(
        "Renderer window initialized\nCapture backend selected\n");
    diagnostics.consumeLogSnapshot(exitLog, exitLog.size(), false);
    const RendererDiagnostic exited = diagnostics.processFinished(23, false);
    require(exited.valid
                && exited.kind == RendererFailureKind::ExitedBeforeReady,
            "exit before Ready produces diagnostic");
    require(exited.details.contains(QStringLiteral("23")),
            "early exit diagnostic contains exit code");
    require(exited.details.contains(QStringLiteral("Renderer window initialized"))
                && exited.details.contains(QStringLiteral("日志尾部")),
            "early exit diagnostic includes the current attempt log tail");

    diagnostics.beginAttempt(logPath, 0, {});
    const RendererDiagnostic crashed = diagnostics.processFinished(-1, true);
    require(crashed.valid
                && crashed.kind == RendererFailureKind::ExitedBeforeReady,
            "crash before Ready produces diagnostic");
    require(crashed.details.contains(QStringLiteral("crash"), Qt::CaseInsensitive),
            "early crash diagnostic identifies crash");
}

void testTimeoutAndStopping(const QString &logPath)
{
    RendererStartupDiagnostics diagnostics;
    diagnostics.beginAttempt(logPath, 0, {});
    const QByteArray timeoutLog = QByteArrayLiteral(
        "Creating OpenGL context\nCompiling fragment shader\n");
    diagnostics.consumeLogSnapshot(timeoutLog, timeoutLog.size(), false);
    const RendererDiagnostic timedOut = diagnostics.timeout();
    require(timedOut.valid
                && timedOut.kind == RendererFailureKind::ReadyTimeout,
            "timeout produces ReadyTimeout");
    require(timedOut.details.contains(QStringLiteral("8000")),
            "timeout diagnostic contains 8000 ms threshold");
    require(timedOut.details.contains(QStringLiteral("Compiling fragment shader"))
                && timedOut.details.contains(QStringLiteral("日志尾部")),
            "timeout diagnostic includes the current attempt log tail");

    diagnostics.beginAttempt(logPath, 0, {});
    diagnostics.beginStopping();
    require(diagnostics.state() == RendererStartupState::Stopping,
            "beginStopping enters Stopping");
    const RendererDiagnostic stopped = diagnostics.processFinished(0, false);
    require(!stopped.valid && diagnostics.state() == RendererStartupState::Stopped,
            "exit while Stopping enters Stopped without error");
}

void testDiagnosticDetailsAreCapped(const QString &logPath)
{
    RendererStartupDiagnostics diagnostics;
    diagnostics.beginAttempt(logPath, 0, {});
    const QByteArray longFailure = QByteArrayLiteral("[FAIL] ")
                                   + QByteArray(12000, 'x') + '\n';
    const RendererDiagnostic failed = diagnostics.consumeLogSnapshot(
        longFailure, longFailure.size(), true);
    require(failed.valid, "long renderer failure produces diagnostic");
    require(failed.details.toUtf8().size() <= 8192,
            "diagnostic details are capped to 8192 bytes");
}

void testRollingLogTailIsBounded(const QString &logPath)
{
    RendererStartupDiagnostics diagnostics;
    diagnostics.beginAttempt(logPath, 0, {});
    const QByteArray oldPrefix = QByteArrayLiteral("OLD_PREFIX_MUST_BE_DROPPED\n");
    const QByteArray recentSuffix = QByteArrayLiteral("RECENT_SUFFIX_MUST_REMAIN\n");
    const QByteArray largeLog = oldPrefix + QByteArray(12000, 'x') + '\n'
                                + recentSuffix;
    diagnostics.consumeLogSnapshot(largeLog, largeLog.size(), false);

    const RendererDiagnostic exited = diagnostics.processFinished(9, false);
    require(exited.valid, "large current-attempt log still produces early-exit diagnostic");
    require(exited.details.contains(QStringLiteral("RECENT_SUFFIX_MUST_REMAIN")),
            "rolling log tail retains recent bytes");
    require(!exited.details.contains(QStringLiteral("OLD_PREFIX_MUST_BE_DROPPED")),
            "rolling log tail drops old bytes");
    require(exited.details.toUtf8().size() <= 8192,
            "diagnostic with rolling log tail remains capped to 8192 bytes");
}

void testLogTailDoesNotLeakAcrossAttemptsOrReplacement(const QString &logPath)
{
    RendererStartupDiagnostics diagnostics;
    const QByteArray oldAttempt = QByteArrayLiteral("OLD_ATTEMPT_CONTEXT\n");
    diagnostics.beginAttempt(logPath, 0, {});
    diagnostics.consumeLogSnapshot(oldAttempt, oldAttempt.size(), false);

    const QByteArray newAttempt = QByteArrayLiteral("NEW_ATTEMPT_CONTEXT\n");
    diagnostics.beginAttempt(logPath, oldAttempt.size(), QDateTime::currentDateTime());
    diagnostics.consumeLogSnapshot(oldAttempt + newAttempt,
                                   oldAttempt.size() + newAttempt.size(),
                                   false);
    const RendererDiagnostic newAttemptExit = diagnostics.processFinished(4, false);
    require(newAttemptExit.details.contains(QStringLiteral("NEW_ATTEMPT_CONTEXT")),
            "new attempt diagnostic contains only newly appended context");
    require(!newAttemptExit.details.contains(QStringLiteral("OLD_ATTEMPT_CONTEXT")),
            "old attempt context is not retained");

    diagnostics.beginAttempt(logPath, newAttempt.size(), QDateTime::currentDateTime());
    const QByteArray replacedLog = QByteArrayLiteral("REPLACED_FILE_CONTEXT\n");
    diagnostics.consumeLogSnapshot(replacedLog, replacedLog.size(), true);
    const RendererDiagnostic replacedExit = diagnostics.processFinished(5, false);
    require(replacedExit.details.contains(QStringLiteral("REPLACED_FILE_CONTEXT")),
            "replacement context is retained");
    require(!replacedExit.details.contains(QStringLiteral("NEW_ATTEMPT_CONTEXT")),
            "replacement clears previous file context");
}

void testFailureMarkerAcrossSequentialChunks(const QString &logPath)
{
    RendererStartupDiagnostics diagnostics;
    diagnostics.beginAttempt(logPath, 0, {});

    QByteArray firstChunk(64 * 1024, 'x');
    firstChunk.replace(firstChunk.size() - 3, 3, QByteArrayLiteral("[FA"));
    const RendererDiagnostic beforeFailure = diagnostics.consumeLogChunk(
        firstChunk, 0, false);
    require(!beforeFailure.valid
                && diagnostics.state() == RendererStartupState::Starting,
            "first bounded chunk with partial marker keeps attempt Starting");
    require(diagnostics.logOffset() == firstChunk.size(),
            "first bounded chunk advances the absolute log offset");

    const QByteArray secondChunk = QByteArrayLiteral(
        "IL] failure after first bounded chunk\n");
    const RendererDiagnostic afterFailure = diagnostics.consumeLogChunk(
        secondChunk, firstChunk.size(), false);
    require(afterFailure.valid
                && afterFailure.kind == RendererFailureKind::InitializationFailed,
            "FAIL marker split across sequential bounded chunks is detected");
    require(afterFailure.details.contains(
                QStringLiteral("failure after first bounded chunk")),
            "cross-chunk failure retains the complete failure line");

    diagnostics.beginAttempt(logPath, 0, {});
    QByteArray readyFirstChunk(64 * 1024, 'y');
    const QByteArray partialReady = QByteArrayLiteral(
        "[OK] Ready, entering main ");
    readyFirstChunk.replace(readyFirstChunk.size() - partialReady.size(),
                            partialReady.size(),
                            partialReady);
    diagnostics.consumeLogChunk(readyFirstChunk, 0, false);
    const QByteArray readySecondChunk = QByteArrayLiteral("loop\n");
    const RendererDiagnostic ready = diagnostics.consumeLogChunk(
        readySecondChunk, readyFirstChunk.size(), false);
    require(!ready.valid && diagnostics.state() == RendererStartupState::Ready,
            "Ready marker split across sequential bounded chunks is detected");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    require(temporaryDirectory.isValid(), "temporary directory is valid");

    const QString rootPath = temporaryDirectory.path();
    const QString logPath = QDir(rootPath).filePath(QStringLiteral("blackhole_debug.txt"));

    testAttemptAndMissingFiles(rootPath, logPath);
    testReadyAndLogBaselines(logPath);
    testFailureMarker(logPath);
    testFailureMarkerAcrossSnapshots(logPath);
    testReadyMarkerAcrossSnapshots(logPath);
    testProcessFailures(logPath);
    testTimeoutAndStopping(logPath);
    testDiagnosticDetailsAreCapped(logPath);
    testRollingLogTailIsBounded(logPath);
    testLogTailDoesNotLeakAcrossAttemptsOrReplacement(logPath);
    testFailureMarkerAcrossSequentialChunks(logPath);

    std::cout << "RENDERER_STARTUP_DIAGNOSTICS_TESTS_OK\n";
    return EXIT_SUCCESS;
}
