// EAvatar.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Item {
    id: root
    width: 80
    height: 80

    property url avatarSource: "qrc:/new/prefix1/fonts/pic/avatar.png"

    Image {
        id: sourceItem
        source: root.avatarSource
        anchors.centerIn: parent
        width: root.width
        height: root.height
        fillMode: Image.PreserveAspectCrop
        visible: false
    }

    MultiEffect {
        id: multiEffect
        source: sourceItem
        anchors.fill: sourceItem
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }

    Item {
        id: mask
        width: sourceItem.width
        height: sourceItem.height
        layer.enabled: true
        visible: false

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "black"
        }
    }
}
