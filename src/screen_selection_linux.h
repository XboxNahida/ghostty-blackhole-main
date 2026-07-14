#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

// Linux/Wayland policy: 0 primary, 1 secondary, 2 safely falls back to primary,
// 3 one renderer per screen. A saved connector name wins over its old index.
QVector<int> ResolveLinuxDisplays(int target, const QString &savedName,
                                  const QStringList &screenNames, int primaryIndex = 0);
QString StableScreenNameForTarget(int target, const QStringList &screenNames,
                                  int primaryIndex = 0);
