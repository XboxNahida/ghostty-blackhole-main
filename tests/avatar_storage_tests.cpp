#include "avatar_storage.h"

#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

void Require(bool condition, const char *message)
{
    if (condition) return;
    QTextStream(stderr) << "AVATAR_STORAGE_TEST_FAILED: " << message << Qt::endl;
    std::exit(1);
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    Require(temp.isValid(), "temporary directory is unavailable");

    const QString source = temp.filePath(QStringLiteral("source.png"));
    QImage image(8, 8, QImage::Format_ARGB32);
    image.fill(QColor(QStringLiteral("#22aa88")));
    Require(image.save(source, "PNG"), "cannot create source PNG");

    const QString destination = temp.filePath(QStringLiteral("profile"));
    QString savedPath;
    QString error;
    Require(AvatarStorage_Save(source, destination, &savedPath, &error),
            "valid PNG was rejected");
    Require(savedPath.endsWith(QStringLiteral("custom_avatar.png")),
            "destination filename is not normalized");
    Require(QFile::exists(savedPath), "saved avatar is missing");
    Require(AvatarStorage_FileUrl(savedPath).startsWith(QStringLiteral("file:///")),
            "saved avatar URL is not a local file URL");

    QFile existing(savedPath);
    Require(existing.open(QIODevice::ReadOnly), "cannot read saved avatar");
    const QByteArray originalBytes = existing.readAll();
    existing.close();

    const QString invalidSource = temp.filePath(QStringLiteral("invalid.png"));
    QFile invalid(invalidSource);
    Require(invalid.open(QIODevice::WriteOnly), "cannot create invalid source");
    invalid.write("not an image");
    invalid.close();

    QString unchangedPath = savedPath;
    Require(!AvatarStorage_Save(invalidSource, destination, &unchangedPath, &error),
            "invalid image was accepted");
    Require(unchangedPath == savedPath, "failure replaced the caller's old path");
    Require(existing.open(QIODevice::ReadOnly), "saved avatar disappeared after failure");
    Require(existing.readAll() == originalBytes, "saved avatar changed after failure");

    const QString corruptPath = temp.filePath(QStringLiteral("corrupt.png"));
    Require(QFile::copy(savedPath, corruptPath), "cannot create corrupt avatar fixture");
    QFile corrupt(corruptPath);
    Require(corrupt.open(QIODevice::WriteOnly), "cannot corrupt avatar fixture");
    corrupt.resize(0);
    corrupt.write("broken image");
    corrupt.close();
    Require(AvatarStorage_FileUrl(corruptPath).isEmpty(),
            "corrupted saved avatar was treated as readable");

    QTextStream(stdout) << "AVATAR_STORAGE_OK" << Qt::endl;
    return 0;
}
