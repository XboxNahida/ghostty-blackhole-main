#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QtGlobal>

enum class RendererStartupState {
    Stopped,
    Starting,
    Ready,
    Stopping,
    Failed
};

enum class RendererFailureKind {
    None,
    MissingFile,
    FailedToStart,
    ExitedBeforeReady,
    InitializationFailed,
    ReadyTimeout
};

struct RendererDiagnostic {
    bool valid = false;
    quint64 attemptId = 0;
    RendererFailureKind kind = RendererFailureKind::None;
    QString title;
    QString summary;
    QString details;
    QString logPath;
};

class RendererStartupDiagnostics {
public:
    quint64 beginAttempt(const QString &logPath,
                         qint64 previousLogSize,
                         const QDateTime &previousLogModified);
    RendererDiagnostic validateRequiredFiles(
        const QString &exePath,
        const QString &workingDirectory,
        const QStringList &relativeRequiredFiles);
    RendererDiagnostic consumeLogSnapshot(const QByteArray &content,
                                          qint64 fileSize,
                                          bool fileReplacedOrTruncated);
    RendererDiagnostic processFailedToStart(const QString &processError);
    RendererDiagnostic processFinished(int exitCode, bool crashed);
    RendererDiagnostic timeout();
    void beginStopping();
    RendererStartupState state() const;
    quint64 attemptId() const;

private:
    RendererDiagnostic fail(RendererFailureKind kind,
                            const QString &summary,
                            const QString &technicalDetails);

    RendererStartupState m_state = RendererStartupState::Stopped;
    quint64 m_attemptId = 0;
    bool m_reportedCurrentAttempt = false;
    QString m_logPath;
    qint64 m_logOffset = 0;
    QDateTime m_previousLogModified;
};
