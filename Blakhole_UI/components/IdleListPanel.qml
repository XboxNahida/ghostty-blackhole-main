import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "." as Components

Rectangle {
    id: root

    property string title: ""
    property string description: ""
    property color accentColor: theme.focusColor
    property var values: []

    signal manualAddRequested(string processName)
    signal runningPickerRequested()
    signal executablePickerRequested()
    signal removeRequested(string processName)

    radius: 8
    color: theme.secondaryColor
    opacity: 0.85

    function refreshModel() {
        entriesModel.clear()
        for (var i = 0; i < root.values.length; i++)
            entriesModel.append({ processName: String(root.values[i]) })
    }

    onValuesChanged: refreshModel()
    Component.onCompleted: refreshModel()

    ListModel { id: entriesModel }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        Text {
            text: root.title
            font.pixelSize: 14
            font.bold: true
            color: root.accentColor
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Text {
            text: root.description
            font.pixelSize: 11
            color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.48)
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Components.EButton {
                text: "运行中"
                iconCharacter: "\uf2d0"
                size: "xs"
                radius: 8
                Layout.fillWidth: true
                onClicked: root.runningPickerRequested()
            }

            Components.EButton {
                text: "选择 EXE"
                iconCharacter: "\uf07c"
                size: "xs"
                radius: 8
                Layout.fillWidth: true
                onClicked: root.executablePickerRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                height: 34
                radius: 8
                color: theme.primaryColor
                border.color: theme.borderColor
                border.width: 1

                TextInput {
                    id: manualInput
                    anchors.fill: parent
                    anchors.margins: 8
                    color: theme.textColor
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    selectByMouse: true
                    onAccepted: submitManual()

                    function submitManual() {
                        var value = text.trim()
                        if (value === "") return
                        root.manualAddRequested(value)
                        text = ""
                    }

                    Text {
                        anchors.fill: parent
                        text: "手动输入进程名..."
                        color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.30)
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        visible: !manualInput.activeFocus && manualInput.text === ""
                    }
                }
            }

            Components.EButton {
                text: ""
                iconCharacter: "\uf067"
                size: "xs"
                radius: 8
                implicitHeight: 34
                onClicked: manualInput.submitManual()
            }
        }

        ListView {
            id: entriesView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: entriesModel
            clip: true
            spacing: 2

            delegate: Rectangle {
                required property string processName
                width: entriesView.width
                height: 32
                radius: 6
                color: itemMouse.containsMouse
                    ? Qt.darker(theme.secondaryColor, 1.15)
                    : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.right: removeButton.left
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    text: processName
                    elide: Text.ElideMiddle
                    font.pixelSize: 13
                    color: theme.textColor
                }

                Components.EButton {
                    id: removeButton
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    text: ""
                    iconCharacter: "\uf00d"
                    size: "xs"
                    radius: 6
                    implicitHeight: 26
                    containerColor: "transparent"
                    shadowEnabled: false
                    iconColor: "#ff5555"
                    onClicked: root.removeRequested(processName)
                }

                MouseArea {
                    id: itemMouse
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                }
            }
        }

        Text {
            text: entriesModel.count + " 条"
            font.pixelSize: 11
            color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.35)
            Layout.alignment: Qt.AlignRight
        }
    }
}
