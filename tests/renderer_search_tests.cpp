// renderer_search_tests.cpp — DS-05: 渲染器搜索顺序测试
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QStringList>

#include <cstdlib>
#include <iostream>

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
    return true;
}

// 模拟 findRendererExe 的搜索逻辑
QString FindRendererExe(const QString &appDir,
                        const QStringList &exeNames,
                        const QString &overridePath,
                        QString *projectRootOut = nullptr)
{
    if (!overridePath.isEmpty()) {
        if (QFileInfo::exists(overridePath)) {
            if (projectRootOut)
                *projectRootOut = QFileInfo(overridePath).absolutePath();
            return overridePath;
        }
        return {};
    }

    QDir dir(appDir);
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
        if (QFileInfo::exists(p)) {
            if (projectRootOut) {
                QFileInfo fi(p);
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
    return {};
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
        QString found = FindRendererExe(dir.path(), exeNames, exePath);
        Require(found == exePath, "explicit path returns set path");
    }

    // 测试2: 显式注入路径不存在
    {
        QTemporaryDir dir;
        QString found = FindRendererExe(dir.path(), exeNames,
                                         QStringLiteral("/nonexistent/test-renderer"));
        Require(found.isEmpty(), "explicit nonexistent path returns empty");
    }

    // 测试3: 搜索 — UI 同级
    {
        QTemporaryDir dir;
        QString exePath = dir.path() + QStringLiteral("/blackhole-renderer");
        Require(CreateFile(exePath), "create sibling renderer");
        QString found = FindRendererExe(dir.path(), exeNames, {});
        Require(found == exePath, "search finds sibling exe");
    }

    // 测试4: 搜索 — 项目结构: 从 appDir=project_root/sub/deep 搜索 ../../build/ 找到 project_root/build/blackhole-renderer
    {
        QTemporaryDir dir;
        QStringList exeNames = {QStringLiteral("blackhole-renderer")};
        // project_root/build/blackhole-renderer
        QString buildPath = dir.path() + QStringLiteral("/build");
        QDir().mkpath(buildPath);
        QString exePath = buildPath + QStringLiteral("/blackhole-renderer");
        Require(CreateFile(exePath), "create build renderer");
        // appDir 在 project_root/sub/deep (2层深), ../../build/ 应定位到 project_root/build/
        QString appDir = dir.path() + QStringLiteral("/sub/deep");
        QDir().mkpath(appDir);
        QString projectRoot;
        QString found = FindRendererExe(appDir, exeNames, {}, &projectRoot);
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
        QString found = FindRendererExe(dir.path(), exeNames, exePath);
        Require(found == exePath, "explicit path with spaces/chinese works");
    }

    // 测试6: 搜索 — 上一级目录
    {
        QTemporaryDir dir;
        QString subDir = dir.path() + QStringLiteral("/sub");
        QDir().mkpath(subDir);
        QString exePath = dir.path() + QStringLiteral("/blackhole-renderer");
        Require(CreateFile(exePath), "create parent renderer");
        QString found = FindRendererExe(subDir, exeNames, {});
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
        QString found = FindRendererExe(deepDir, exeNames, {});
        Require(!found.isEmpty(), "search finds root exe (non-empty)");
        Require(QFileInfo(found).canonicalFilePath() == QFileInfo(exePath).canonicalFilePath(),
                "search finds exe via ../../");
    }

    // 测试8: 无渲染器可执行文件 — 不崩溃
    {
        QTemporaryDir dir;
        QStringList emptyNames = {QStringLiteral("nonexistent-renderer-binary")};
        QString found = FindRendererExe(dir.path(), emptyNames, {});
        // 应该找不到,但程序不应崩溃
    }

    std::cout << "RENDERER_SEARCH_TESTS_OK\n";
    return 0;
}
