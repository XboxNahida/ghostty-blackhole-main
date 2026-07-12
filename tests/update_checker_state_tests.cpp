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

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir settingsDir;
    Require(settingsDir.isValid(), "temporary settings directory");
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

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

    checker.applyReleaseForTesting(Release(QStringLiteral("v1.2.0")), true);
    Require(checker.updateAvailable(), "manual check clears suppression and shows red dot");

    checker.applyReleaseForTesting(Release(QStringLiteral("v1.3.0")), false);
    Require(checker.updateAvailable(), "higher release resets old suppression");

    checker.applyReleaseForTesting(Release(QStringLiteral("v1.0.0")), false);
    Require(!checker.updateAvailable(), "current version clears stale state");

    return 0;
}
