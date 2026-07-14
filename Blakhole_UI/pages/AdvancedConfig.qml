// AdvancedConfig.qml — settings consumed by the GNOME compositor path.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

Item {
    id: advPage

    anchors.fill: parent
    anchors.margins: 20

    property var bhCore: null

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: settingsColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: settingsColumn
            width: parent.width
            spacing: 16

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "\uf085"
                    font.family: iconFont.name
                    font.pixelSize: 22
                    color: theme.focusColor
                }
                Text {
                    text: "GNOME 高级设置"
                    font.pixelSize: 22
                    font.bold: true
                    color: theme.focusColor
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "仅显示当前合成器路径中真正生效的选项"
                    font.pixelSize: 12
                    color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: idleColumn.implicitHeight + 40
                radius: 16
                color: theme.secondaryColor
                opacity: 0.85

                ColumnLayout {
                    id: idleColumn
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Text {
                        text: "\uf028  媒体与空闲规则"
                        font.family: iconFont.name
                        font.pixelSize: 14
                        font.bold: true
                        color: theme.focusColor
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: theme.borderColor
                        opacity: 0.2
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                text: "媒体播放时仍允许空闲触发"
                                font.pixelSize: 14
                                color: theme.textColor
                            }
                            Text {
                                text: "关闭时，MPRIS 播放中会抑制黑洞；开启后忽略媒体播放状态"
                                font.pixelSize: 11
                                color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5)
                            }
                        }

                        Components.ESwitchButton {
                            size: "xs"
                            text: checked ? "已允许" : "已抑制"
                            checked: advPage.bhCore ? advPage.bhCore.videoAsIdle : false
                            onToggled: function(value) {
                                if (advPage.bhCore) advPage.bhCore.videoAsIdle = value
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: effectColumn.implicitHeight + 40
                radius: 16
                color: theme.secondaryColor
                opacity: 0.85

                ColumnLayout {
                    id: effectColumn
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Text {
                        text: "\uf06e  合成器效果"
                        font.family: iconFont.name
                        font.pixelSize: 14
                        font.bold: true
                        color: theme.focusColor
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: theme.borderColor
                        opacity: 0.2
                    }

                    Components.ESlider {
                        Layout.fillWidth: true
                        label: "黑洞半径系数"
                        from: 0.2
                        to: 3.0
                        stepSize: 0.1
                        decimals: 1
                        externalValue: advPage.bhCore ? advPage.bhCore.holeSize : 1.0
                        onUserChanged: function(value) {
                            if (advPage.bhCore) advPage.bhCore.holeSize = value
                        }
                    }

                    Components.ESlider {
                        Layout.fillWidth: true
                        label: "自由移动速度"
                        from: 0.1
                        to: 3.0
                        stepSize: 0.1
                        decimals: 1
                        externalValue: advPage.bhCore ? advPage.bhCore.movementSpeed : 1.0
                        onUserChanged: function(value) {
                            if (advPage.bhCore) advPage.bhCore.movementSpeed = value
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                text: "黑洞出现位置"
                                font.pixelSize: 14
                                color: theme.textColor
                            }
                            Text {
                                text: "随机模式会在每次启动效果时重新选择出生位置"
                                font.pixelSize: 11
                                color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5)
                            }
                        }

                        Components.EDropDown {
                            preferredWidth: 150
                            model: ["随机", "左上", "右上", "左下", "右下"]
                            currentIndex: advPage.bhCore ? advPage.bhCore.spawnPosition : 0
                            onActivated: function(index) {
                                if (advPage.bhCore) advPage.bhCore.spawnPosition = index
                            }
                        }
                    }

                    Components.ESlider {
                        Layout.fillWidth: true
                        label: "吸积盘旋转速度"
                        from: 1
                        to: 10
                        stepSize: 1
                        decimals: 0
                        externalValue: advPage.bhCore ? advPage.bhCore.animationSpeed : 1
                        onUserChanged: function(value) {
                            if (advPage.bhCore) advPage.bhCore.animationSpeed = Math.round(value)
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: theme.borderColor
                        opacity: 0.2
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                text: "固定黑洞大小"
                                font.pixelSize: 14
                                color: theme.textColor
                            }
                            Text {
                                text: fixedSizeSwitch.checked
                                    ? "锁定核心与透镜尺度；预设切换仍会改变吸积盘外观"
                                    : "40 秒长大、40 秒回缩，平滑往返循环"
                                font.pixelSize: 11
                                color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5)
                            }
                        }

                        Components.ESwitchButton {
                            id: fixedSizeSwitch
                            size: "xs"
                            text: checked ? "已固定" : "动态"
                            checked: advPage.bhCore ? advPage.bhCore.fixedSize : false
                            onToggled: function(value) {
                                if (advPage.bhCore) advPage.bhCore.fixedSize = value
                            }
                        }
                    }

                    Components.ESlider {
                        Layout.fillWidth: true
                        visible: advPage.bhCore ? advPage.bhCore.fixedSize : false
                        implicitHeight: visible ? 48 : 0
                        label: "固定大小级别"
                        from: 0.01
                        to: 1.0
                        stepSize: 0.01
                        decimals: 2
                        externalValue: advPage.bhCore ? advPage.bhCore.fixedLevel : 1.0
                        onUserChanged: function(value) {
                            if (advPage.bhCore) advPage.bhCore.fixedLevel = value
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: "更改后点击“启动黑洞”会保存配置并让 GNOME 扩展重新读取。"
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.55)
            }

            Item { Layout.preferredHeight: 4 }
        }
    }
}
