// BlackholePreviewFBO.qml — 真实 Shader 黑洞预览 (QQuickFramebufferObject)
// 替代原 Canvas 模拟，支持点击放大
import QtQuick
import QtQuick.Controls
import BlakholeUI 1.0

Item {
    id: root

    Component.onCompleted: console.log("BlackholePreviewArea CREATED")
    Component.onDestruction: console.log("BlackholePreviewArea DESTROYED")

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
    property alias running:  fbo.running

    // 点击放大信号
    signal enlargeRequested()

    // === 黑洞实时渲染 ===
    BlackholePreviewFBO {
        id: fbo
        anchors.fill: parent
        running: true
    }

    // === 点击区域 ===
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.enlargeRequested()
    }

    // === 悬浮提示 ===
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 6
        width: hintText.implicitWidth + 16
        height: hintText.implicitHeight + 10
        radius: 10
        color: Qt.rgba(0, 0, 0, 0.55)
        opacity: hoverMa.containsMouse ? 1.0 : 0.0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        Text {
            id: hintText
            anchors.centerIn: parent
            text: "\u25a1 \u70b9\u51fb\u653e\u5927"
            // font auto-detected
            font.pixelSize: 12
            color: "#ffffff"
        }

        MouseArea {
            id: hoverMa
            anchors.fill: parent
            hoverEnabled: true
            // 透传点击给外层 MouseArea
            onClicked: root.enlargeRequested()
        }
    }

    // === 加载占位 (Shader 编译期间) ===
    Rectangle {
        anchors.centerIn: parent
        width: 120; height: 30
        radius: 8
        color: Qt.rgba(0,0,0,0.6)
        visible: false  // FBO 启动很快，通常不需要
        Text {
            anchors.centerIn: parent
            text: "\u52a0\u8f7d\u4e2d..."
            font.pixelSize: 13
            color: "#aaa"
        }
    }
}