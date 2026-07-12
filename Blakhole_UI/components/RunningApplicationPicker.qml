import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "." as Components

Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 2200

    signal applicationSelected(var application)
    signal refreshRequested()

    function open(applications) {
        applicationsModel.clear()
        for (var i = 0; i < applications.length; i++)
            applicationsModel.append(applications[i])
        searchInput.text = ""
        visible = true
    }

    function close() {
        visible = false
    }

    ListModel { id: applicationsModel }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.52)
        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    Rectangle {
        width: Math.min(760, root.width - 80)
        height: Math.min(620, root.height - 80)
        anchors.centerIn: parent
        radius: 8
        color: theme.primaryColor
        border.color: theme.borderColor
        border.width: 1

        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: false
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "选择正在运行的程序"
                    font.pixelSize: 18
                    font.bold: true
                    color: theme.focusColor
                }

                Item { Layout.fillWidth: true }

                Components.EButton {
                    text: "刷新"
                    iconCharacter: "\uf021"
                    size: "xs"
                    radius: 8
                    onClicked: root.refreshRequested()
                }

                Components.EButton {
                    text: ""
                    iconCharacter: "\uf00d"
                    size: "xs"
                    radius: 8
                    onClicked: root.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 38
                radius: 8
                color: theme.secondaryColor
                border.color: theme.borderColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Text {
                        text: "\uf002"
                        font.family: iconFont.name
                        font.pixelSize: 13
                        color: theme.textColor
                    }

                    TextInput {
                        id: searchInput
                        Layout.fillWidth: true
                        color: theme.textColor
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        selectByMouse: true
                        Text {
                            anchors.fill: parent
                            text: "搜索名称、进程名或路径"
                            color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.32)
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            visible: !searchInput.activeFocus && searchInput.text === ""
                        }
                    }
                }
            }

            ListView {
                id: applicationsView
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: applicationsModel
                clip: true
                spacing: 3

                delegate: Rectangle {
                    required property string displayName
                    required property string processName
                    required property string executablePath
                    required property string iconDataUrl

                    property string searchableText:
                        (displayName + " " + processName + " " + executablePath).toLowerCase()
                    property bool matchesSearch:
                        searchInput.text.trim() === "" ||
                        searchableText.indexOf(searchInput.text.trim().toLowerCase()) >= 0

                    width: applicationsView.width
                    height: matchesSearch ? 64 : 0
                    visible: matchesSearch
                    radius: 6
                    color: rowMouse.containsMouse
                        ? Qt.darker(theme.secondaryColor, 1.12)
                        : theme.secondaryColor
                    clip: true

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 10

                        Item {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36

                            Image {
                                anchors.fill: parent
                                source: iconDataUrl
                                fillMode: Image.PreserveAspectFit
                                visible: iconDataUrl !== ""
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "\uf2d0"
                                font.family: iconFont.name
                                font.pixelSize: 20
                                color: theme.focusColor
                                visible: iconDataUrl === ""
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: displayName
                                    font.pixelSize: 13
                                    font.bold: true
                                    color: theme.textColor
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: processName
                                    font.pixelSize: 11
                                    color: theme.focusColor
                                }
                            }

                            Text {
                                text: executablePath
                                font.pixelSize: 10
                                color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.48)
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.applicationSelected({
                                displayName: displayName,
                                processName: processName,
                                executablePath: executablePath,
                                iconDataUrl: iconDataUrl
                            })
                            root.close()
                        }
                    }
                }
            }

            Text {
                text: applicationsModel.count + " 个可选程序"
                font.pixelSize: 11
                color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.4)
                Layout.alignment: Qt.AlignRight
            }
        }
    }
}
