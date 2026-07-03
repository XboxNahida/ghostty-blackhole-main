// EToast.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Item {
    id: root
    property var theme
    property real yOffset: 0
    width: 300
    height: 48

    function show(message) {
        label.text = message
        toastAnimation.restart()
    }

    Rectangle {
        id: toastBg
        anchors.fill: parent
        radius: 24
        color: root.theme ? root.theme.secondaryColor : "#333333"
        opacity: 0
        scale: 0.8

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 1.0
            shadowHorizontalOffset: 2
            shadowVerticalOffset: 2
        }

        Text {
            id: label
            anchors.centerIn: parent
            color: root.theme ? root.theme.textColor : "#ffffff"
            font.pixelSize: 14
        }
    }

    SequentialAnimation {
        id: toastAnimation

        ParallelAnimation {
            NumberAnimation { target: toastBg; property: "opacity"; to: 1.0; duration: 200 }
            NumberAnimation { target: toastBg; property: "scale"; to: 1.0; duration: 200; easing.type: Easing.OutBack }
        }

        PauseAnimation { duration: 1500 }

        ParallelAnimation {
            NumberAnimation { target: toastBg; property: "opacity"; to: 0.0; duration: 300 }
            NumberAnimation { target: toastBg; property: "scale"; to: 0.8; duration: 300 }
        }
    }
}
