// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls.impl
import QtQuick.Controls.FluentWinUI3.impl

T.DelayButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    spacing: config.spacing || 0

    topPadding: config.topPadding || 0
    bottomPadding: config.bottomPadding || 0
    leftPadding: config.leftPadding || 0
    rightPadding: config.rightPadding || 0

    icon.width: config.icon.width
    icon.height: config.icon.height
    icon.color: __buttonText

    readonly property color __buttonText: {
        if (control.down) {
            return (control.checked)
                ? Application.styleHints.colorScheme == Qt.Light
                    ? Color.transparent("white", 0.7) : Color.transparent("black", 0.5)
                : (Application.styleHints.colorScheme === Qt.Light
                    ? Color.transparent(control.palette.buttonText, 0.62)
                    : Color.transparent(control.palette.buttonText, 0.7725))
        } else if (control.checked) {
            return (Application.styleHints.colorScheme === Qt.Dark && !control.enabled)
                ? Color.transparent("white", 0.5302)
                : (Application.styleHints.colorScheme === Qt.Dark ? "black" : "white")
        } else {
            return control.palette.buttonText
        }
    }

    readonly property string __currentState: [
        control.checked && "checked",
        !control.enabled && "disabled",
        control.enabled && !control.down && control.hovered && "hovered",
        control.down && "pressed"
    ].filter(Boolean).join("_") || "normal"
    readonly property var config: Config.controls.button[__currentState] || {}

    readonly property Item __focusFrameTarget: control

    transition: Transition {
        NumberAnimation {
            duration: control.delay * (control.pressed ? 1.0 - control.progress : 0.3 * control.progress)
        }
    }

    contentItem: ItemGroup {
        ClippedText {
            clip: control.progress > 0
            clipX: -control.leftPadding + control.progress * control.width
            clipWidth: (1.0 - control.progress) * control.width
            visible: control.progress < 1

            text: control.text
            font: control.font
            color: control.palette.buttonText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        ClippedText {
            clip: control.progress > 0
            clipX: -control.leftPadding
            clipWidth: control.progress * control.width
            visible: control.progress > 0

            text: control.text
            font: control.font
            color: control.palette.brightText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    background: ButtonBackground {
        control: control
        implicitHeight: control.config.background.height
        implicitWidth: control.config.background.width
        radius: control.config.background.topOffset
        subtle: false

        Rectangle {
            width: control.progress * parent.width
            height: parent.height
            radius: parent.radius
            color: control.down ? control.palette.accent : "transparent"
            visible: !control.checked && control.enabled
        }
    }
}
