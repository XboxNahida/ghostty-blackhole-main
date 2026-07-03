// ETheme.qml
import QtQuick
import QtQuick.Controls

Item {
    id: theme

    // === 默认暗色模式，跟随系统 (从 blackHoleCore 加载) ===
    property bool isDark: typeof blackHoleCore !== "undefined" ? blackHoleCore.isDark : true
    property bool followSystem: typeof blackHoleCore !== "undefined" ? blackHoleCore.followSystem : true

    // === 基础颜色 ===
    property color primaryColor: isDark ? "#1d1d1d" : "#FFFFFF"
    property color secondaryColor: isDark ? "#262626" : "#F8FAFD"
    property color textColor: isDark ? "#ffffff" : "#000000"
    property color borderColor: isDark ? "#666666" : "#cccccc"
    property color blurOverlayColor: isDark ? "#4E000000" : "#4EFFFFFF"
    property color defaultFocusColor: "#00C4B3"
    property color focusColor: typeof blackHoleCore !== "undefined" ? blackHoleCore.focusColor : defaultFocusColor

    // === 阴影统一样式 ===
    property color shadowColor: isDark ? "#80000000" : "#40000000"
    property real shadowBlur: 1.0
    property int shadowXOffset: 2
    property int shadowYOffset: 2

    // === 跟随系统主题 ===
    Timer {
        id: systemThemeTimer
        interval: 500
        running: true
        repeat: true
        onTriggered: {
            if (followSystem) {
                var systemDark = (Application.styleHints.colorScheme === Qt.Dark)
                if (systemDark !== isDark) {
                    isDark = systemDark
                }
            }
        }
    }

    // === 方法 ===
    function getBorderColor(focused) {
        return focused ? focusColor : borderColor
    }

    function toggleTheme() {
        followSystem = false
        isDark = !isDark
        if (typeof blackHoleCore !== "undefined") {
            blackHoleCore.followSystem = false
            blackHoleCore.isDark = isDark
        }
    }

    Behavior on focusColor {
        ColorAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }
}