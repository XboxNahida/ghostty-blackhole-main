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
                text: "关于 Blakhole Ubuntu Local"
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

            Text {
                Layout.fillWidth: true
                text: "版本 " + Qt.application.version
                font.pixelSize: 15
                font.bold: true
                color: root.theme.focusColor
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
                text: "Blakhole Ubuntu Local 是面向当前 Ubuntu 26.04、GNOME 50、Wayland 单显示器主机的本地桌面黑洞。Qt UI 通过会话 D-Bus 控制 GNOME Shell 合成器扩展，并提供预设、空闲触发、MPRIS 媒体抑制和全局停止快捷键。"
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
                text: "在主界面调整预设、播放模式和空闲时间，在高级页调整 GNOME 扩展实际读取的大小与速度。点击“启动黑洞”会保存配置并重新加载合成器效果；默认可随时按 Ctrl+Alt+B 停止。"
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
                    text: "本机变体不维护 Windows、多显示器、Xorg、Portal 捕获或上游在线更新。黑洞 shader 算法已冻结，后续只修复当前主机核心链路的真实缺陷。"
                    wrapMode: Text.WordWrap
                    font.pixelSize: 14
                    color: root.theme.focusColor
                }
            }

            Text {
                Layout.fillWidth: true
                text: "上游源码参考：<a href=\"https://github.com/XboxNahida/ghostty-blackhole-main\">github.com/XboxNahida/ghostty-blackhole-main</a>"
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
                text: "请作者喝咖啡"
                font.pixelSize: 18
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
