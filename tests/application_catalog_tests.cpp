#include "application_catalog.h"

#include <QApplication>
#include <QFileInfo>

#include <cassert>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const ApplicationCatalogEntry empty =
        ApplicationCatalog_FromExecutable(QString(), false);
    assert(empty.displayName.isEmpty());
    assert(empty.processName.isEmpty());
    assert(empty.executablePath.isEmpty());
    assert(empty.iconDataUrl.isEmpty());

    const QString currentPath = QFileInfo(QString::fromLocal8Bit(argv[0])).absoluteFilePath();
    const ApplicationCatalogEntry current =
        ApplicationCatalog_FromExecutable(currentPath, false);
    assert(!current.displayName.isEmpty());
    assert(current.processName.compare(QFileInfo(currentPath).fileName(),
                                       Qt::CaseInsensitive) == 0);
    assert(QFileInfo(current.executablePath).isAbsolute());
    assert(current.iconDataUrl.isEmpty());

    const QVector<ApplicationCatalogEntry> running =
        ApplicationCatalog_EnumerateRunning(0);
    bool foundCurrent = false;
    for (const ApplicationCatalogEntry &entry : running) {
        assert(!entry.displayName.isEmpty());
        assert(!entry.processName.isEmpty());
        assert(!entry.executablePath.isEmpty());
        if (QFileInfo(entry.executablePath).canonicalFilePath().compare(
                QFileInfo(current.executablePath).canonicalFilePath(),
                Qt::CaseInsensitive) == 0) {
            foundCurrent = true;
        }
    }
    assert(foundCurrent);

    std::cout << "APPLICATION_CATALOG_TESTS_OK entries=" << running.size() << "\n";
    return 0;
}
