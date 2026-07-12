#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "update_release.h"

class QNetworkReply;

class UpdateChecker final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
    Q_PROPERTY(QString latestName READ latestName NOTIFY latestNameChanged)
    Q_PROPERTY(QString latestNotes READ latestNotes NOTIFY latestNotesChanged)

public:
    explicit UpdateChecker(const QString &currentVersion, QObject *parent = nullptr);

    bool checking() const { return m_checking; }
    bool updateAvailable() const { return m_updateAvailable; }
    QString statusText() const { return m_statusText; }
    QString latestVersion() const { return m_latestVersion; }
    QString latestName() const { return m_latestName; }
    QString latestNotes() const { return m_latestNotes; }

    Q_INVOKABLE void checkAutomatically();
    Q_INVOKABLE void checkManually();
    Q_INVOKABLE void openDownloadPage();
    Q_INVOKABLE void ignoreCurrentRelease();

    // 纯状态注入入口，供不依赖实时网络的单元测试使用。
    void applyReleaseForTesting(const UpdateReleaseInfo &release, bool manual);
    void clearStateForTesting();
    void openDownloadPageForTesting();

signals:
    void checkingChanged();
    void updateAvailableChanged();
    void statusTextChanged();
    void latestVersionChanged();
    void latestNameChanged();
    void latestNotesChanged();
    void manualResultReady();

private slots:
    void onReplyFinished();

private:
    void startRequest(bool manual);
    void applyRelease(const UpdateReleaseInfo &release, bool manual);
    void setChecking(bool value);
    void setStatus(const QString &value);
    void setUpdateAvailable(bool value);
    void clearSuppression();

    QString m_currentVersion;
    ParsedVersion m_currentParsed;
    bool m_checking = false;
    bool m_updateAvailable = false;
    bool m_manualRequest = false;
    QString m_statusText;
    QString m_latestVersion;
    QString m_latestName;
    QString m_latestNotes;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_timeout;
};
