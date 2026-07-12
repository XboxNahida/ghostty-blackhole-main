import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var theme
    required property var bhCore
    signal backRequested()

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        ColumnLayout {
            id: contentColumn
            width: parent.width
            spacing: 18

            RowLayout {
                Layout.fillWidth: true

                Components.EButton {
                    text: "返回"
                    iconCharacter: "\uf060"
                    backgroundVisible: false
                    onClicked: root.backRequested()
                }

                Item { Layout.fillWidth: true }
            }

            Text {
                Layout.fillWidth: true
                text: "关于 Blakhole UI"
                font.pixelSize: 42
                font.bold: true
                color: root.theme.focusColor
            }

            Text {
                Layout.fillWidth: true
                text: "让黑洞成为桌面的一部分"
                font.pixelSize: 24
                color: root.theme.textColor
                opacity: 0.78
            }

            Components.EAvatar {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 132
                Layout.preferredHeight: 132
                clickable: true
                avatarSource: root.bhCore ? root.bhCore.customAvatarUrl : "qrc:/new/prefix1/fonts/pic/avatar.png"
                onClicked: {
                    if (root.bhCore) root.bhCore.chooseCustomAvatar()
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "点击头像可更换，选择结果会持久化保存"
                font.pixelSize: 12
                color: root.theme.textColor
                opacity: 0.55
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: root.theme.borderColor
                opacity: 0.35
            }

            Text {
                Layout.fillWidth: true
                text: "软件说明"
                font.pixelSize: 20
                font.bold: true
                color: root.theme.textColor
            }

            Text {
                Layout.fillWidth: true
                text: "Blakhole UI 是一款 Windows 黑洞可视化工具，可根据空闲状态、媒体播放和游戏运行状态自动控制黑洞显示。"
                wrapMode: Text.WordWrap
                font.pixelSize: 15
                lineHeight: 1.3
                color: root.theme.textColor
            }

            Text {
                Layout.fillWidth: true
                text: "使用说明"
                font.pixelSize: 20
                font.bold: true
                color: root.theme.textColor
            }

            Text {
                Layout.fillWidth: true
                text: "先在主界面调整黑洞外观、动画和显示模式；需要自动触发时，请配置空闲时间及相关名单。遇到未识别的视频软件或游戏，可在空闲名单中手动添加。"
                wrapMode: Text.WordWrap
                font.pixelSize: 14
                lineHeight: 1.3
                color: root.theme.textColor
                opacity: 0.78
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: pendingText.implicitHeight + 24
                radius: 8
                color: Qt.rgba(root.theme.focusColor.r, root.theme.focusColor.g, root.theme.focusColor.b, 0.10)

                Text {
                    id: pendingText
                    anchors.fill: parent
                    anchors.margins: 12
                    text: "待完善：黑洞吞噬效果选项仍在开发和优化中，当前版本的相关行为可能不完整。"
                    wrapMode: Text.WordWrap
                    font.pixelSize: 14
                    color: root.theme.focusColor
                }
            }

            Text {
                Layout.fillWidth: true
                text: "项目开源地址：<a href=\"https://github.com/XboxNahida/ghostty-blackhole-main\">github.com/XboxNahida/ghostty-blackhole-main</a>"
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                font.pixelSize: 14
                color: root.theme.textColor
                linkColor: root.theme.focusColor
                onLinkActivated: function(link) { Qt.openUrlExternally(link) }
            }

            Text {
                Layout.fillWidth: true
                visible: root.bhCore && root.bhCore.paymentQrAvailable
                text: "支持项目"
                font.pixelSize: 20
                font.bold: true
                color: root.theme.textColor
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.bhCore && root.bhCore.paymentQrAvailable
                spacing: 18

                Image {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.maximumWidth: 280
                    Layout.preferredHeight: 320
                    source: root.bhCore ? root.bhCore.paymentQrPrimaryUrl : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }

                Image {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.maximumWidth: 280
                    Layout.preferredHeight: 320
                    source: root.bhCore ? root.bhCore.paymentQrSecondaryUrl : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.bhCore ? root.bhCore.avatarStatus : ""
                visible: text.length > 0
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: root.theme.textColor
                opacity: 0.55
            }
        }
    }
}
