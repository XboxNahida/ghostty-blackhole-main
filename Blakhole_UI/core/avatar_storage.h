#pragma once

#include <QString>

bool AvatarStorage_Save(const QString &sourcePath,
                        const QString &destinationDirectory,
                        QString *savedPath,
                        QString *errorMessage);

QString AvatarStorage_FileUrl(const QString &savedPath);
