// renderer_search_linux.cpp — Linux 渲染器搜索 (可测试单元)
#include "renderer_search_linux.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

QString ResolveRendererPath(const QString &overridePath,
                            const QStringList &exeNames,
                            const QString &appDir,
                            QString *projectRootOut)
{
    // 1) 显式注入路径
    if (!overridePath.isEmpty()) {
        if (QFileInfo::exists(overridePath)) {
            if (projectRootOut)
                *projectRootOut = QFileInfo(overridePath).absolutePath();
            return overridePath;
        }
        return {};
    }

    // 2) UI 同级 → 上溯各级 → 兄弟 build 目录
    QDir dir(appDir.isEmpty() ? QCoreApplication::applicationDirPath() : appDir);
    QStringList searchPaths;
    for (const QString &name : exeNames) {
        searchPaths << dir.absoluteFilePath(name);
        searchPaths << dir.absoluteFilePath(QStringLiteral("../") + name);
        searchPaths << dir.absoluteFilePath(QStringLiteral("../../") + name);
        searchPaths << dir.absoluteFilePath(QStringLiteral("../../build/") + name);
        searchPaths << dir.absoluteFilePath(QStringLiteral("../../../") + name);
        searchPaths << dir.absoluteFilePath(QStringLiteral("../../../build/") + name);
        searchPaths << dir.absoluteFilePath(QStringLiteral("../../../release/") + name);
    }

    for (const QString &p : searchPaths) {
        QFileInfo fi(p);
        if (fi.exists() && fi.isFile() && fi.isExecutable()) {
            if (projectRootOut) {
                QDir exeDir = fi.dir();
                QString parent = exeDir.absolutePath();
                if (exeDir.dirName().compare("build", Qt::CaseInsensitive) == 0
                    || exeDir.dirName().compare("release", Qt::CaseInsensitive) == 0) {
                    QDir root = exeDir;
                    root.cdUp();
                    parent = root.absolutePath();
                }
                *projectRootOut = parent;
            }
            return p;
        }
    }

    // 3) QStandardPaths 回退
    for (const QString &name : exeNames) {
        QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty()) {
            if (projectRootOut)
                *projectRootOut = QFileInfo(found).absolutePath();
            return found;
        }
    }

    return {};
}
