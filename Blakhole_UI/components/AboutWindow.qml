import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "." as Components

Window {
    id: root
    title: "关于 Blakhole UI"
    width: 720
    height: 760
    minimumWidth: 520
    minimumHeight: 560
    visible: false
    flags: Qt.Window

    property var theme
    property var bhCore
    readonly property string defaultAvatar: "qrc:/new/prefix1/fonts/pic/avatar.png"

    function openAndActivate() {
        show()
        raise()
        requestActivate()
    }

    onClosing: function(close) {
        close.accepted = false
        hide()
    }

    color: theme ? theme.primaryColor : "#1d1d1d"

    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.margins: 24
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: Math.max(scrollView.availableWidth, 460)
            spacing: 18

            Components.EAvatar {
                id: aboutAvatar
                width: 112
                height: 112
                Layout.alignment: Qt.AlignHCenter
                clickable: true
                avatarSource: root.bhCore ? root.bhCore.customAvatarUrl : root.defaultAvatar
                onClicked: if (root.bhCore) root.bhCore.chooseCustomAvatar()
            }

            Text {
                Layout.fillWidth: true
                text: "Blakhole UI"
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 28
                font.bold: true
                color: root.theme ? root.theme.focusColor : "#00C4B3"
            }

            Text {
                Layout.fillWidth: true
                text: root.bhCore ? "版本 " + Qt.application.version : "版本 1.2.0"
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 13
                color: root.theme ? Qt.rgba(root.theme.textColor.r, root.theme.textColor.g, root.theme.textColor.b, 0.55) : "#999999"
            }

            Text {
                Layout.fillWidth: true
                text: "Blakhole UI 是一款 Windows 黑洞可视化工具，可根据空闲状态、媒体播放和游戏运行状态自动控制黑洞显示。"
                wrapMode: Text.WordWrap
                font.pixelSize: 15
                lineHeight: 1.25
                color: root.theme ? root.theme.textColor : "#ffffff"
            }

            Text {
                Layout.fillWidth: true
                text: "使用前可在主界面调整黑洞外观、动画和显示模式；需要自动触发时，请配置空闲时间及相关名单。遇到未识别的视频软件或游戏，可在“空闲名单”中手动添加。"
                wrapMode: Text.WordWrap
                font.pixelSize: 14
                lineHeight: 1.25
                color: root.theme ? Qt.rgba(root.theme.textColor.r, root.theme.textColor.g, root.theme.textColor.b, 0.78) : "#d0d0d0"
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: pendingText.implicitHeight + 20
                radius: 10
                color: root.theme ? Qt.rgba(root.theme.focusColor.r, root.theme.focusColor.g, root.theme.focusColor.b, 0.10) : "#143c3a"

                Text {
                    id: pendingText
                    anchors.fill: parent
                    anchors.margins: 10
                    text: "待完善功能：黑洞吞噬效果选项。该功能仍在开发和优化中，当前版本的相关行为可能不完整。"
                    wrapMode: Text.WordWrap
                    font.pixelSize: 14
                    color: root.theme ? root.theme.focusColor : "#00C4B3"
                }
            }

            Text {
                Layout.fillWidth: true
                text: "项目开源地址：<a href=\"https://github.com/XboxNahida/ghostty-blackhole-main\">XboxNahida/ghostty-blackhole-main</a>"
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                font.pixelSize: 14
                color: root.theme ? root.theme.textColor : "#ffffff"
                linkColor: root.theme ? root.theme.focusColor : "#00C4B3"
                onLinkActivated: function(link) { Qt.openUrlExternally(link) }
            }

            Text {
                Layout.fillWidth: true
                text: "软件仍在持续完善，欢迎通过 GitHub 提交问题和建议。"
                wrapMode: Text.WordWrap
                font.pixelSize: 13
                color: root.theme ? Qt.rgba(root.theme.textColor.r, root.theme.textColor.g, root.theme.textColor.b, 0.58) : "#999999"
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: root.theme ? root.theme.borderColor : "#666666"
                opacity: 0.35
            }

            Text {
                Layout.fillWidth: true
                text: "请作者喝咖啡"
                font.pixelSize: 18
                font.bold: true
                color: root.theme ? root.theme.textColor : "#ffffff"
            }

            GridLayout {
                Layout.fillWidth: true
                columns: width >= 620 ? 2 : 1
                columnSpacing: 18
                rowSpacing: 18

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "支付宝"; font.pixelSize: 13; color: "#2d8cff"; Layout.alignment: Qt.AlignHCenter }
                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.min(300, root.width - 80)
                        Layout.preferredHeight: Layout.preferredWidth
                        source: "qrc:/new/prefix1/fonts/pic/QR_payment.jpg"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "微信支付"; font.pixelSize: 13; color: "#10b96b"; Layout.alignment: Qt.AlignHCenter }
                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.min(300, root.width - 80)
                        Layout.preferredHeight: Layout.preferredWidth
                        source: "qrc:/new/prefix1/fonts/pic/WeChat_QR.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.bhCore ? root.bhCore.avatarStatus : ""
                visible: text.length > 0
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: root.theme ? Qt.rgba(root.theme.textColor.r, root.theme.textColor.g, root.theme.textColor.b, 0.55) : "#999999"
            }
        }
    }
}
