import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "components" as Components

import BlakholeUI 1.0
import "pages" as Pages

ApplicationWindow {
    id: root
    visible: true
    width: 1400
    height: 800
    minimumWidth: 1400
    minimumHeight: 800
    title: "Blakhole UI"
    flags: Qt.Window | Qt.FramelessWindowHint

    FontLoader {
        id: iconFont
        source: "qrc:/new/prefix1/fonts/fontawesome-free-6.7.2-desktop/otfs/Font Awesome 6 Free-Solid-900.otf"
    }

    Components.ETheme {
        id: theme
    }

    SystemTray {
        id: systemTray
        Component.onCompleted: systemTray.visible = false
        onExitRequested: root.close()
    }

    Component.onCompleted: {
        if (root.launchMinimized && blackHoleCore) {
            root.visible = false
            systemTray.visible = true
            blackHoleCore.applyAndStart()
        }
    }

    color: theme.primaryColor

    readonly property int resizeMargin: 6
    property int currentPageIndex: 0
    property bool skipExitDialog: blackHoleCore ? blackHoleCore.skipExitDialog : false
    property int defaultCloseAction: blackHoleCore ? blackHoleCore.defaultCloseAction : 0
    property bool autoStart: blackHoleCore ? blackHoleCore.autoStart : false
    property bool launchMinimized: blackHoleCore ? blackHoleCore.launchMinimized : false
    property int screenTarget: blackHoleCore ? blackHoleCore.screenTarget : 0

    Item {
        id: contentWrapper
        anchors.fill: parent

        // === 纯色背景(供毛玻璃侧边栏模糊) ===
        Rectangle {
            id: background
            anchors.fill: parent
            color: theme.primaryColor
        }

        // === 顶部可拖动区域（自定义标题栏） ===
        Rectangle {
            id: customTitleBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 36
            color: "transparent"
            z: 1000

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onPressed: root.startSystemMove()
            }
        }

        // === 无边框窗口缩放区域 ===
        // 左侧缩放
        Item {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: resizeMargin
            z: 1000
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor
                onPressed: root.startSystemResize(Qt.LeftEdge)
            }
        }

        // 右侧缩放
        Item {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: resizeMargin
            z: 1000
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor
                onPressed: root.startSystemResize(Qt.RightEdge)
            }
        }

        // 底边缩放
        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: resizeMargin
            z: 1000
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeVerCursor
                onPressed: root.startSystemResize(Qt.BottomEdge)
            }
        }

        // 顶边缩放（避开右上按钮区域）
        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: titleButtonsPanel.width + 20
            height: resizeMargin
            z: 1000
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeVerCursor
                onPressed: root.startSystemResize(Qt.TopEdge)
            }
        }

        // 左下角斜向缩放
        Item {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: resizeMargin
            height: resizeMargin
            z: 1000
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeBDiagCursor
                onPressed: root.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
            }
        }

        // 右下角斜向缩放
        Item {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: resizeMargin
            height: resizeMargin
            z: 1000
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeFDiagCursor
                onPressed: root.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
            }
        }

        // === 右上角标题文字 ===
        Item {
            id: appTitle
            width: 400
            height: 210
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 50

            Column {
                spacing: 4
                anchors.centerIn: parent

                Row {
                    spacing: 6
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text { text: "B"; font.pixelSize: 36; font.bold: true; color: theme.focusColor }
                    Text { text: "l"; font.pixelSize: 36; font.bold: true; color: theme.focusColor }
                    Text { text: "a"; font.pixelSize: 36; font.bold: true; color: theme.focusColor }
                    Text { text: "k"; font.pixelSize: 36; font.bold: true; color: theme.focusColor }
                    Text { text: "h"; font.pixelSize: 36; font.bold: true; color: theme.focusColor }
                    Text { text: "o"; font.pixelSize: 36; font.bold: true; color: theme.focusColor }
                    Text { text: "l"; font.pixelSize: 36; font.bold: true; color: theme.focusColor }
                    Text { text: "e"; font.pixelSize: 36; font.bold: true; color: theme.focusColor }
                    Text { text: " "; font.pixelSize: 36 }
                    Text { text: "U"; font.pixelSize: 36; font.bold: true; color: theme.textColor }
                    Text { text: "I"; font.pixelSize: 36; font.bold: true; color: theme.textColor }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\u9ed1\u6d1e\u53ef\u89c6\u5316\u914d\u7f6e\u5de5\u5177"
                    font.pixelSize: 24
                    color: theme.textColor
                    opacity: 0.5
                }
            }
        }

        // === 左侧侧边栏毛玻璃卡片 ===
        Components.EBlurCard {
            id: leftSidebarCard
            width: 250
            height: parent.height
            blurSource: background
            borderRadius: 35
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: -30
            layer.enabled: true

            // 头像
            Components.EAvatar {
                id: avatar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: 40
                anchors.leftMargin: 50
                avatarSource: "qrc:/new/prefix1/fonts/pic/avatar.png"
            }

            // 时间
            Components.ETimeDisplay {
                is24Hour: true
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: 160
                anchors.leftMargin: 50
            }

            // 侧边栏列表
            Components.EList {
                id: sidebarList
                backgroundVisible: false
                radius: 16
                width: parent.width - 40
                height: 210
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: 380
                anchors.leftMargin: 50

                model: ListModel {
                    ListElement { display: "\u9ed1\u6d1e\u914d\u7f6e"; iconChar: "\uf661" }
                    ListElement { display: "\u9ad8\u7ea7\u8bbe\u7f6e"; iconChar: "\uf085" }
                    ListElement { display: "\u5b9a\u65f6\u663e\u793a"; iconChar: "\uf017" }
                    ListElement { display: "\u7a7a\u95f2\u540d\u5355"; iconChar: "\uf502" }
                }

                onItemClicked: function(index, data) {
                    root.currentPageIndex = index
                }
            }

            // 设置按钮
            Components.EButton {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.bottomMargin: 100
                anchors.leftMargin: 30
                backgroundVisible: false
                text: "\u8bbe\u7f6e"
                iconCharacter: "\uf013"
                onClicked: {
                    settingsDrawer.toggle()
                }
            }

            // 主题切换
            Components.EButton {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.bottomMargin: 50
                anchors.leftMargin: 30
                backgroundVisible: false
                text: theme.isDark ? "\u65e5\u95f4" : "\u591c\u95f4"
                iconCharacter: theme.isDark ? "\uf185" : "\uf186"
                iconRotateOnClick: true
                onClicked: {
                    theme.toggleTheme()
                }
            }
        }

        // === Toast 通知 ===
        Components.EToast {
            id: toast
            theme: theme
            anchors.top: contentWrapper.top
            anchors.horizontalCenter: contentWrapper.horizontalCenter
            anchors.topMargin: 32 + toast.yOffset
        }

        // === 右侧内容区域 ===
        Item {
            id: contentArea
            anchors.top: parent.top
            anchors.left: leftSidebarCard.right
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.topMargin: 50
            anchors.leftMargin: 10
            anchors.rightMargin: 40
            anchors.bottomMargin: 20

            // 黑洞配置页面
            Pages.BlackholeConfig {
                bhCore: blackHoleCore
                anchors.fill: parent
                visible: root.currentPageIndex === 0
            }

            // 高级设置页面
            Pages.AdvancedConfig {
                anchors.fill: parent
                visible: root.currentPageIndex === 1
            }

            // 定时显示页面
            Pages.ScheduleConfig {
                anchors.fill: parent
                visible: root.currentPageIndex === 2
            }

            // 空闲检测名单页面
            Pages.IdleListConfig {
                anchors.fill: parent
                visible: root.currentPageIndex === 3
            }
        }

        // === 右侧设置抽屉 ===
        Components.EDrawer {
            id: settingsDrawer
            panelWidth: 400
            opened: false
            backgroundVisible: true
            anchors.right: parent.right
            anchors.rightMargin: -24
            padding: 30

            Column {
                spacing: 20
                width: parent.width

                Text {
                    text: "\u8bbe\u7f6e"
                    font.pixelSize: 24
                    font.bold: true
                    color: theme.focusColor
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: theme.borderColor
                    opacity: 0.3
                }

                Rectangle {
                    width: parent.width
                    height: 50
                    radius: 12
                    color: theme.secondaryColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Text {
                            text: "\uf0eb"
                            font.family: iconFont.name
                            font.pixelSize: 18
                            color: theme.focusColor
                        }

                        Text {
                            text: "\u6697\u8272\u6a21\u5f0f"
                            font.pixelSize: 15
                            color: theme.textColor
                            Layout.fillWidth: true
                        }

                        Components.ESwitchButton {
                            checked: theme.isDark
                            onToggled: {
                                theme.toggleTheme()
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 50
                    radius: 12
                    color: theme.secondaryColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Text {
                            text: "\uf2f5"
                            font.family: iconFont.name
                            font.pixelSize: 18
                            color: theme.textColor
                        }

                        Text {
                            text: "\u9000\u51fa\u65f6\u8be2\u95ee\u786e\u8ba4"
                            font.pixelSize: 15
                            color: theme.textColor
                            Layout.fillWidth: true
                        }

                        CheckBox {
                            id: exitAskCheck
                            checked: !root.skipExitDialog
                            onToggled: { root.skipExitDialog = !checked; if (blackHoleCore) blackHoleCore.skipExitDialog = !checked }

                            indicator: Rectangle {
                                implicitWidth: 20
                                implicitHeight: 20
                                x: exitAskCheck.leftPadding
                                y: parent.height / 2 - height / 2
                                radius: 4
                                color: exitAskCheck.checked ? theme.focusColor : "transparent"
                                border.color: exitAskCheck.checked ? theme.focusColor : theme.borderColor
                                border.width: 2

                                Behavior on color { ColorAnimation { duration: 120 } }

                                Text {
                                    anchors.centerIn: parent
                                    text: "\uf00c"
                                    font.family: iconFont.name
                                    font.pixelSize: 12
                                    color: "#ffffff"
                                    visible: exitAskCheck.checked
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 50
                    radius: 12
                    color: theme.secondaryColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Text {
                            text: "\uf013"
                            font.family: iconFont.name
                            font.pixelSize: 18
                            color: theme.textColor
                        }

                        Text {
                            text: "\u5f00\u673a\u81ea\u52a8\u542f\u52a8"
                            font.pixelSize: 15
                            color: theme.textColor
                            Layout.fillWidth: true
                        }

                        CheckBox {
                            id: autoStartCheck
                            checked: root.autoStart
                            onToggled: { root.autoStart = checked; if (blackHoleCore) blackHoleCore.autoStart = checked }

                            indicator: Rectangle {
                                implicitWidth: 20
                                implicitHeight: 20
                                x: autoStartCheck.leftPadding
                                y: parent.height / 2 - height / 2
                                radius: 4
                                color: autoStartCheck.checked ? theme.focusColor : "transparent"
                                border.color: autoStartCheck.checked ? theme.focusColor : theme.borderColor
                                border.width: 2

                                Behavior on color { ColorAnimation { duration: 120 } }

                                Text {
                                    anchors.centerIn: parent
                                    text: "\uf00c"
                                    font.family: iconFont.name
                                    font.pixelSize: 12
                                    color: "#ffffff"
                                    visible: autoStartCheck.checked
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 50
                    radius: 12
                    color: theme.secondaryColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Text {
                            text: "\uf013"
                            font.family: iconFont.name
                            font.pixelSize: 18
                            color: theme.textColor
                        }


                        Text {
                            text: "\u542f\u52a8\u540e\u81ea\u52a8\u9690\u85cf\u754c\u9762\u5e76\u542f\u52a8\u9ed1\u6d1e"
                            font.pixelSize: 15
                            color: theme.textColor
                            Layout.fillWidth: true
                        }
                        CheckBox {
                            id: launchMinimizedCheck
                            checked: root.launchMinimized
                            onToggled: { root.launchMinimized = checked; if (blackHoleCore) blackHoleCore.launchMinimized = checked }

                            indicator: Rectangle {
                                implicitWidth: 20
                                implicitHeight: 20
                                x: launchMinimizedCheck.leftPadding
                                y: parent.height / 2 - height / 2
                                radius: 4
                                color: launchMinimizedCheck.checked ? theme.focusColor : "transparent"
                                border.color: launchMinimizedCheck.checked ? theme.focusColor : theme.borderColor
                                border.width: 2

                                Behavior on color { ColorAnimation { duration: 120 } }

                                Text {
                                    anchors.centerIn: parent
                                    text: "\uf00c"
                                    font.family: iconFont.name
                                    font.pixelSize: 12
                                    color: "#ffffff"
                                    visible: launchMinimizedCheck.checked
                                }
                            }

                        }
                    }
                }
                // 多显示器选择
                Rectangle {
                    width: parent.width
                    height: 56
                    radius: 12
                    color: theme.secondaryColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Text {
                            text: "\uf108"
                            font.family: iconFont.name
                            font.pixelSize: 18
                            color: theme.focusColor
                        }

                        Text {
                            text: "\u591a\u663e\u793a\u5668"
                            font.pixelSize: 15
                            color: theme.textColor
                            Layout.fillWidth: true
                        }

                        Components.EDropDown {
                            id: screenTargetDrop
                            preferredWidth: 150
                            model: ["\u4e3b\u5c4f\uff08\u9ed8\u8ba4\uff09", "\u526f\u5c4f", "\u8de8\u5c4f", "\u4e00\u5c4f\u4e00\u9ed1\u6d1e"]
                            currentIndex: root.screenTarget
                            onActivated: function(index) {
                                root.screenTarget = index
                                if (blackHoleCore) blackHoleCore.screenTarget = index
                            }
                        }
                    }
                }


                Rectangle {
                    width: parent.width
                    height: 50
                    radius: 12
                    color: theme.secondaryColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Text {
                            text: "\uf05a"
                            font.family: iconFont.name
                            font.pixelSize: 18
                            color: theme.textColor
                        }

                        Text {
                            text: "\u5173\u4e8e Blakhole UI"
                            font.pixelSize: 15
                            color: theme.textColor
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "v1.0.0"
                            font.pixelSize: 13
                            color: theme.borderColor
                        }
                    }
                }
            }
        }

    } // end of contentWrapper

    // === 右上角窗口控制按钮面板 ===
    Components.EBlurCard {
        id: titleButtonsPanel
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 8
        anchors.topMargin: 4
        width: titleButtonsRow.implicitWidth + 14
        height: 38
        borderRadius: 14
        blurSource: contentWrapper
        blurAmount: 1.2
        blurMax: 32
        borderColor: Qt.rgba(theme.borderColor.r, theme.borderColor.g, theme.borderColor.b, theme.borderColor.a * 0.6)
        borderWidth: 1
        z: 1000

        Row {
            id: titleButtonsRow
            spacing: 8
            anchors.fill: parent
            anchors.margins: 7

            Components.EButton {
                width: 28
                height: 24
                radius: 12
                backgroundVisible: true
                text: ""
                iconCharacter: "\uf068"
                onClicked: systemTray.show()
            }

            Components.EButton {
                width: 28
                height: 24
                radius: 12
                backgroundVisible: true
                text: ""
                iconCharacter: "\uf00d"
                onClicked: {
                    if (skipExitDialog) {
                        if (defaultCloseAction === 1) {
                            systemTray.show()
                        } else {
                            root.close()
                        }
                    } else {
                        exitDialog.open()
                    }
                }
            }
        }
    }

    // === 退出确认对话框 ===
    Components.EAlertDialog {
        id: exitDialog
        title: "\u8981\u9000\u51fa\u5e94\u7528\u5417\uff1f"
        message: "\u9000\u51fa\u5c06\u5173\u95ed\u6240\u6709\u7a97\u53e3\u3002"
        cancelText: "\u53d6\u6d88"
        confirmText: "\u9000\u51fa"
        extraText: "\u6700\u5c0f\u5316"
        checkboxText: "\u4ee5\u540e\u4e0d\u518d\u8be2\u95ee"
        dismissOnOverlay: false
        onConfirm: {
            if (exitDialog.checkboxChecked) { root.defaultCloseAction = 0; if (blackHoleCore) blackHoleCore.defaultCloseAction = 0 }
            root.close()
        }
        onExtra: {
            if (exitDialog.checkboxChecked) { root.defaultCloseAction = 1; if (blackHoleCore) blackHoleCore.defaultCloseAction = 1 }
            systemTray.show()
        }
        onCheckboxToggled: function(checked) { root.skipExitDialog = checked; if (blackHoleCore) blackHoleCore.skipExitDialog = checked }
    }

    // === 关闭时保存所有配置 ===
    onClosing: {
        if (typeof blackHoleCore !== "undefined") {
            blackHoleCore.saveConfig()
        }
    }
}
