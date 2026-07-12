#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>

#include <cstdlib>

#include "update_checker.h"
#include "update_release.h"

namespace {

void Require(bool condition, const char *message)
{
    if (!condition) {
        qCritical("FAIL: %s", message);
        std::exit(1);
    }
}

UpdateReleaseInfo Release(const QString &tag)
{
    UpdateReleaseInfo info;
    info.valid = true;
    info.tagName = tag;
    info.version = ParseReleaseVersion(tag);
    info.name = QStringLiteral("Release ") + tag;
    info.notes = QStringLiteral("notes");
    info.htmlUrl = QUrl(QStringLiteral("https://github.com/XboxNahida/ghostty-blackhole-main/releases/tag/") + tag);
    return info;
}

QSettings Settings()
{
    return QSettings(QStringLiteral("XboxNahida"), QStringLiteral("Blakhole UI"));
}

QByteArray ReleaseJson(const QByteArray &draft, const QByteArray &prerelease)
{
    return QByteArrayLiteral("{\"tag_name\":\"v1.2.0\",\"name\":\"Release\",\"body\":\"notes\",")
        + QByteArrayLiteral("\"html_url\":\"https://github.com/XboxNahida/ghostty-blackhole-main/releases/tag/v1.2.0\",")
        + QByteArrayLiteral("\"draft\":") + draft + QByteArrayLiteral(",\"prerelease\":")
        + prerelease + QByteArrayLiteral("}");
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir settingsDir;
    Require(settingsDir.isValid(), "temporary settings directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

    BoundedPayloadAccumulator payload(8);
    Require(payload.append(QByteArrayLiteral("1234")), "bounded payload accepts first chunk");
    Require(payload.append(QByteArrayLiteral("5678")), "bounded payload accepts exact limit");
    Require(!payload.append(QByteArrayLiteral("9")), "bounded payload rejects overflow immediately");
    Require(payload.overflowed(), "bounded payload records overflow");

    QString parseError;
    Require(ParseOfficialGitHubRelease(ReleaseJson("false", "false"), &parseError).valid,
            "official release requires boolean false flags");
    Require(!ParseOfficialGitHubRelease(ReleaseJson("true", "false"), &parseError).valid,
            "draft release is rejected");
    Require(!ParseOfficialGitHubRelease(ReleaseJson("false", "true"), &parseError).valid,
            "prerelease is rejected");
    Require(!ParseOfficialGitHubRelease(ReleaseJson("\"false\"", "false"), &parseError).valid,
            "non-boolean release flag is rejected");
    QByteArray missingFlags = ReleaseJson("false", "false");
    missingFlags.replace(QByteArrayLiteral(",\"draft\":false,\"prerelease\":false"), QByteArray());
    Require(!ParseOfficialGitHubRelease(missingFlags, &parseError).valid,
            "missing official release flags are rejected");

    UpdateChecker checker(QStringLiteral("1.0.0"));
    checker.clearStateForTesting();

    checker.applyReleaseForTesting(Release(QStringLiteral("v1.2.0")), false);
    Require(checker.updateAvailable(), "automatic newer release shows red dot");

    checker.ignoreCurrentRelease();
    checker.applyReleaseForTesting(Release(QStringLiteral("v1.2.0")), false);
    Require(!checker.updateAvailable(), "ignored release hides red dot");

    checker.clearStateForTesting();
    checker.applyReleaseForTesting(Release(QStringLiteral("v1.2.0")), false);
    checker.openDownloadPageForTesting();
    checker.applyReleaseForTesting(Release(QStringLiteral("v1.2.0")), false);
    Require(checker.updateAvailable(), "visited release keeps red dot");

    checker.ignoreCurrentRelease();
    Require(Settings().value(QStringLiteral("ignoredVersion")).toString() == QStringLiteral("v1.2.0"),
            "manual suppression setup stores ignored release");
    checker.applyReleaseForTesting(Release(QStringLiteral("v1.2.0")), true);
    Require(checker.updateAvailable(), "manual check clears suppression and shows red dot");
    Require(!Settings().contains(QStringLiteral("ignoredVersion")),
            "manual check removes ignored release key");

    checker.ignoreCurrentRelease();
    Settings().setValue(QStringLiteral("visitedVersion"), QStringLiteral("v1.2.0"));
    Require(Settings().contains(QStringLiteral("ignoredVersion"))
                && Settings().contains(QStringLiteral("visitedVersion")),
            "higher release setup stores both stale suppression keys");
    checker.applyReleaseForTesting(Release(QStringLiteral("v1.3.0")), false);
    Require(checker.updateAvailable(), "higher release resets old suppression");
    Require(!Settings().contains(QStringLiteral("ignoredVersion")),
            "higher release removes stale ignored key");
    Require(!Settings().contains(QStringLiteral("visitedVersion")),
            "higher release removes stale visited key");

    checker.clearStateForTesting();
    checker.applyReleaseForTesting(Release(QStringLiteral("1.4.0")), false);
    Require(checker.openDownloadPageForTesting(), "download supports valid release tag without v prefix");
    Require(Settings().value(QStringLiteral("visitedVersion")).toString() == QStringLiteral("1.4.0"),
            "download records exact version without v prefix");

    int manualResults = 0;
    QObject::connect(&checker, &UpdateChecker::manualResultReady, [&manualResults]() { ++manualResults; });
    checker.beginRequestForTesting(false);
    checker.checkManually();
    Require(checker.requestIsManualForTesting(), "manual request upgrades in-flight automatic request");
    checker.finishRequestForTesting();
    Require(manualResults == 1, "upgraded request emits manual result on completion");

    checker.applyReleaseForTesting(Release(QStringLiteral("v1.0.0")), false);
    Require(!checker.updateAvailable(), "current version clears stale state");

    return 0;
}
