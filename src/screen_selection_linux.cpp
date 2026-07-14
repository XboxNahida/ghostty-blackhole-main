#include "screen_selection_linux.h"
#include <algorithm>

QVector<int> ResolveLinuxDisplays(int target, const QString &savedName,
                                  const QStringList &names, int primary)
{
    if (names.isEmpty()) return {0};
    primary = std::clamp(primary, 0, static_cast<int>(names.size()) - 1);
    if (target == 3) {
        QVector<int> all;
        for (int i = 0; i < names.size(); ++i) all.append(i);
        return all;
    }
    if (!savedName.isEmpty()) {
        const int stable = names.indexOf(savedName);
        if (stable >= 0) return {stable};
    }
    if (target == 1 && names.size() > 1) return {primary == 0 ? 1 : 0};
    // A single cross-global-coordinate surface is not portable on Wayland.
    return {primary};
}

QString StableScreenNameForTarget(int target, const QStringList &names, int primary)
{
    if (names.isEmpty() || target == 3) return {};
    const auto displays = ResolveLinuxDisplays(target, {}, names, primary);
    return names.value(displays.value(0));
}
