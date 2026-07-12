#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QtGlobal>

inline constexpr int kUpdateReleaseMaxTagLength = 64;
inline constexpr int kUpdateReleaseMaxNameLength = 256;
inline constexpr int kUpdateReleaseMaxNotesLength = 16384;
inline constexpr int kUpdateReleaseMaxUrlLength = 2048;
inline constexpr qsizetype kUpdateReleaseMaxJsonBytes = 1024 * 1024;

struct ParsedVersion
{
    bool valid = false;
    quint32 major = 0;
    quint32 minor = 0;
    quint32 patch = 0;
};

struct UpdateReleaseInfo
{
    bool valid = false;
    QString tagName;
    ParsedVersion version;
    QString name;
    QString notes;
    QUrl htmlUrl;
};

ParsedVersion ParseReleaseVersion(const QString &label);
int CompareReleaseVersions(const ParsedVersion &left, const ParsedVersion &right);
UpdateReleaseInfo ParseGitHubRelease(const QByteArray &json, QString *error);
