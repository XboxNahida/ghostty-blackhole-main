#include "application_catalog.h"

#include <QBuffer>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QSet>

#include <algorithm>
#include <vector>

#ifdef Q_OS_WIN
#include <tlhelp32.h>
#endif

namespace {

#ifdef Q_OS_WIN
QString ReadVersionString(const QString &path, const wchar_t *field)
{
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(nativePath.c_str(), &ignored);
    if (size == 0) return {};

    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(nativePath.c_str(), 0, size, data.data())) return {};

    struct Translation { WORD language; WORD codePage; };
    Translation *translations = nullptr;
    UINT translationsSize = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void **>(&translations), &translationsSize) ||
        translationsSize < sizeof(Translation)) {
        return {};
    }

    const QString query = QStringLiteral("\\StringFileInfo\\%1%2\\%3")
        .arg(translations[0].language, 4, 16, QLatin1Char('0'))
        .arg(translations[0].codePage, 4, 16, QLatin1Char('0'))
        .arg(QString::fromWCharArray(field));
    wchar_t *value = nullptr;
    UINT valueLength = 0;
    if (!VerQueryValueW(data.data(), query.toStdWString().c_str(),
                        reinterpret_cast<void **>(&value), &valueLength) ||
        !value || valueLength <= 1) {
        return {};
    }
    return QString::fromWCharArray(value).trimmed();
}
#endif // Q_OS_WIN

QString IconDataUrl(const QFileInfo &fileInfo)
{
    QFileIconProvider provider;
    const QPixmap pixmap = provider.icon(fileInfo).pixmap(24, 24);
    if (pixmap.isNull()) return {};

    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !pixmap.save(&buffer, "PNG")) return {};
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
}

#ifdef Q_OS_WIN
QString ProcessPath(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};

    std::vector<wchar_t> path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    const BOOL queried = QueryFullProcessImageNameW(process, 0, path.data(), &length);
    CloseHandle(process);
    if (!queried || length == 0) return {};
    return QString::fromWCharArray(path.data(), static_cast<qsizetype>(length));
}

bool IsHiddenProcess(const ApplicationCatalogEntry &entry, const QString &windowsDirectory)
{
    if (entry.processName.compare(QStringLiteral("blackhole.exe"),
                                  Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (windowsDirectory.isEmpty()) return false;
    const QString prefix = QDir::toNativeSeparators(windowsDirectory) + QLatin1Char('\\');
    return QDir::toNativeSeparators(entry.executablePath).startsWith(
        prefix, Qt::CaseInsensitive);
}
#endif // Q_OS_WIN

} // namespace

ApplicationCatalogEntry ApplicationCatalog_FromExecutable(
    const QString &path,
    bool includeIcon)
{
    const QFileInfo source(path.trimmed());
    if (path.trimmed().isEmpty() || !source.exists() || !source.isFile()) return {};

    ApplicationCatalogEntry entry;
    entry.executablePath = source.canonicalFilePath();
    if (entry.executablePath.isEmpty()) entry.executablePath = source.absoluteFilePath();
    entry.executablePath = QDir::toNativeSeparators(entry.executablePath);
    entry.processName = source.fileName();
#ifdef Q_OS_WIN
    entry.displayName = ReadVersionString(entry.executablePath, L"FileDescription");
#else
    entry.displayName.clear();
#endif
    if (entry.displayName.isEmpty()) {
        entry.displayName = source.completeBaseName();
    }
    if (entry.displayName.isEmpty()) entry.displayName = entry.processName;
    if (includeIcon) entry.iconDataUrl = IconDataUrl(source);
    return entry;
}

#ifdef Q_OS_WIN
QVector<ApplicationCatalogEntry> ApplicationCatalog_EnumerateRunning(DWORD excludedPid)
{
    QVector<ApplicationCatalogEntry> entries;
    QSet<QString> knownPaths;
    const QString windowsDirectory = qEnvironmentVariable("WINDIR");

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return entries;

    PROCESSENTRY32W processEntry = {};
    processEntry.dwSize = sizeof(processEntry);
    if (Process32FirstW(snapshot, &processEntry)) {
        do {
            const DWORD pid = processEntry.th32ProcessID;
            if (pid == 0 || pid == excludedPid) continue;

            const QString path = ProcessPath(pid);
            if (path.isEmpty()) continue;
            ApplicationCatalogEntry entry = ApplicationCatalog_FromExecutable(path);
            if (entry.executablePath.isEmpty() || IsHiddenProcess(entry, windowsDirectory)) {
                continue;
            }

            const QString key = entry.executablePath.toCaseFolded();
            if (knownPaths.contains(key)) continue;
            knownPaths.insert(key);
            entries.append(std::move(entry));
        } while (Process32NextW(snapshot, &processEntry));
    }
    CloseHandle(snapshot);

    std::sort(entries.begin(), entries.end(),
              [](const ApplicationCatalogEntry &left,
                 const ApplicationCatalogEntry &right) {
                  const int byName = QString::localeAwareCompare(
                      left.displayName, right.displayName);
                  if (byName != 0) return byName < 0;
                  return left.executablePath.compare(
                      right.executablePath, Qt::CaseInsensitive) < 0;
              });
    return entries;
}
#else
QVector<ApplicationCatalogEntry> ApplicationCatalog_EnumerateRunning(int)
{
    return {};
}
#endif // Q_OS_WIN
