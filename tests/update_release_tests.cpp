#include "update_release.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <cstdlib>

namespace {

void Require(bool condition, const char *message)
{
    if (condition) return;
    QTextStream(stderr) << "UPDATE_RELEASE_TEST_FAILED: " << message << Qt::endl;
    std::exit(1);
}

QByteArray ReleaseJson(const QString &tag,
                       const QString &name,
                       const QString &notes,
                       const QString &url)
{
    QJsonObject object;
    object.insert(QStringLiteral("tag_name"), tag);
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("body"), notes);
    object.insert(QStringLiteral("html_url"), url);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

void RequireReleaseFailure(const QByteArray &json, const char *message)
{
    QString error;
    const UpdateReleaseInfo info = ParseGitHubRelease(json, &error);
    Require(!info.valid, message);
    Require(!error.isEmpty(), "解析失败时必须返回错误信息");
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const ParsedVersion v1100 = ParseReleaseVersion(QStringLiteral("v1.10.0"));
    const ParsedVersion v199 = ParseReleaseVersion(QStringLiteral("v1.9.9"));
    Require(v1100.valid && v1100.major == 1 && v1100.minor == 10 && v1100.patch == 0,
            "无法解析带 v 前缀的三段版本号");
    Require(v199.valid, "无法解析不带 v 前缀的三段版本号");
    Require(CompareReleaseVersions(v1100, v199) > 0,
            "版本比较错误地使用了字典序");
    Require(CompareReleaseVersions(v1100, ParseReleaseVersion(QStringLiteral("1.10.0"))) == 0,
            "相同数值版本应当相等");
    Require(CompareReleaseVersions(v199, v1100) < 0,
            "较低版本应比较为小于");

    const QStringList invalidLabels = {
        QString(),
        QStringLiteral("1.2"),
        QStringLiteral("v1.2"),
        QStringLiteral("1.2.3.4"),
        QStringLiteral("V1.2.3"),
        QStringLiteral("v1.2.3-beta"),
        QStringLiteral(" v1.2.3"),
        QStringLiteral("v1.2.3 "),
        QStringLiteral("v-1.2.3"),
        QStringLiteral("v4294967296.0.0")
    };
    for (const QString &label : invalidLabels) {
        Require(!ParseReleaseVersion(label).valid,
                "非法或不完整的版本标签被接受");
    }

    QString error = QStringLiteral("old error");
    const UpdateReleaseInfo valid = ParseGitHubRelease(
        ReleaseJson(QStringLiteral("v1.2.3"),
                    QStringLiteral("正式版本"),
                    QStringLiteral("修复若干问题"),
                    QStringLiteral("https://github.com/XboxNahida/ghostty-blackhole-main/releases/tag/v1.2.3")),
        &error);
    Require(valid.valid, "合法 GitHub Release JSON 被拒绝");
    Require(error.isEmpty(), "解析成功后未清空旧错误");
    Require(valid.tagName == QStringLiteral("v1.2.3"), "tag_name 未保留");
    Require(valid.version.valid && valid.version.major == 1 && valid.version.minor == 2
                && valid.version.patch == 3,
            "Release 版本未解析");
    Require(valid.name == QStringLiteral("正式版本"), "name 未解析");
    Require(valid.notes == QStringLiteral("修复若干问题"), "body 未解析");
    Require(valid.htmlUrl.toString() == QStringLiteral(
                "https://github.com/XboxNahida/ghostty-blackhole-main/releases/tag/v1.2.3"),
            "html_url 未解析");

    RequireReleaseFailure(QByteArrayLiteral("not json"), "非法 JSON 被接受");
    RequireReleaseFailure(QByteArrayLiteral("[]"), "非对象 JSON 被接受");
    RequireReleaseFailure(QByteArrayLiteral("{}"), "缺少正式字段的 JSON 被接受");
    QString oversizedJsonError;
    Require(!ParseGitHubRelease(QByteArray(kUpdateReleaseMaxJsonBytes + 1, ' '),
                                &oversizedJsonError).valid,
            "超过响应上限的 JSON 被接受");
    Require(oversizedJsonError.contains(QStringLiteral("超过")),
            "超过响应上限时未返回明确错误");
    RequireReleaseFailure(QByteArrayLiteral(
        "{\"tag_name\":1,\"name\":\"x\",\"body\":\"x\",\"html_url\":\"https://github.com/a/b/releases/tag/v1.0.0\"}"),
        "类型错误的正式字段被接受");
    RequireReleaseFailure(
        ReleaseJson(QStringLiteral("v1.2"), QStringLiteral("x"), QStringLiteral("x"),
                    QStringLiteral("https://github.com/a/b/releases/tag/v1.2")),
        "非法 tag_name 被接受");
    RequireReleaseFailure(
        ReleaseJson(QStringLiteral("v1.2.3"), QStringLiteral("x"), QStringLiteral("x"), QString()),
        "空 html_url 被接受");
    RequireReleaseFailure(
        ReleaseJson(QStringLiteral("v1.2.3"), QStringLiteral("x"), QStringLiteral("x"),
                    QStringLiteral("http://github.com/a/b/releases/tag/v1.2.3")),
        "非 HTTPS URL 被接受");
    RequireReleaseFailure(
        ReleaseJson(QStringLiteral("v1.2.3"), QStringLiteral("x"), QStringLiteral("x"),
                    QStringLiteral("https://github.com.evil.example/a/b/releases/tag/v1.2.3")),
        "伪造 GitHub 主机 URL 被接受");
    RequireReleaseFailure(
        ReleaseJson(QStringLiteral("v1.2.3"), QStringLiteral("x"), QStringLiteral("x"),
                    QStringLiteral("https://user@github.com/a/b/releases/tag/v1.2.3")),
        "包含用户信息的 URL 被接受");
    RequireReleaseFailure(
        ReleaseJson(QStringLiteral("v1.2.3"), QStringLiteral("x"), QStringLiteral("x"),
                    QStringLiteral("https://github.com/a/b/issues/1")),
        "非 Release 页面 URL 被接受");

    const QString oversizedName(kUpdateReleaseMaxNameLength + 20, QLatin1Char('n'));
    const QString oversizedNotes(kUpdateReleaseMaxNotesLength + 20, QLatin1Char('b'));
    const UpdateReleaseInfo bounded = ParseGitHubRelease(
        ReleaseJson(QStringLiteral("v2.0.0"), oversizedName, oversizedNotes,
                    QStringLiteral("https://github.com/a/b/releases/tag/v2.0.0")),
        &error);
    Require(bounded.valid, "可截断的超长文本导致 Release 被拒绝");
    Require(bounded.name.size() == kUpdateReleaseMaxNameLength, "name 未按上限截断");
    Require(bounded.notes.size() == kUpdateReleaseMaxNotesLength, "body 未按上限截断");
    Require(bounded.notes == oversizedNotes.left(kUpdateReleaseMaxNotesLength),
            "超长说明未保留正确前缀");

    const QString oversizedTag(kUpdateReleaseMaxTagLength + 1, QLatin1Char('1'));
    RequireReleaseFailure(
        ReleaseJson(oversizedTag, QStringLiteral("x"), QStringLiteral("x"),
                    QStringLiteral("https://github.com/a/b/releases/tag/v1.0.0")),
        "超长 tag_name 被接受");
    const QString oversizedUrl = QStringLiteral("https://github.com/a/b/releases/tag/")
        + QString(kUpdateReleaseMaxUrlLength, QLatin1Char('x'));
    RequireReleaseFailure(
        ReleaseJson(QStringLiteral("v1.2.3"), QStringLiteral("x"), QStringLiteral("x"), oversizedUrl),
        "超长 html_url 被接受");

    QTextStream(stdout) << "UPDATE_RELEASE_TESTS_OK" << Qt::endl;
    return 0;
}
