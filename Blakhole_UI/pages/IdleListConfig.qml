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
        if (bhCore)
            return bhCore.idleWhitelist
        return []
    }
    property var blacklist: {
        if (bhCore)
            return bhCore.idleBlacklist
        return ["vlc", "mpv", "potplayer", "mpc", "wmplayer", "bilibili", "iqiyi", "youku", "芒果TV", "腾讯视频"]
    }
    property var forceBlocklist: {
        if (bhCore)
            return bhCore.idleForceBlocklist
        return []
    }

    ListModel { id: whiteModel }
    ListModel { id: blackModel }
    ListModel { id: forceModel }

    Component.onCompleted: {
        refreshModels()
        // sync to bhCore if available
        if (bhCore) {
            bhCore.idleWhitelist = listPage.whitelist
            bhCore.idleBlacklist = listPage.blacklist
            bhCore.idleForceBlocklist = listPage.forceBlocklist
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
        forceModel.clear()
        for (var k = 0; k < listPage.forceBlocklist.length; k++) {
            forceModel.append({ name: listPage.forceBlocklist[k] })
        }
    }

    function containsIgnoreCase(values, candidate) {
        var lowerCandidate = candidate.toLowerCase()
        for (var i = 0; i < values.length; i++) {
            if (String(values[i]).toLowerCase() === lowerCandidate)
                return true
        }
        return false
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

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // === 始终允许触发 ===
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 8
                color: theme.secondaryColor
                opacity: 0.85

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                Text {
                    text: "\uf058  始终允许触发"
                    font.family: iconFont.name
                    font.pixelSize: 14
                    color: theme.focusColor
                    font.bold: true
                }
                Text {
                    text: "前台命中时忽略媒体与游戏检测"
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
                            if (t !== "" && !containsIgnoreCase(listPage.whitelist, t)) {
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
                            anchors.right: removeWhiteButton.left
                            anchors.rightMargin: 4
                            text: model.name
                            elide: Text.ElideMiddle
                            font.pixelSize: 13
                            color: theme.textColor
                        }

                        Components.EButton {
                            id: removeWhiteButton
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

            // === 媒体识别提示 ===
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 8
                color: theme.secondaryColor
                opacity: 0.85

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                Text {
                    text: "\uf144  媒体识别提示"
                    font.family: iconFont.name
                    font.pixelSize: 14
                    color: theme.focusColor
                    font.bold: true
                }
                Text {
                    text: "仅在实际播放且检测到媒体信号时阻止"
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
                            if (t !== "" && !containsIgnoreCase(listPage.blacklist, t)) {
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
                            anchors.right: removeMediaButton.left
                            anchors.rightMargin: 4
                            text: model.name
                            elide: Text.ElideMiddle
                            font.pixelSize: 13
                            color: theme.textColor
                        }

                        Components.EButton {
                            id: removeMediaButton
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

            // === 前台强制不触发 ===
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 8
                color: theme.secondaryColor
                opacity: 0.85

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                    Text {
                        text: "\uf05e  前台强制不触发"
                        font.family: iconFont.name
                        font.pixelSize: 14
                        color: "#ff6b6b"
                        font.bold: true
                    }
                    Text {
                        text: "前台命中时无条件阻止，切到后台后失效"
                        font.pixelSize: 11
                        color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.45)
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

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
                                id: forceInput
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
                                    visible: !forceInput.activeFocus && forceInput.text === ""
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
                                var t = forceInput.text.trim()
                                if (t !== "" && !containsIgnoreCase(listPage.forceBlocklist, t)) {
                                    listPage.forceBlocklist.push(t)
                                    refreshModels()
                                    forceInput.text = ""
                                    if (bhCore) bhCore.idleForceBlocklist = listPage.forceBlocklist
                                }
                            }
                        }
                    }

                    ListView {
                        id: forceListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: forceModel
                        clip: true
                        spacing: 2

                        delegate: Rectangle {
                            width: forceListView.width
                            height: 32
                            radius: 6
                            color: itemMouse3.containsMouse ? Qt.darker(theme.secondaryColor, 1.15) : "transparent"

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: removeForceButton.left
                                anchors.rightMargin: 4
                                text: model.name
                                elide: Text.ElideMiddle
                                font.pixelSize: 13
                                color: theme.textColor
                            }

                            Components.EButton {
                                id: removeForceButton
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
                                    var idx = listPage.forceBlocklist.indexOf(model.name)
                                    if (idx >= 0) listPage.forceBlocklist.splice(idx, 1)
                                    refreshModels()
                                    if (bhCore) bhCore.idleForceBlocklist = listPage.forceBlocklist
                                }
                            }

                            MouseArea {
                                id: itemMouse3
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                        }
                    }

                    Text {
                        text: forceModel.count + " 条"
                        font.pixelSize: 11
                        color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.35)
                        Layout.alignment: Qt.AlignRight
                    }
                }
            }
        }
    }
}
