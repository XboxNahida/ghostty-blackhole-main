#include "avatar_storage.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QSaveFile>
#include <QUrl>

namespace {

QByteArray NormalizedFormat(const QString &sourcePath)
{
    const QByteArray suffix = QFileInfo(sourcePath).suffix().toLower().toUtf8();
    if (suffix == "jpg" || suffix == "jpeg") return "JPEG";
    if (suffix == "webp") return "WEBP";
    return suffix == "png" ? QByteArrayLiteral("PNG") : QByteArray();
}

void SetError(QString *errorMessage, const QString &message)
{
    if (errorMessage) *errorMessage = message;
}

}

bool AvatarStorage_Save(const QString &sourcePath,
                        const QString &destinationDirectory,
                        QString *savedPath,
                        QString *errorMessage)
{
    const QByteArray format = NormalizedFormat(sourcePath);
    if (format.isEmpty()) {
        SetError(errorMessage, QStringLiteral("头像格式不受支持"));
        return false;
    }

    QImageReader reader(sourcePath, format);
    const QImage image = reader.read();
    if (image.isNull()) {
        SetError(errorMessage, QStringLiteral("无法读取图片"));
        return false;
    }

    QDir directory(destinationDirectory);
    if (!directory.exists() && !QDir().mkpath(destinationDirectory)) {
        SetError(errorMessage, QStringLiteral("无法创建头像目录"));
        return false;
    }

    const QString suffix = QString::fromLatin1(format).toLower();
    const QString targetPath = directory.filePath(QStringLiteral("custom_avatar.%1").arg(suffix));
    QSaveFile output(targetPath);
    if (!output.open(QIODevice::WriteOnly)) {
        SetError(errorMessage, QStringLiteral("无法写入头像文件"));
        return false;
    }

    QImageWriter writer(&output, format);
    if (!writer.write(image) || !output.commit()) {
        SetError(errorMessage, QStringLiteral("保存头像失败"));
        return false;
    }

    if (savedPath) *savedPath = QFileInfo(targetPath).absoluteFilePath();
    if (errorMessage) errorMessage->clear();
    return true;
}

QString AvatarStorage_FileUrl(const QString &savedPath)
{
    if (savedPath.isEmpty() || !QFileInfo::exists(savedPath)) return {};
    return QUrl::fromLocalFile(QFileInfo(savedPath).absoluteFilePath()).toString(QUrl::FullyEncoded);
}
