// EButton.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: root

    // ==== 外部接口 ====
    property string text: "Button"
    property string iconCharacter: ""
    property string iconFontFamily: iconFont.name
    signal clicked

    // ==== 样式 ====
    property bool backgroundVisible: true
    property real radius: 20
    property color containerColor: theme.secondaryColor
    property color hoverColor: Qt.darker(containerColor, 1.2)
    property color textColor: theme.textColor
    property color iconColor: theme.textColor
    property string size: "m"
    property bool shadowEnabled: true
    property real pressedScale: 0.96
    property color shadowColor: theme.shadowColor
    property bool iconRotateOnClick: false
    property bool textShown: root.text !== ""
    readonly property bool hasIcon: root.iconCharacter !== ""
    property int labelSpacing: md3.current.spacing

    QtObject {
        id: md3
        property var tokens: ({
            xs: { height: 32, padding: 12, fontsize: 12, spacing: 4 },
            s:  { height: 40, padding: 16, fontsize: 16, spacing: 8 },
            m:  { height: 56, padding: 24, fontsize: 20, spacing: 8 },
            l:  { height: 96, padding: 48, fontsize: 24, spacing: 12 },
            xl: { height: 136, padding: 64, fontsize: 32, spacing: 16 }
        })
        readonly property var current: tokens[root.size] || tokens.m
    }

    // ==== 尺寸计算 ====
    readonly property real contentScale: 0.4
    readonly property int iconSize: md3.current.fontsize
    readonly property int fontSize: iconSize

    // ==== 尺寸控制 ====
    readonly property int paddingLeft: md3.current.padding
    readonly property int paddingRight: md3.current.padding
    implicitHeight: md3.current.height
    implicitWidth: root.textShown ? (layout.implicitWidth + paddingLeft + paddingRight) : implicitHeight

    color: "transparent"

    transform: Scale {
        id: scale
        origin.x: root.width / 2
        origin.y: root.height / 2
    }

    // ==== 阴影效果 ====
    MultiEffect {
        source: background
        anchors.fill: background
        visible: root.backgroundVisible && root.shadowEnabled
        shadowEnabled: root.shadowEnabled
        shadowColor: root.shadowColor
        shadowBlur: theme.shadowBlur
        shadowHorizontalOffset: theme.shadowXOffset
        shadowVerticalOffset: theme.shadowYOffset
    }

    // ==== 背景 ====
    Rectangle {
        id: background
        anchors.fill: parent
        radius: root.radius
        color: mouseArea.containsMouse ? root.hoverColor : root.containerColor
        visible: root.backgroundVisible

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on opacity { NumberAnimation { duration: 100 } }
    }

    // ==== 内容布局 ====
    RowLayout {
        id: layout
        anchors.centerIn: parent
        spacing: (root.textShown && root.hasIcon) ? root.labelSpacing : 0
        Behavior on spacing { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        Text {
            id: iconLabel
            text: root.iconCharacter
            visible: root.iconCharacter !== ""
            color: root.iconColor
            font.pixelSize: root.iconSize
            font.family: root.iconFontFamily
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            Layout.preferredWidth: iconLabel.implicitWidth
            Layout.preferredHeight: iconSize

            transform: Rotation {
                id: iconRotation
                origin.x: iconLabel.width / 2
                origin.y: iconLabel.height / 2
                angle: 0
            }

            PropertyAnimation {
                id: rotateAnimation
                target: iconRotation
                property: "angle"
                from: 0
                to: 360
                duration: 200
                easing.type: Easing.InOutQuad
            }

            SpringAnimation {
                id: restoreRotation
                target: iconRotation
                property: "angle"
                to: 0
                spring: 3
                damping: 0.3
            }
        }

        Item {
            id: labelWrap
            width: root.textShown ? label.implicitWidth : 0
            height: label.implicitHeight
            Layout.preferredWidth: width
            Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
            clip: true

            Text {
                id: label
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                opacity: root.textShown ? 1 : 0
                color: root.textColor
                font.pixelSize: root.fontSize
                font.bold: true
                verticalAlignment: Text.AlignVCenter
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
            }
        }
    }

    // ==== 交互与动画 ====
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onPressed: {
            scale.xScale = root.pressedScale
            scale.yScale = root.pressedScale
            background.opacity = 0.85

            if (root.iconRotateOnClick) {
                rotateAnimation.start()
            }
        }

        onReleased: {
            restoreAnimation.restart()
            background.opacity = 1.0
            root.clicked()

            if (root.iconRotateOnClick) {
                restoreRotation.start()
            }
        }

        onCanceled: {
            restoreAnimation.restart()
            background.opacity = 1.0

            if (root.iconRotateOnClick) {
                restoreRotation.start()
            }
        }
    }

    ParallelAnimation {
        id: restoreAnimation
        SpringAnimation { target: scale; property: "xScale"; to: 1.0; spring: 2.5; damping: 0.25 }
        SpringAnimation { target: scale; property: "yScale"; to: 1.0; spring: 2.5; damping: 0.25 }
    }
}
