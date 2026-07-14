import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "." as Components

Item {
    id: dialogRoot
    anchors.fill: parent
    visible: false
    z: 2200

    property var theme
    property var core
    property string failureTitle: ""
    property string failureSummary: ""
    property string failureDetails: ""
    property string failureLogPath: ""

    function showFailure(title, summary, details, logPath) {
        failureTitle = title
        failureSummary = summary
        failureDetails = details
        failureLogPath = logPath
        visible = true
    }

    function close() {
        visible = false
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.52)

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            onPressed: function(mouse) { mouse.accepted = true }
            onReleased: function(mouse) { mouse.accepted = true }
            onClicked: function(mouse) { mouse.accepted = true }
            onWheel: function(wheel) { wheel.accepted = true }
        }
    }

    Components.EBlurCard {
        id: card
        width: Math.min(680, parent.width - 64)
        height: Math.min(620, parent.height - 64)
        anchors.centerIn: parent
        blurSource: dialogRoot.parent
        borderRadius: 16

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            onPressed: function(mouse) { mouse.accepted = true }
            onReleased: function(mouse) { mouse.accepted = true }
            onClicked: function(mouse) { mouse.accepted = true }
            onWheel: function(wheel) { wheel.accepted = true }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: dialogRoot.failureTitle
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                font.pixelSize: 21
                font.bold: true
                color: dialogRoot.theme ? dialogRoot.theme.focusColor : "#00C4B3"
            }

            Text {
                Layout.fillWidth: true
                text: dialogRoot.failureSummary
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                font.pixelSize: 14
                color: dialogRoot.theme ? dialogRoot.theme.textColor : "#ffffff"
            }

            Text {
                Layout.fillWidth: true
                visible: dialogRoot.failureLogPath.length > 0
                text: "日志：" + dialogRoot.failureLogPath
                wrapMode: Text.WrapAnywhere
                font.pixelSize: 12
                color: dialogRoot.theme
                       ? Qt.rgba(dialogRoot.theme.textColor.r,
                                 dialogRoot.theme.textColor.g,
                                 dialogRoot.theme.textColor.b, 0.62)
                       : "#aaaaaa"
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 120
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                TextArea {
                    id: detailsText
                    width: parent.width
                    text: dialogRoot.failureDetails
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    font.family: "monospace"
                    font.pixelSize: 12
                    color: dialogRoot.theme ? dialogRoot.theme.textColor : "#ffffff"
                    selectionColor: dialogRoot.theme ? dialogRoot.theme.focusColor : "#00C4B3"
                    selectedTextColor: "#ffffff"
                    padding: 12

                    background: Rectangle {
                        radius: 8
                        color: dialogRoot.theme
                               ? Qt.rgba(dialogRoot.theme.secondaryColor.r,
                                         dialogRoot.theme.secondaryColor.g,
                                         dialogRoot.theme.secondaryColor.b, 0.72)
                               : "#242424"
                        border.width: 1
                        border.color: dialogRoot.theme ? dialogRoot.theme.borderColor : "#555555"
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight
                spacing: 10

                Components.EButton {
                    size: "xs"
                    text: "复制详情"
                    iconCharacter: "\uf0c5"
                    onClicked: {
                        detailsText.selectAll()
                        detailsText.copy()
                        detailsText.deselect()
                    }
                }

                Components.EButton {
                    visible: dialogRoot.failureLogPath.length > 0
                    size: "xs"
                    text: "打开日志目录"
                    iconCharacter: "\uf07c"
                    onClicked: if (core) core.openRendererLogDirectory()
                }

                Components.EButton {
                    size: "xs"
                    text: "关闭"
                    iconCharacter: "\uf00d"
                    onClicked: dialogRoot.close()
                }
            }
        }
    }
}
