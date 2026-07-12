#include "update_checker.h"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

namespace {
constexpr auto kEndpoint = "https://api.github.com/repos/XboxNahida/ghostty-blackhole-main/releases/latest";
constexpr auto kUserAgent = "Blakhole-UI-UpdateChecker/1.0";
constexpr int kTimeoutMs = 8000;
constexpr qsizetype kMaxResponseBytes = 1024 * 1024;
constexpr auto kIgnoredKey = "ignoredVersion";
constexpr auto kVisitedKey = "visitedVersion";
QSettings Settings() { return QSettings(QStringLiteral("XboxNahida"), QStringLiteral("Blakhole UI")); }
}

UpdateChecker::UpdateChecker(const QString &currentVersion, QObject *parent)
    : QObject(parent), m_currentVersion(currentVersion), m_currentParsed(ParseReleaseVersion(currentVersion))
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        if (m_reply) m_reply->abort();
    });
    setStatus(QStringLiteral("尚未检查"));
}

void UpdateChecker::checkAutomatically() { startRequest(false); }
void UpdateChecker::checkManually() { startRequest(true); }

void UpdateChecker::startRequest(bool manual)
{
    if (m_checking) return;
    m_manualRequest = manual;
    setChecking(true);
    setStatus(QStringLiteral("正在检查更新…"));

    QNetworkRequest request(QUrl(QString::fromLatin1(kEndpoint)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::onReplyFinished);
    m_timeout.start(kTimeoutMs);
}

void UpdateChecker::onReplyFinished()
{
    m_timeout.stop();
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    const bool manual = m_manualRequest;
    setChecking(false);
    if (!reply) return;

    const QByteArray payload = reply->readAll();
    const QUrl finalUrl = reply->url();
    QString error;
    UpdateReleaseInfo release;
    if (reply->error() != QNetworkReply::NoError || payload.size() > kMaxResponseBytes
        || finalUrl.scheme() != QStringLiteral("https")
        || finalUrl.host().compare(QStringLiteral("api.github.com"), Qt::CaseInsensitive) != 0) {
        error = QStringLiteral("网络请求失败");
    } else {
        release = ParseGitHubRelease(payload, &error);
        if (release.valid) {
            const QJsonDocument document = QJsonDocument::fromJson(payload);
            const QJsonObject object = document.object();
            const QJsonValue draft = object.value(QStringLiteral("draft"));
            const QJsonValue prerelease = object.value(QStringLiteral("prerelease"));
            if ((draft.isBool() && draft.toBool()) || (prerelease.isBool() && prerelease.toBool())) {
                release = {};
                error = QStringLiteral("仅支持正式 Release");
            }
        }
    }
    reply->deleteLater();
    if (!release.valid) {
        setStatus(error.isEmpty() ? QStringLiteral("检查失败") : error);
        if (manual) emit manualResultReady();
        return;
    }
    applyRelease(release, manual);
    if (manual) emit manualResultReady();
}

void UpdateChecker::applyReleaseForTesting(const UpdateReleaseInfo &release, bool manual)
{
    applyRelease(release, manual);
}

void UpdateChecker::applyRelease(const UpdateReleaseInfo &release, bool manual)
{
    if (!release.valid || !m_currentParsed.valid || !release.version.valid) {
        setUpdateAvailable(false);
        setStatus(QStringLiteral("检查失败"));
        return;
    }
    QSettings settings = Settings();
    const int comparison = CompareReleaseVersions(release.version, m_currentParsed);
    if (comparison <= 0) {
        settings.remove(kIgnoredKey);
        settings.remove(kVisitedKey);
        setUpdateAvailable(false);
        setStatus(QStringLiteral("当前已是最新版本"));
        return;
    }
    if (manual) clearSuppression();

    if (m_latestVersion != release.tagName) {
        m_latestVersion = release.tagName;
        emit latestVersionChanged();
    }
    if (m_latestName != release.name) {
        m_latestName = release.name;
        emit latestNameChanged();
    }
    if (m_latestNotes != release.notes) {
        m_latestNotes = release.notes;
        emit latestNotesChanged();
    }
    const QString ignored = settings.value(kIgnoredKey).toString();
    setUpdateAvailable(ignored != release.tagName);
    setStatus(m_updateAvailable ? QStringLiteral("发现 %1").arg(release.tagName)
                                : QStringLiteral("已忽略 %1").arg(release.tagName));
}

void UpdateChecker::openDownloadPage()
{
    if (!m_updateAvailable || !m_latestVersion.startsWith(QLatin1Char('v'))) return;
    const QUrl url(QStringLiteral("https://github.com/XboxNahida/ghostty-blackhole-main/releases/tag/") + m_latestVersion);
    if (QDesktopServices::openUrl(url)) {
        Settings().setValue(kVisitedKey, m_latestVersion);
    } else {
        setStatus(QStringLiteral("无法打开下载页面"));
    }
}

void UpdateChecker::openDownloadPageForTesting()
{
    Settings().setValue(kVisitedKey, m_latestVersion);
}

void UpdateChecker::ignoreCurrentRelease()
{
    if (m_latestVersion.isEmpty()) return;
    Settings().setValue(kIgnoredKey, m_latestVersion);
    setUpdateAvailable(false);
    setStatus(QStringLiteral("已忽略 %1").arg(m_latestVersion));
}

void UpdateChecker::clearSuppression()
{
    QSettings settings = Settings();
    settings.remove(kIgnoredKey);
    settings.remove(kVisitedKey);
}

void UpdateChecker::clearStateForTesting()
{
    QSettings settings = Settings();
    settings.remove(kIgnoredKey);
    settings.remove(kVisitedKey);
    setUpdateAvailable(false);
}

void UpdateChecker::setChecking(bool value) { if (m_checking != value) { m_checking = value; emit checkingChanged(); } }
void UpdateChecker::setUpdateAvailable(bool value) { if (m_updateAvailable != value) { m_updateAvailable = value; emit updateAvailableChanged(); } }
void UpdateChecker::setStatus(const QString &value) { if (m_statusText != value) { m_statusText = value; emit statusTextChanged(); } }
