#pragma once

#include <QString>
#include <QVector>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

struct ApplicationCatalogEntry {
    QString displayName;
    QString processName;
    QString executablePath;
    QString iconDataUrl;
};

ApplicationCatalogEntry ApplicationCatalog_FromExecutable(
    const QString &path,
    bool includeIcon = true);

#ifdef Q_OS_WIN
QVector<ApplicationCatalogEntry> ApplicationCatalog_EnumerateRunning(
    DWORD excludedPid = 0);
#else
QVector<ApplicationCatalogEntry> ApplicationCatalog_EnumerateRunning(
    int excludedPid = 0);
#endif
