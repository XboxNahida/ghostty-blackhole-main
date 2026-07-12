#pragma once

#include <QString>
#include <QVector>
#include <windows.h>

struct ApplicationCatalogEntry {
    QString displayName;
    QString processName;
    QString executablePath;
    QString iconDataUrl;
};

ApplicationCatalogEntry ApplicationCatalog_FromExecutable(
    const QString &path,
    bool includeIcon = true);

QVector<ApplicationCatalogEntry> ApplicationCatalog_EnumerateRunning(
    DWORD excludedPid = 0);
