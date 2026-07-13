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
QSettings Settings() { return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("XboxNahida"), QStringLiteral("Blakhole UI")); }

void SetError(QString *error, const QString &message)
{
    if (error) *error = message;
}
}

bool BoundedPayloadAccumulator::append(const QByteArray &chunk)
{
    if (m_overflowed) return false;
    if (chunk.size() > m_maxBytes - m_payload.size()) {
        m_overflowed = true;
        m_payload.clear();
        return false;
    }
    m_payload.append(chunk);
    return true;
}

void BoundedPayloadAccumulator::reset()
{
    m_payload.clear();
    m_overflowed = false;
}

UpdateReleaseInfo ParseOfficialGitHubRelease(const QByteArray &json, QString *error)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject()) {
        SetError(error, QStringLiteral("Release 响应不是有效的 JSON 对象"));
        return {};
    }
    const QJsonObject object = document.object();
    const QJsonValue draft = object.value(QStringLiteral("draft"));
    const QJsonValue prerelease = object.value(QStringLiteral("prerelease"));
    if (!draft.isBool() || !prerelease.isBool() || draft.toBool() || prerelease.toBool()) {
        SetError(error, QStringLiteral("仅支持正式 Release"));
        return {};
    }
    return ParseGitHubRelease(json, error);
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
    if (m_checking) {
        if (manual) m_manualRequest = true;
        return;
    }
    m_manualRequest = manual;
    m_payload.reset();
    setChecking(true);
    setStatus(QStringLiteral("正在检查更新…"));

    QNetworkRequest request(QUrl(QString::fromLatin1(kEndpoint)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &UpdateChecker::onReplyReadyRead);
    connect(m_reply, &QNetworkReply::metaDataChanged, this,
            &UpdateChecker::onReplyMetadataChanged);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::onReplyFinished);
    m_timeout.start(kTimeoutMs);
}

void UpdateChecker::onReplyReadyRead()
{
    if (!m_reply) return;
    const qsizetype remaining = m_payload.remainingCapacity();
    if (remaining <= 0) {
        m_payload.append(QByteArrayLiteral("x"));
        m_reply->abort();
        return;
    }
    const QByteArray chunk = m_reply->read(remaining + 1);
    if (!m_payload.append(chunk)) m_reply->abort();
}

void UpdateChecker::onReplyMetadataChanged()
{
    if (!m_reply) return;
    bool ok = false;
    const qlonglong contentLength = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(&ok);
    if (ok && contentLength > kMaxResponseBytes) m_reply->abort();
}

void UpdateChecker::onReplyFinished()
{
    m_timeout.stop();
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    const bool manual = m_manualRequest;
    m_manualRequest = false;
    setChecking(false);
    if (!reply) return;

    const QByteArray payload = m_payload.payload();
    const QUrl finalUrl = reply->url();
    QString error;
    UpdateReleaseInfo release;
    if (m_payload.overflowed()) {
        error = QStringLiteral("Release 响应超过大小限制");
    } else if (reply->error() != QNetworkReply::NoError
        || finalUrl.scheme() != QStringLiteral("https")
        || finalUrl.host().compare(QStringLiteral("api.github.com"), Qt::CaseInsensitive) != 0) {
        error = QStringLiteral("网络请求失败");
    } else {
        release = ParseOfficialGitHubRelease(payload, &error);
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

void UpdateChecker::beginRequestForTesting(bool manual)
{
    m_manualRequest = manual;
    setChecking(true);
}

void UpdateChecker::finishRequestForTesting()
{
    const bool manual = m_manualRequest;
    m_manualRequest = false;
    setChecking(false);
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
    settings.sync();
    const QString ignored = settings.value(kIgnoredKey).toString();
    const QString visited = settings.value(kVisitedKey).toString();
    if (!ignored.isEmpty() && ignored != release.tagName) settings.remove(kIgnoredKey);
    if (!visited.isEmpty() && visited != release.tagName) settings.remove(kVisitedKey);

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
    m_latestUrl = release.htmlUrl;
    setUpdateAvailable(settings.value(kIgnoredKey).toString() != release.tagName);
    setStatus(m_updateAvailable ? QStringLiteral("发现 %1").arg(release.tagName)
                                : QStringLiteral("已忽略 %1").arg(release.tagName));
}

void UpdateChecker::openDownloadPage()
{
    if (!m_updateAvailable || !m_latestUrl.isValid()) return;
    if (QDesktopServices::openUrl(m_latestUrl)) {
        Settings().setValue(kVisitedKey, m_latestVersion);
    } else {
        setStatus(QStringLiteral("无法打开下载页面"));
    }
}

bool UpdateChecker::openDownloadPageForTesting()
{
    if (!m_updateAvailable || !m_latestUrl.isValid()
        || m_latestUrl.scheme() != QStringLiteral("https")
        || m_latestUrl.host().compare(QStringLiteral("github.com"), Qt::CaseInsensitive) != 0) {
        return false;
    }
    {
        QSettings settings = Settings();
        settings.setValue(kVisitedKey, m_latestVersion);
        settings.sync();
    }
    return true;
}

void UpdateChecker::ignoreCurrentRelease()
{
    if (m_latestVersion.isEmpty()) return;
    QSettings settings = Settings();
    settings.setValue(kIgnoredKey, m_latestVersion);
    settings.sync();
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
    settings.sync();
    setUpdateAvailable(false);
}

void UpdateChecker::setChecking(bool value) { if (m_checking != value) { m_checking = value; emit checkingChanged(); } }
void UpdateChecker::setUpdateAvailable(bool value) { if (m_updateAvailable != value) { m_updateAvailable = value; emit updateAvailableChanged(); } }
void UpdateChecker::setStatus(const QString &value) { if (m_statusText != value) { m_statusText = value; emit statusTextChanged(); } }
