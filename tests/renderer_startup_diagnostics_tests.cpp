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
    const RendererDiagnostic exited = diagnostics.processFinished(23, false);
    require(exited.valid
                && exited.kind == RendererFailureKind::ExitedBeforeReady,
            "exit before Ready produces diagnostic");
    require(exited.details.contains(QStringLiteral("23")),
            "early exit diagnostic contains exit code");

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
    const RendererDiagnostic timedOut = diagnostics.timeout();
    require(timedOut.valid
                && timedOut.kind == RendererFailureKind::ReadyTimeout,
            "timeout produces ReadyTimeout");
    require(timedOut.details.contains(QStringLiteral("8000")),
            "timeout diagnostic contains 8000 ms threshold");

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
    testProcessFailures(logPath);
    testTimeoutAndStopping(logPath);
    testDiagnosticDetailsAreCapped(logPath);

    std::cout << "RENDERER_STARTUP_DIAGNOSTICS_TESTS_OK\n";
    return EXIT_SUCCESS;
}
