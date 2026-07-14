// renderer_search_linux.h — Linux 渲染器搜索 (可测试单元)
#pragma once

#include <QString>
#include <QStringList>

// 按优先级顺序解析渲染器路径:
// 1) 显式注入路径 overridePath (非空且文件存在时使用)
// 2) UI 同级 → 上溯各级 → 兄弟 build/ 目录
// 3) QStandardPaths::findExecutable()
// exeNames: 要搜索的可执行文件名列表 (如 {"blackhole-renderer"})
// appDir: 起始搜索目录 (空则用 QCoreApplication::applicationDirPath())
// projectRootOut: 若非空, 输出项目根目录
// 返回空 QString 表示未找到
QString ResolveRendererPath(const QString &overridePath = {},
                            const QStringList &exeNames = {QStringLiteral("blackhole-renderer")},
                            const QString &appDir = {},
                            QString *projectRootOut = nullptr);
