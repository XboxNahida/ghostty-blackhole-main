// AdvancedConfig.qml — 高级行为设置页面
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Root
import "../components" as Components

Item {
    id: advPage

    anchors.fill: parent
    anchors.margins: 20

    property var bhCore: null

    property bool videoAsIdle: bhCore ? bhCore.videoAsIdle : false
    property bool followMouse: bhCore ? bhCore.followMouse : false
    property real mouseInertia: bhCore ? bhCore.mouseInertia : 0.30
    property bool limitMouseOvershoot: bhCore ? bhCore.limitMouseOvershoot : true
    property bool randomPath: bhCore ? bhCore.randomPath : true
    property int animationSpeed: bhCore ? bhCore.animationSpeed : 1
    property bool screenSwallow: bhCore ? bhCore.screenSwallow : false
    property real swallowStrength: bhCore ? bhCore.swallowStrength : 0.65
    property real distortion: bhCore ? bhCore.distortion : 1.0
    property bool allowRecordingCapture: bhCore ? bhCore.allowRecordingCapture : false
    property real holeSize: bhCore ? bhCore.holeSize : 1.0
    property bool growEnabled: bhCore ? bhCore.growEnabled : false
    property real initialSize: bhCore ? bhCore.initialSize : 0.3
    // 新增：捕获方式 / 固定大小（对齐 ImGui UI / main.cpp 后端）
    property int captureMode: bhCore ? bhCore.captureMode : -1  // -1=自动 0=WGC 1=DXGI
    property bool fixedSize: bhCore ? bhCore.fixedSize : false
    property real fixedLevel: bhCore ? bhCore.fixedLevel : 1.0   // 0.01~1.0

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        // === 标题 ===
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "\uf085"
                font.family: iconFont.name
                font.pixelSize: 22
                color: theme.focusColor
            }
            Text {
                text: "高级设置"
                font.pixelSize: 22
                font.bold: true
                color: theme.focusColor
            }
            Item { Layout.fillWidth: true }
        }

        // === 空闲检测 ===
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: idleCol.implicitHeight + 24
            radius: 16
            color: theme.secondaryColor
            opacity: 0.85

            ColumnLayout {
                id: idleCol
                anchors.fill: parent
                anchors.margins: 20
                spacing: 4

                Text {
                    text: "\uf017  空闲检测"
                    font.family: iconFont.name
                    font.pixelSize: 14
                    color: theme.focusColor
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.borderColor
                    opacity: 0.2
                }

                // 视频检测
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: "播放视频时视为空闲"
                        font.pixelSize: 14
                        color: theme.textColor
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "检测到前景视频播放时不阻止黑洞触发"
                        font.pixelSize: 11
                        color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                    }
                    CheckBox {
                        id: videoCheck
                        checked: advPage.videoAsIdle
                        onToggled: {
                            advPage.videoAsIdle = checked
                            if (bhCore) bhCore.videoAsIdle = checked
                        }
                        indicator: Rectangle {
                            implicitWidth: 20; implicitHeight: 20
                            x: videoCheck.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 4
                            color: videoCheck.checked ? theme.focusColor : "transparent"
                            border.color: videoCheck.checked ? theme.focusColor : theme.borderColor
                            border.width: 2
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Text {
                                anchors.centerIn: parent; text: "\uf00c"
                                font.family: iconFont.name; font.pixelSize: 12
                                color: "#ffffff"; visible: videoCheck.checked
                            }
                        }
                    }
                }

                // 跟随鼠标
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: "跟随鼠标移动"
                        font.pixelSize: 14
                        color: theme.textColor
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "黑洞效果跟随光标位置偏移"
                        font.pixelSize: 11
                        color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                    }
                    CheckBox {
                        id: mouseCheck
                        checked: advPage.followMouse
                        onToggled: {
                            advPage.followMouse = checked
                            if (bhCore) bhCore.followMouse = checked
                        }
                        indicator: Rectangle {
                            implicitWidth: 20; implicitHeight: 20
                            x: mouseCheck.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 4
                            color: mouseCheck.checked ? theme.focusColor : "transparent"
                            border.color: mouseCheck.checked ? theme.focusColor : theme.borderColor
                            border.width: 2
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Text {
                                anchors.centerIn: parent; text: "\uf00c"
                                font.family: iconFont.name; font.pixelSize: 12
                                color: "#ffffff"; visible: mouseCheck.checked
                            }
                        }
                    }
                }

                Components.ESlider {
                    label: "鼠标惯性"
                    from: 0.0; to: 1.0; stepSize: 0.01; decimals: 2
                    value: advPage.mouseInertia
                    onValueChanged: {
                        advPage.mouseInertia = value
                        if (bhCore) bhCore.mouseInertia = value
                    }
                    visible: advPage.followMouse
                    implicitHeight: advPage.followMouse ? 48 : 0
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: advPage.followMouse

                    Text {
                        text: "限制冲出范围"
                        font.pixelSize: 14
                        color: theme.textColor
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "关闭后可被鼠标甩飞"
                        font.pixelSize: 11
                        color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                    }
                    CheckBox {
                        id: overshootCheck
                        checked: advPage.limitMouseOvershoot
                        onToggled: {
                            advPage.limitMouseOvershoot = checked
                            if (bhCore) bhCore.limitMouseOvershoot = checked
                        }
                        indicator: Rectangle {
                            implicitWidth: 20; implicitHeight: 20
                            x: overshootCheck.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 4
                            color: overshootCheck.checked ? theme.focusColor : "transparent"
                            border.color: overshootCheck.checked ? theme.focusColor : theme.borderColor
                            border.width: 2
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Text {
                                anchors.centerIn: parent; text: "\uf00c"
                                font.family: iconFont.name; font.pixelSize: 12
                                color: "#ffffff"; visible: overshootCheck.checked
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: "允许截屏/录屏捕获黑洞"
                        font.pixelSize: 14
                        color: theme.textColor
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "开启后冻结桌面纹理以避免递归残影"
                        font.pixelSize: 11
                        color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                    }
                    CheckBox {
                        id: recordingCaptureCheck
                        checked: advPage.allowRecordingCapture
                        onToggled: {
                            advPage.allowRecordingCapture = checked
                            if (bhCore) bhCore.allowRecordingCapture = checked
                        }
                        indicator: Rectangle {
                            implicitWidth: 20; implicitHeight: 20
                            x: recordingCaptureCheck.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 4
                            color: recordingCaptureCheck.checked ? theme.focusColor : "transparent"
                            border.color: recordingCaptureCheck.checked ? theme.focusColor : theme.borderColor
                            border.width: 2
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Text {
                                anchors.centerIn: parent; text: "\uf00c"
                                font.family: iconFont.name; font.pixelSize: 12
                                color: "#ffffff"; visible: recordingCaptureCheck.checked
                            }
                        }
                    }
                }
            }
        }

        // === 视觉效果 ===
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visualCol.implicitHeight + 24
            radius: 16
            color: theme.secondaryColor
            opacity: 0.85

            ColumnLayout {
                id: visualCol
                anchors.fill: parent
                anchors.margins: 20
                spacing: 4

                Text {
                    text: "\uf06e  视觉效果"
                    font.family: iconFont.name
                    font.pixelSize: 14
                    color: theme.focusColor
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.borderColor
                    opacity: 0.2
                }

                // 吞噬屏幕UI
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: "吞噬屏幕UI特效"
                        font.pixelSize: 14
                        color: theme.textColor
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "UI元素被黑洞引力吸入扭曲"
                        font.pixelSize: 11
                        color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                    }
                    CheckBox {
                        id: swallowCheck
                        checked: advPage.screenSwallow
                        onToggled: {
                            advPage.screenSwallow = checked
                            if (bhCore) bhCore.screenSwallow = checked
                        }
                        indicator: Rectangle {
                            implicitWidth: 20; implicitHeight: 20
                            x: swallowCheck.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 4
                            color: swallowCheck.checked ? theme.focusColor : "transparent"
                            border.color: swallowCheck.checked ? theme.focusColor : theme.borderColor
                            border.width: 2
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Text {
                                anchors.centerIn: parent; text: "\uf00c"
                                font.family: iconFont.name; font.pixelSize: 12
                                color: "#ffffff"; visible: swallowCheck.checked
                            }
                        }
                    }
                }

                Components.ESlider {
                    label: "吞噬强度"
                    from: 0.0; to: 1.0; stepSize: 0.01; decimals: 2
                    value: advPage.swallowStrength
                    onValueChanged: {
                        advPage.swallowStrength = value
                        if (bhCore) bhCore.swallowStrength = value
                    }
                    visible: advPage.screenSwallow
                    implicitHeight: advPage.screenSwallow ? 48 : 0
                }

                // 扭曲程度
                Components.ESlider {
                    label: "扭曲程度"
                    from: 0; to: 1; stepSize: 0.01; decimals: 2
                    value: advPage.distortion
                    onValueChanged: {
                        advPage.distortion = value
                        if (bhCore) bhCore.distortion = value
                    }
                }

                // 黑洞大小 + 大小模式
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Components.ESlider {
                        label: "黑洞大小"
                        from: 0.2; to: 3.0; stepSize: 0.1; decimals: 1
                        value: advPage.holeSize
                        onValueChanged: {
                            advPage.holeSize = value
                            if (bhCore) bhCore.holeSize = value
                        }
                        Layout.fillWidth: true
                    }

                    ColumnLayout {
                        spacing: 2
                        Layout.alignment: Qt.AlignBottom
                        Layout.bottomMargin: 4

                        Text {
                            text: "自定义渐变"
                            font.pixelSize: 11
                            color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.50)
                        }
                        CheckBox {
                            id: growCheck
                            checked: advPage.growEnabled
                            onToggled: {
                                advPage.growEnabled = checked
                                if (bhCore) bhCore.growEnabled = checked
                                // 互斥: 启用自定义渐变时关闭固定大小
                                if (checked && advPage.fixedSize) {
                                    advPage.fixedSize = false
                                    if (bhCore) bhCore.fixedSize = false
                                }
                            }
                            indicator: Rectangle {
                                implicitWidth: 20; implicitHeight: 20
                                x: growCheck.leftPadding
                                y: parent.height / 2 - height / 2
                                radius: 4
                                color: growCheck.checked ? theme.focusColor : "transparent"
                                border.color: growCheck.checked ? theme.focusColor : theme.borderColor
                                border.width: 2
                                Behavior on color { ColorAnimation { duration: 120 } }
                                Text {
                                    anchors.centerIn: parent; text: "\uf00c"
                                    font.family: iconFont.name; font.pixelSize: 12
                                    color: "#ffffff"; visible: growCheck.checked
                                }
                            }
                        }
                    }
                }

                Text {
                    text: "大小模式: 两者都不选时使用默认出生动画; 自定义渐变可设置初始大小; 固定大小会覆盖所有增长动画。"
                    font.pixelSize: 11
                    color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                // 初始大小(仅勾选自定义渐变时显示)
                Components.ESlider {
                    label: "初始大小"
                    from: 0.05; to: 1.5; stepSize: 0.05; decimals: 2
                    value: advPage.initialSize
                    onValueChanged: { advPage.initialSize = value; if (bhCore) bhCore.initialSize = value }
                    visible: advPage.growEnabled
                    implicitHeight: advPage.growEnabled ? 48 : 0
                }

                // === 固定大小 (与自定义渐变互斥; 对接 main.cpp uFixedSize/uFixedLevel) ===
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: "固定大小"
                        font.pixelSize: 14
                        color: theme.textColor
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "黑洞保持固定大小,不再随时间增长(覆盖自定义渐变和默认出生动画)"
                        font.pixelSize: 11
                        color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                    }
                    CheckBox {
                        id: fixedSizeCheck
                        checked: advPage.fixedSize
                        onToggled: {
                            advPage.fixedSize = checked
                            if (bhCore) bhCore.fixedSize = checked
                            // 互斥: 启用固定大小时关闭自定义渐变
                            if (checked && advPage.growEnabled) {
                                advPage.growEnabled = false
                                if (bhCore) bhCore.growEnabled = false
                            }
                        }
                        indicator: Rectangle {
                            implicitWidth: 20; implicitHeight: 20
                            x: fixedSizeCheck.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 4
                            color: fixedSizeCheck.checked ? theme.focusColor : "transparent"
                            border.color: fixedSizeCheck.checked ? theme.focusColor : theme.borderColor
                            border.width: 2
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Text {
                                anchors.centerIn: parent; text: "\uf00c"
                                font.family: iconFont.name; font.pixelSize: 12
                                color: "#ffffff"; visible: fixedSizeCheck.checked
                            }
                        }
                    }
                }

                // 固定大小级别(仅勾选固定大小时显示)
                Components.ESlider {
                    label: "固定大小级别"
                    from: 0.01; to: 1.0; stepSize: 0.01; decimals: 2
                    value: advPage.fixedLevel
                    onValueChanged: { advPage.fixedLevel = value; if (bhCore) bhCore.fixedLevel = value }
                    visible: advPage.fixedSize
                    implicitHeight: advPage.fixedSize ? 48 : 0
                }
            }
        }

        // === 路径与动画 ===
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: pathCol.implicitHeight + 24
            radius: 16
            color: theme.secondaryColor
            opacity: 0.85

            ColumnLayout {
                id: pathCol
                anchors.fill: parent
                anchors.margins: 20
                spacing: 4

                Text {
                    text: "\uf140  路径与动画"
                    font.family: iconFont.name
                    font.pixelSize: 14
                    color: theme.focusColor
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.borderColor
                    opacity: 0.2
                }

                // 路径随机化
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: "路径随机化"
                        font.pixelSize: 14
                        color: theme.textColor
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "每次启动使用随机移动轨迹"
                        font.pixelSize: 11
                        color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                    }
                    CheckBox {
                        id: pathCheck
                        checked: advPage.randomPath
                        onToggled: {
                            advPage.randomPath = checked
                            if (bhCore) bhCore.randomPath = checked
                        }
                        indicator: Rectangle {
                            implicitWidth: 20; implicitHeight: 20
                            x: pathCheck.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 4
                            color: pathCheck.checked ? theme.focusColor : "transparent"
                            border.color: pathCheck.checked ? theme.focusColor : theme.borderColor
                            border.width: 2
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Text {
                                anchors.centerIn: parent; text: "\uf00c"
                                font.family: iconFont.name; font.pixelSize: 12
                                color: "#ffffff"; visible: pathCheck.checked
                            }
                        }
                    }
                }

                // 动画速度
                Components.ESlider {
                    label: "动画速度"
                    from: 1; to: 10; stepSize: 1; decimals: 0
                    value: advPage.animationSpeed
                    onValueChanged: { advPage.animationSpeed = value; if (bhCore) bhCore.animationSpeed = value }
                }


            }
        }

        // === 捕获与显示 ===
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: captureCol.implicitHeight + 24
            radius: 16
            color: theme.secondaryColor
            opacity: 0.85

            ColumnLayout {
                id: captureCol
                anchors.fill: parent
                anchors.margins: 20
                spacing: 4

                Text {
                    text: "\uf030  捕获与显示"
                    font.family: iconFont.name
                    font.pixelSize: 14
                    color: theme.focusColor
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.borderColor
                    opacity: 0.2
                }

                // 捕获方式 (WGC/DXGI/自动; 对接 main.cpp cfg.captureMode)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: "捕获方式"
                        font.pixelSize: 14
                        color: theme.textColor
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "Win11 22H2+ 用 WGC,旧系统回退 DXGI"
                        font.pixelSize: 11
                        color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                    }
                    Components.EDropDown {
                        id: captureModeDrop
                        preferredWidth: 180
                        // UI 索引 0=自动检测 1=WGC 2=DXGI; cfg.captureMode -1=auto 0=WGC 1=DXGI
                        model: ["自动检测", "WGC (Win11 22H2+)", "DXGI (兼容 Win10)"]
                        currentIndex: (advPage.captureMode === 0) ? 1 : (advPage.captureMode === 1) ? 2 : 0
                        onActivated: function(index) {
                            var m = (index === 1) ? 0 : (index === 2) ? 1 : -1
                            advPage.captureMode = m
                            if (bhCore) bhCore.captureMode = m
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
