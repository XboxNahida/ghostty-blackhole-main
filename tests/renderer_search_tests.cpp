// renderer_search_tests.cpp — DS-05: 渲染器搜索顺序测试 (测试生产代码 ResolveRendererPath)
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

#include "renderer_search_linux.h"

namespace {

void Require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
    std::cout << "PASS: " << message << "\n";
}

bool CreateFile(const QString &path)
{
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write("x");
    f.close();
    // 设置为可执行
    QFile::setPermissions(path, QFile::permissions(path) | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList exeNames = {QStringLiteral("blackhole-renderer")};

    // 测试1: 显式注入路径有效
    {
        QTemporaryDir dir;
        QString exePath = dir.path() + QStringLiteral("/blackhole-renderer");
        Require(CreateFile(exePath), "create temp renderer exe");
        QString found = ResolveRendererPath(exePath, exeNames, dir.path());
        Require(QFileInfo(found).canonicalFilePath() == QFileInfo(exePath).canonicalFilePath(),
                "explicit path returns set path");
    }

    // 测试2: 显式注入路径不存在
    {
        QTemporaryDir dir;
        QString found = ResolveRendererPath(QStringLiteral("/nonexistent/test-renderer"),
                                             exeNames, dir.path());
        Require(found.isEmpty(), "explicit nonexistent path returns empty");
    }

    // 测试3: 搜索 — UI 同级
    {
        QTemporaryDir dir;
        QString exePath = dir.path() + QStringLiteral("/blackhole-renderer");
        Require(CreateFile(exePath), "create sibling renderer");
        QString found = ResolveRendererPath({}, exeNames, dir.path());
        Require(QFileInfo(found).canonicalFilePath() == QFileInfo(exePath).canonicalFilePath(),
                "search finds sibling exe");
    }

    // 测试4: 搜索 — 项目结构: 从 appDir=project_root/sub/deep 搜索 ../../build/ 找到 project_root/build/blackhole-renderer
    {
        QTemporaryDir dir;
        // project_root/build/blackhole-renderer
        QString buildPath = dir.path() + QStringLiteral("/build");
        QDir().mkpath(buildPath);
        QString exePath = buildPath + QStringLiteral("/blackhole-renderer");
        Require(CreateFile(exePath), "create build renderer");
        // appDir 在 project_root/sub/deep (2层深), ../../build/ 应定位到 project_root/build/
        QString appDir = dir.path() + QStringLiteral("/sub/deep");
        QDir().mkpath(appDir);
        QString projectRoot;
        QString found = ResolveRendererPath({}, exeNames, appDir, &projectRoot);
        Require(!found.isEmpty(), "search finds build exe (non-empty)");
        Require(QFileInfo(found).canonicalFilePath() == QFileInfo(exePath).canonicalFilePath(),
                "search finds build exe via ../../build/");
        Require(QDir(projectRoot).canonicalPath() == QDir(dir.path()).canonicalPath(),
                "projectRoot is parent of build dir");
    }

    // 测试5: 路径含空格/中文
    {
        QTemporaryDir dir;
        QString spacedPath = dir.path() + QStringLiteral("/my renderer/黑洞渲染器");
        QDir().mkpath(spacedPath);
        QString exePath = spacedPath + QStringLiteral("/blackhole-renderer");
        Require(CreateFile(exePath), "create renderer in spaced path");
        QString found = ResolveRendererPath(exePath, exeNames, dir.path());
        Require(QFileInfo(found).canonicalFilePath() == QFileInfo(exePath).canonicalFilePath(),
                "explicit path with spaces/chinese works");
    }

    // 测试6: 搜索 — 上一级目录
    {
        QTemporaryDir dir;
        QString subDir = dir.path() + QStringLiteral("/sub");
        QDir().mkpath(subDir);
        QString exePath = dir.path() + QStringLiteral("/blackhole-renderer");
        Require(CreateFile(exePath), "create parent renderer");
        QString found = ResolveRendererPath({}, exeNames, subDir);
        Require(!found.isEmpty(), "search finds parent dir exe (non-empty)");
        Require(QFileInfo(found).canonicalFilePath() == QFileInfo(exePath).canonicalFilePath(),
                "search finds parent dir exe via ../");
    }

    // 测试7: 搜索 — 上溯两级
    {
        QTemporaryDir dir;
        QString deepDir = dir.path() + QStringLiteral("/a/b");
        QDir().mkpath(deepDir);
        QString exePath = dir.path() + QStringLiteral("/blackhole-renderer");
        Require(CreateFile(exePath), "create root renderer");
        QString found = ResolveRendererPath({}, exeNames, deepDir);
        Require(!found.isEmpty(), "search finds root exe (non-empty)");
        Require(QFileInfo(found).canonicalFilePath() == QFileInfo(exePath).canonicalFilePath(),
                "search finds exe via ../../");
    }

    // 测试8: 无渲染器可执行文件 — 不崩溃
    {
        QTemporaryDir dir;
        QStringList emptyNames = {QStringLiteral("nonexistent-renderer-binary")};
        QString found = ResolveRendererPath({}, emptyNames, dir.path());
        // 应该找不到,但程序不应崩溃
    }

    // 测试9: 搜索 — 非可执行文件不被接受
    {
        QTemporaryDir dir;
        QString exePath = dir.path() + QStringLiteral("/blackhole-renderer");
        QFile f(exePath);
        Require(f.open(QIODevice::WriteOnly), "create non-executable file");
        f.write("x");
        f.close();
        // 不设置可执行权限
        QString found = ResolveRendererPath({}, exeNames, dir.path());
        Require(found.isEmpty(), "non-executable search file is rejected");
    }

    // 测试10: 显式覆盖路径为目录 — 应返回空
    {
        QTemporaryDir dir;
        QString found = ResolveRendererPath(dir.path(), exeNames, dir.path());
        Require(found.isEmpty(), "directory as explicit override returns empty");
    }

    // 测试11: 显式覆盖路径为不可执行文件 — 应返回空
    {
        QTemporaryDir dir;
        QString filePath = dir.path() + QStringLiteral("/renderer-file");
        QFile f(filePath);
        Require(f.open(QIODevice::WriteOnly), "create non-executable override file");
        f.write("x");
        f.close();
        // 不设置可执行权限
        QString found = ResolveRendererPath(filePath, exeNames, dir.path());
        Require(found.isEmpty(), "non-executable file as explicit override returns empty");
    }

    std::cout << "RENDERER_SEARCH_TESTS_OK\n";
    return 0;
}
