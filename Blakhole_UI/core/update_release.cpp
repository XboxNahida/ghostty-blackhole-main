#include "update_release.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace {

void SetError(QString *error, const QString &message)
{
    if (error) *error = message;
}

bool HasStringField(const QJsonObject &object, const QString &name)
{
    return object.contains(name) && object.value(name).isString();
}

bool ReadOptionalStringField(const QJsonObject &object, const QString &name, QString *value)
{
    const QJsonValue field = object.value(name);
    if (field.isUndefined() || field.isNull()) {
        value->clear();
        return true;
    }
    if (!field.isString()) return false;
    *value = field.toString();
    return true;
}

QString TruncateUtf16Safe(const QString &text, qsizetype maxLength)
{
    if (text.size() <= maxLength) return text;

    qsizetype length = maxLength;
    if (length > 0 && text.at(length - 1).isHighSurrogate()
        && text.at(length).isLowSurrogate()) {
        --length;
    }
    return text.left(length);
}

bool IsSafeGitHubReleaseUrl(const QUrl &url, const QString &urlText, const QString &tagName)
{
    if (!url.isValid() || url.scheme() != QStringLiteral("https")
        || url.host().compare(QStringLiteral("github.com"), Qt::CaseInsensitive) != 0
        || !url.userInfo().isEmpty() || (url.port() != -1 && url.port() != 443)
        || !url.query().isEmpty() || !url.fragment().isEmpty()) {
        return false;
    }

    const qsizetype schemeSeparator = urlText.indexOf(QStringLiteral("://"));
    const qsizetype pathStart = urlText.indexOf(QLatin1Char('/'), schemeSeparator + 3);
    if (schemeSeparator < 0 || pathStart < 0) return false;

    static const QRegularExpression releasePath(
        QStringLiteral("^/([^/]+)/([^/]+)/releases/tag/([^/]+)$"));
    const QRegularExpressionMatch match = releasePath.match(urlText.mid(pathStart));
    return match.hasMatch()
        && match.captured(1).compare(QStringLiteral("XboxNahida"), Qt::CaseInsensitive) == 0
        && match.captured(2).compare(QStringLiteral("ghostty-blackhole-main"),
                                     Qt::CaseInsensitive) == 0
        && match.captured(3) == tagName;
}

}

ParsedVersion ParseReleaseVersion(const QString &label)
{
    if (label.isEmpty() || label.size() > kUpdateReleaseMaxTagLength) return {};

    static const QRegularExpression versionPattern(
        QStringLiteral("^v?(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"));
    const QRegularExpressionMatch match = versionPattern.match(label);
    if (!match.hasMatch()) return {};

    bool majorOk = false;
    bool minorOk = false;
    bool patchOk = false;
    const quint32 major = match.captured(1).toUInt(&majorOk);
    const quint32 minor = match.captured(2).toUInt(&minorOk);
    const quint32 patch = match.captured(3).toUInt(&patchOk);
    if (!majorOk || !minorOk || !patchOk) return {};

    ParsedVersion result;
    result.valid = true;
    result.major = major;
    result.minor = minor;
    result.patch = patch;
    return result;
}

int CompareReleaseVersions(const ParsedVersion &left, const ParsedVersion &right)
{
    Q_ASSERT_X(left.valid && right.valid, "CompareReleaseVersions",
               "版本比较仅接受 valid=true 的输入");
    if (!left.valid || !right.valid) return 0;
    if (left.major != right.major) return left.major < right.major ? -1 : 1;
    if (left.minor != right.minor) return left.minor < right.minor ? -1 : 1;
    if (left.patch != right.patch) return left.patch < right.patch ? -1 : 1;
    return 0;
}

UpdateReleaseInfo ParseGitHubRelease(const QByteArray &json, QString *error)
{
    if (json.size() > kUpdateReleaseMaxJsonBytes) {
        SetError(error, QStringLiteral("Release 响应超过大小限制"));
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        SetError(error, QStringLiteral("Release 响应不是有效的 JSON 对象"));
        return {};
    }

    const QJsonObject object = document.object();
    const QString tagField = QStringLiteral("tag_name");
    const QString nameField = QStringLiteral("name");
    const QString notesField = QStringLiteral("body");
    const QString urlField = QStringLiteral("html_url");
    if (!HasStringField(object, tagField) || !HasStringField(object, urlField)) {
        SetError(error, QStringLiteral("Release 响应缺少有效的正式字段"));
        return {};
    }

    QString name;
    QString notes;
    if (!ReadOptionalStringField(object, nameField, &name)
        || !ReadOptionalStringField(object, notesField, &notes)) {
        SetError(error, QStringLiteral("Release 可选文本字段类型无效"));
        return {};
    }

    const QString tagName = object.value(tagField).toString();
    const QString htmlUrlText = object.value(urlField).toString();
    if (tagName.isEmpty() || tagName.size() > kUpdateReleaseMaxTagLength) {
        SetError(error, QStringLiteral("Release 版本标签无效"));
        return {};
    }
    if (htmlUrlText.isEmpty() || htmlUrlText.size() > kUpdateReleaseMaxUrlLength) {
        SetError(error, QStringLiteral("Release 页面地址无效"));
        return {};
    }

    const ParsedVersion version = ParseReleaseVersion(tagName);
    if (!version.valid) {
        SetError(error, QStringLiteral("Release 版本标签格式无效"));
        return {};
    }

    const QUrl htmlUrl = QUrl::fromEncoded(htmlUrlText.toUtf8(), QUrl::StrictMode);
    if (!IsSafeGitHubReleaseUrl(htmlUrl, htmlUrlText, tagName)) {
        SetError(error, QStringLiteral("Release 页面必须是安全的 GitHub Release 地址"));
        return {};
    }

    UpdateReleaseInfo result;
    result.valid = true;
    result.tagName = tagName;
    result.version = version;
    result.name = TruncateUtf16Safe(name, kUpdateReleaseMaxNameLength);
    result.notes = TruncateUtf16Safe(notes, kUpdateReleaseMaxNotesLength);
    result.htmlUrl = htmlUrl;
    if (error) error->clear();
    return result;
}
