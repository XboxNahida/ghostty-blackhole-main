// ESlider.qml — 带标签和数值显示的滑块组件
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    // === 外部接口 ===
    property string label: ""
    property real value: 0.5          // 内部跟踪值，绑定到 Slider
    property real externalValue: 0.5  // 接收外部绑定（永不直接赋值）
    signal userChanged(real newValue)  // 仅用户拖拽时发射

    property real from: 0.0
    property real to: 1.0
    property real stepSize: 0.01
    property int decimals: 2
    property string suffix: ""
    property string prefix: ""

    // === 外部赋值 → 内部同步（不打破 externalValue 绑定） ===
    onExternalValueChanged: {
        if (Math.abs(root.value - externalValue) > 0.0001) {
            root.value = externalValue
        }
    }

    implicitHeight: 48
    implicitWidth: 260

    ColumnLayout {
        anchors.fill: parent
        spacing: 2

        // 标签行: 名称 + 数值
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: root.label
                font.pixelSize: 13
                color: theme.textColor
                opacity: 0.65
                Layout.alignment: Qt.AlignLeft
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.prefix + root.value.toFixed(root.decimals) + root.suffix
                font.pixelSize: 13
                font.bold: true
                color: theme.focusColor
                Layout.alignment: Qt.AlignRight
            }
        }

        // 滑块
        Slider {
            id: slider
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            from: root.from
            to: root.to
            stepSize: root.stepSize
            value: root.value

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                implicitWidth: slider.availableWidth
                implicitHeight: 4
                width: slider.availableWidth
                height: implicitHeight
                radius: 2
                color: theme.borderColor
                opacity: 0.4

                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    radius: 2
                    color: theme.focusColor
                }
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                implicitWidth: 18
                implicitHeight: 18
                radius: 9
                color: theme.focusColor
                border.color: theme.textColor
                border.width: 2

                Behavior on color { ColorAnimation { duration: 100 } }
            }

            // 用户拖拽：更新内部值 + 发射 userChanged
            onValueChanged: {
                if (Math.abs(root.value - slider.value) > 0.0001) {
                    root.value = slider.value
                    if (slider.pressed) {
                        root.userChanged(slider.value)
                    }
                }
            }
        }
    }

    // 内部值变化 → 同步 Slider（程序赋值时）
    onValueChanged: {
        if (Math.abs(slider.value - root.value) > 0.0001) {
            slider.value = root.value
        }
    }
}