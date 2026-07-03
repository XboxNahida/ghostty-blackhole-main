// IdleListConfig.qml — 空闲检测名单管理
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Root
import "../components" as Components

Item {
    id: listPage

    anchors.fill: parent
    anchors.margins: 20

    property var bhCore: null

    property var whitelist: {
        if (bhCore && bhCore.idleWhitelist && bhCore.idleWhitelist.length > 0)
            return bhCore.idleWhitelist
        return []
    }
    property var blacklist: {
        if (bhCore && bhCore.idleBlacklist && bhCore.idleBlacklist.length > 0)
            return bhCore.idleBlacklist
        return ["vlc", "mpv", "potplayer", "mpc", "wmplayer", "bilibili", "iqiyi", "youku", "芒果TV", "腾讯视频"]
    }

    ListModel { id: whiteModel }
    ListModel { id: blackModel }

    Component.onCompleted: {
        refreshModels()
        // sync to bhCore if available
        if (bhCore) {
            bhCore.idleWhitelist = listPage.whitelist
            bhCore.idleBlacklist = listPage.blacklist
        }
    }

    function refreshModels() {
        whiteModel.clear()
        for (var i = 0; i < listPage.whitelist.length; i++) {
            whiteModel.append({ name: listPage.whitelist[i] })
        }
        blackModel.clear()
        for (var j = 0; j < listPage.blacklist.length; j++) {
            blackModel.append({ name: listPage.blacklist[j] })
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        // === 标题 ===
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "\uf502"
                font.family: iconFont.name
                font.pixelSize: 22
                color: theme.focusColor
            }
            Text {
                text: "空闲检测名单"
                font.pixelSize: 22
                font.bold: true
                color: theme.focusColor
            }
            Item { Layout.fillWidth: true }
        }

        // === 白名单 ===
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 16
            color: theme.secondaryColor
            opacity: 0.85

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                Text {
                    text: "\uf058  白名单 (视频空闲检测)"
                    font.family: iconFont.name
                    font.pixelSize: 14
                    color: theme.focusColor
                    font.bold: true
                }
                Text {
                    text: "检测到以下应用时视为空闲，不阻止黑洞显示"
                    font.pixelSize: 11
                    color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                // 输入行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        height: 34
                        radius: 8
                        color: theme.primaryColor
                        border.color: theme.borderColor
                        border.width: 1

                        TextInput {
                            id: whiteInput
                            anchors.fill: parent
                            anchors.margins: 8
                            color: theme.textColor
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            Text {
                                anchors.fill: parent
                                text: "输入进程名..."
                                color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.30)
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                                visible: !whiteInput.activeFocus && whiteInput.text === ""
                            }
                        }
                    }

                    Components.EButton {
                        text: "添加"
                        size: "xs"
                        iconCharacter: "\uf067"
                        radius: 8
                        implicitHeight: 34
                        onClicked: {
                            var t = whiteInput.text.trim()
                            if (t !== "" && listPage.whitelist.indexOf(t) < 0) {
                                listPage.whitelist.push(t)
                                refreshModels()
                                whiteInput.text = ""
                                if (bhCore) bhCore.idleWhitelist = listPage.whitelist
                            }
                        }
                    }
                }

                // 列表
                ListView {
                    id: whiteListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: whiteModel
                    clip: true
                    spacing: 2

                    delegate: Rectangle {
                        width: whiteListView.width
                        height: 32
                        radius: 6
                        color: itemMouse.containsMouse ? Qt.darker(theme.secondaryColor, 1.15) : "transparent"

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            text: model.name
                            font.pixelSize: 13
                            color: theme.textColor
                        }

                        Components.EButton {
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            size: "xs"
                            text: ""
                            iconCharacter: "\uf00d"
                            radius: 6
                            implicitHeight: 26
                            containerColor: "transparent"
                            textColor: "#ff5555"
                            iconColor: "#ff5555"
                            onClicked: {
                                var idx = listPage.whitelist.indexOf(model.name)
                                if (idx >= 0) listPage.whitelist.splice(idx, 1)
                                refreshModels()
                                if (bhCore) bhCore.idleWhitelist = listPage.whitelist
                            }
                        }

                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }

                Text {
                    text: whiteModel.count + " 条"
                    font.pixelSize: 11
                    color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.35)
                    Layout.alignment: Qt.AlignRight
                }
            }
        }

        // === 黑名单 ===
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 16
            color: theme.secondaryColor
            opacity: 0.85

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                Text {
                    text: "\uf057  黑名单 (始终阻止)"
                    font.family: iconFont.name
                    font.pixelSize: 14
                    color: "#ff6b6b"
                    font.bold: true
                }
                Text {
                    text: "检测到以下应用时始终阻止黑洞显示"
                    font.pixelSize: 11
                    color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                // 输入行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        height: 34
                        radius: 8
                        color: theme.primaryColor
                        border.color: theme.borderColor
                        border.width: 1

                        TextInput {
                            id: blackInput
                            anchors.fill: parent
                            anchors.margins: 8
                            color: theme.textColor
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            Text {
                                anchors.fill: parent
                                text: "输入进程名..."
                                color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.30)
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                                visible: !blackInput.activeFocus && blackInput.text === ""
                            }
                        }
                    }

                    Components.EButton {
                        text: "添加"
                        size: "xs"
                        iconCharacter: "\uf067"
                        radius: 8
                        implicitHeight: 34
                        onClicked: {
                            var t = blackInput.text.trim()
                            if (t !== "" && listPage.blacklist.indexOf(t) < 0) {
                                listPage.blacklist.push(t)
                                refreshModels()
                                blackInput.text = ""
                                if (bhCore) bhCore.idleBlacklist = listPage.blacklist
                            }
                        }
                    }
                }

                // 列表
                ListView {
                    id: blackListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: blackModel
                    clip: true
                    spacing: 2

                    delegate: Rectangle {
                        width: blackListView.width
                        height: 32
                        radius: 6
                        color: itemMouse2.containsMouse ? Qt.darker(theme.secondaryColor, 1.15) : "transparent"

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            text: model.name
                            font.pixelSize: 13
                            color: theme.textColor
                        }

                        Components.EButton {
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            size: "xs"
                            text: ""
                            iconCharacter: "\uf00d"
                            radius: 6
                            implicitHeight: 26
                            containerColor: "transparent"
                            textColor: "#ff5555"
                            iconColor: "#ff5555"
                            onClicked: {
                                var idx = listPage.blacklist.indexOf(model.name)
                                if (idx >= 0) listPage.blacklist.splice(idx, 1)
                                refreshModels()
                                if (bhCore) bhCore.idleBlacklist = listPage.blacklist
                            }
                        }

                        MouseArea {
                            id: itemMouse2
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }

                Text {
                    text: blackModel.count + " 条"
                    font.pixelSize: 11
                    color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.35)
                    Layout.alignment: Qt.AlignRight
                }
            }
        }
    }
}
