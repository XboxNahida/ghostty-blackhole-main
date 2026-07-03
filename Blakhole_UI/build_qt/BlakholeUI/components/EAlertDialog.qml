import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "." as Components

Item {
    id: dialogRoot
    anchors.fill: parent
    visible: false
    z: 2000

    // API
    property alias title: titleText.text
    property alias message: messageText.text
    property string cancelText: "Cancel"
    property string confirmText: "Continue"
    property string extraText: ""           // 第三个按钮文字,为空则隐藏
    property string checkboxText: ""        // 复选框文字,为空则隐藏
    property bool checkboxChecked: false
    property bool dismissOnOverlay: true
    signal confirm()
    signal cancel()
    signal extra()                          // 第三个按钮信号
    signal checkboxToggled(bool checked)

    function open() { visible = true; overlay.opacity = 0.0; overlay.opacity = 1.0; card.scale = 0.92; card.opacity = 0.0; card.scale = 1.0; card.opacity = 1.0 }
    function close() { visible = false }

    // 背景变暗遮罩
    Rectangle {
        id: overlay
        anchors.fill: parent
        color: Qt.rgba(0,0,0,0.45)
        opacity: 0
        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        MouseArea { 
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            propagateComposedEvents: false
            hoverEnabled: true
            onClicked: if (dialogRoot.dismissOnOverlay) dialogRoot.close()
            onPressed: function(mouse) { mouse.accepted = true }
            onReleased: function(mouse) { mouse.accepted = true }
            onDoubleClicked: function(mouse) { mouse.accepted = true }
            onWheel: function(wheel) { wheel.accepted = true }
        }
    }

    // 毛玻璃对话框主体
    Components.EBlurCard {
        id: card
        width: 420
        height: contentCol.implicitHeight + 28
        anchors.centerIn: parent
        blurSource: dialogRoot.parent
        borderRadius: 18
        opacity: 0
        scale: 0.96
        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutBack } }

        ColumnLayout {
            id: contentCol
            anchors.fill: parent
            anchors.margins: 18
            spacing: 16

            Text {
                id: titleText
                text: "Title"
                font.pixelSize: 20
                font.bold: true
                color: theme ? theme.focusColor : "#ffffff"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Text {
                id: messageText
                text: "Message"
                font.pixelSize: 14
                color: theme ? theme.textColor : "#d0d0d0"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // 复选框
            CheckBox {
                id: checkBox
                visible: dialogRoot.checkboxText !== ""
                text: dialogRoot.checkboxText
                checked: dialogRoot.checkboxChecked
                Layout.fillWidth: true

                contentItem: Text {
                    text: checkBox.text
                    font.pixelSize: 13
                    color: theme ? Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.7) : "#aaaaaa"
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 28
                }

                indicator: Rectangle {
                    implicitWidth: 20
                    implicitHeight: 20
                    x: checkBox.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: 4
                    color: checkBox.checked ? (theme ? theme.focusColor : "#00C4B3") : "transparent"
                    border.color: checkBox.checked
                        ? (theme ? theme.focusColor : "#00C4B3")
                        : (theme ? theme.borderColor : "#666666")
                    border.width: 2

                    Behavior on color { ColorAnimation { duration: 120 } }

                    Text {
                        anchors.centerIn: parent
                        text: "\uf00c"
                        font.family: iconFont.name
                        font.pixelSize: 12
                        color: "#ffffff"
                        visible: checkBox.checked
                    }
                }

                onToggled: dialogRoot.checkboxToggled(checkBox.checked)
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight
                spacing: 10

                // 第三个按钮(如"最小化")
                Components.EButton {
                    text: dialogRoot.extraText
                    visible: dialogRoot.extraText !== ""
                    backgroundVisible: true
                    iconCharacter: "\uf068"
                    onClicked: { dialogRoot.extra(); dialogRoot.close() }
                }

                Components.EButton {
                    text: cancelText
                    backgroundVisible: true
                    iconCharacter: "\uf00d"
                    onClicked: { dialogRoot.cancel(); dialogRoot.close() }
                }
                Components.EButton {
                    text: confirmText
                    backgroundVisible: true
                    iconCharacter: "\uf00c"
                    onClicked: { dialogRoot.confirm(); dialogRoot.close() }
                }
            }
        }
    }
}
