// BlackholeLargePreview.qml — 黑洞放大预览弹窗
import QtQuick
import QtQuick.Controls
import BlakholeUI 1.0

Popup {
    id: root

    Component.onCompleted: console.log("BlackholeLargePreview CREATED")
    Component.onDestruction: console.log("BlackholeLargePreview DESTROYED")

    width: 740
    height: 740
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // === 14 参数透传 ===
    property alias diskTemp:  fbo.diskTemp
    property alias diskIncl:  fbo.diskIncl
    property alias diskRoll:  fbo.diskRoll
    property alias diskInner: fbo.diskInner
    property alias diskOuter: fbo.diskOuter
    property alias diskOpac:  fbo.diskOpac
    property alias diskDopp:  fbo.diskDopp
    property alias diskBeam:  fbo.diskBeam
    property alias diskGain:  fbo.diskGain
    property alias diskContr: fbo.diskContr
    property alias diskWind:  fbo.diskWind
    property alias diskSpeed: fbo.diskSpeed
    property alias diskExpo:  fbo.diskExpo
    property alias diskStar:  fbo.diskStar

    background: Rectangle {
        radius: 16
        color: "#1a1a24"
        border.color: Qt.rgba(1,1,1,0.08)
        border.width: 1
    }

    contentItem: Item {
        anchors.fill: parent

        // === 黑洞实时渲染 (大) ===
        BlackholePreviewArea {
            id: fbo
            anchors.fill: parent
            anchors.margins: 20
            running: root.visible  // 关闭时停止渲染节省资源
        }

        // === 关闭按钮 ===
        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 8
            width: 36; height: 36
            radius: 18
            color: closeMa.containsMouse ? Qt.rgba(1,1,1,0.15) : Qt.rgba(1,1,1,0.05)

            Text {
                anchors.centerIn: parent
                text: "\u2715"
                font.pixelSize: 18
                color: "#aaa"
            }

            MouseArea {
                id: closeMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.close()
            }
        }

        // === 标题 ===
        Text {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 16
            text: "Black Hole Preview"
            font.pixelSize: 16
            font.bold: true
            color: "#888"
        }
    }

    // === 出入场动画 ===
    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0; to: 1
            duration: 200
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            property: "scale"
            from: 0.92; to: 1.0
            duration: 250
            easing.type: Easing.OutBack
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1; to: 0
            duration: 150
        }
        NumberAnimation {
            property: "scale"
            from: 1; to: 0.95
            duration: 150
        }
    }
}