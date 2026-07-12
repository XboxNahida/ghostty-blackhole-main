import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "." as Components

Item {
    id: dialogRoot
    anchors.fill: parent
    visible: false
    z: 2100

    property var theme
    property var checker

    function open() {
        visible = true
        overlay.opacity = 1.0
        card.opacity = 1.0
        card.scale = 1.0
    }

    function close() {
        visible = false
    }

    Rectangle {
        id: overlay
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.48)
        opacity: 0

        Behavior on opacity { NumberAnimation { duration: 180 } }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            onClicked: dialogRoot.close()
            onWheel: function(wheel) { wheel.accepted = true }
        }
    }

    Components.EBlurCard {
        id: card
        width: Math.min(560, dialogRoot.width - 64)
        height: Math.min(600, Math.max(300, contentLayout.implicitHeight + 40))
        anchors.centerIn: parent
        blurSource: dialogRoot.parent
        borderRadius: 16
        opacity: 0
        scale: 0.96

        Behavior on opacity { NumberAnimation { duration: 180 } }
        Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        MouseArea {
            id: cardInputBlocker
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            onPressed: function(mouse) { mouse.accepted = true }
            onReleased: function(mouse) { mouse.accepted = true }
            onClicked: function(mouse) { mouse.accepted = true }
            onWheel: function(wheel) { wheel.accepted = true }
        }

        ColumnLayout {
            id: contentLayout
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: checker && checker.updateAvailable ? "发现新版本" : "检查更新"
                wrapMode: Text.WordWrap
                font.pixelSize: 21
                font.bold: true
                color: dialogRoot.theme ? dialogRoot.theme.focusColor : "#00C4B3"
            }

            Text {
                Layout.fillWidth: true
                text: checker ? checker.statusText : ""
                wrapMode: Text.WordWrap
                font.pixelSize: 14
                color: dialogRoot.theme ? dialogRoot.theme.textColor : "#ffffff"
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 16
                rowSpacing: 8

                Text {
                    text: "当前版本"
                    font.pixelSize: 13
                    color: dialogRoot.theme ? Qt.rgba(dialogRoot.theme.textColor.r, dialogRoot.theme.textColor.g, dialogRoot.theme.textColor.b, 0.58) : "#999999"
                }
                Text {
                    Layout.fillWidth: true
                    text: "v" + Qt.application.version
                    wrapMode: Text.WrapAnywhere
                    font.pixelSize: 13
                    color: dialogRoot.theme ? dialogRoot.theme.textColor : "#ffffff"
                }

                Text {
                    visible: checker && checker.updateAvailable
                    text: "最新版本"
                    font.pixelSize: 13
                    color: dialogRoot.theme ? Qt.rgba(dialogRoot.theme.textColor.r, dialogRoot.theme.textColor.g, dialogRoot.theme.textColor.b, 0.58) : "#999999"
                }
                Text {
                    visible: checker && checker.updateAvailable
                    Layout.fillWidth: true
                    text: checker ? checker.latestVersion : ""
                    wrapMode: Text.WrapAnywhere
                    font.pixelSize: 13
                    color: dialogRoot.theme ? dialogRoot.theme.focusColor : "#00C4B3"
                }
            }

            Text {
                Layout.fillWidth: true
                visible: checker && checker.updateAvailable && checker.latestName.length > 0
                text: checker ? checker.latestName : ""
                wrapMode: Text.WordWrap
                font.pixelSize: 15
                font.bold: true
                color: dialogRoot.theme ? dialogRoot.theme.textColor : "#ffffff"
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 100
                visible: checker && checker.updateAvailable
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                Text {
                    id: releaseNotesText
                    width: parent.width
                    text: checker && checker.latestNotes.length > 0 ? checker.latestNotes : "暂无更新说明。"
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    font.pixelSize: 13
                    lineHeight: 1.25
                    color: dialogRoot.theme ? Qt.rgba(dialogRoot.theme.textColor.r, dialogRoot.theme.textColor.g, dialogRoot.theme.textColor.b, 0.78) : "#d0d0d0"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight
                spacing: 10

                Components.EButton {
                    visible: checker && checker.updateAvailable
                    size: "xs"
                    text: "忽略此版本"
                    iconCharacter: "\uf05e"
                    onClicked: {
                        checker.ignoreCurrentRelease()
                        dialogRoot.close()
                    }
                }

                Components.EButton {
                    visible: checker && checker.updateAvailable
                    size: "xs"
                    text: "下载"
                    iconCharacter: "\uf019"
                    onClicked: checker.openDownloadPage()
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
