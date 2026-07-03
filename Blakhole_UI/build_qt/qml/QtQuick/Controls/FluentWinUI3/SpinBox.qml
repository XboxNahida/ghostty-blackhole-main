// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import QtQuick.Controls.FluentWinUI3.impl

T.SpinBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             up.implicitIndicatorHeight, down.implicitIndicatorHeight)

    property string __controlState: [
        enabled && (down.hovered || down.pressed) && "down",
        enabled && (up.hovered || up.pressed) && !(down.hovered || down.pressed) && "up",
        enabled && (hovered || down.hovered || up.hovered) && !(down.pressed || up.pressed) && "hovered",
        enabled && (down.pressed || up.pressed) && "pressed",
        !enabled && "disabled"
    ].filter(Boolean).join("_") || "normal"
    readonly property var config: Config.controls.spinbox[__controlState] || {}
    readonly property var downConfig: value == from ? Config.controls.spinbox["atlimit"] : config
    readonly property var upConfig: value == to ? Config.controls.spinbox["atlimit"] : config

    spacing: config.contentItem.spacing || 0
    leftPadding: ((!mirrored ? config.leftPadding : config.rightPadding) || 0) + (mirrored ? (up.indicator ? up.indicator.width * 2 : 0) : 0)
    rightPadding: ((!mirrored ? config.rightPadding : config.leftPadding) || 0) + (!mirrored ? (up.indicator ? up.indicator.width * 2 : 0) : 0)
    topPadding: config.topPadding || 0
    bottomPadding: config?.bottomPadding || 0

    topInset: -config.topInset || 0
    bottomInset: -config.bottomInset || 0
    leftInset: -config.leftInset || 0
    rightInset: -config.rightInset || 0

    validator: IntValidator {
        locale: control.locale.name
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: TextInput {
        clip: width < implicitWidth
        text: control.displayText
        opacity: control.enabled ? 1 : 0.3

        font: control.font
        color: control.palette.text
        selectionColor: control.palette.highlight
        selectedTextColor: control.palette.highlightedText
        horizontalAlignment: control.mirrored ? Text.AlignRight : Text.AlignLeft
        verticalAlignment: Text.AlignVCenter

        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }

    down.indicator: StyleImage {
        x: !control.mirrored ? control.up.indicator ? (control.up.indicator.x - width) : 0
                             : control.config.rightPadding
        y: control.topPadding
        height: control.availableHeight
        imageConfig: control.downConfig.indicator_down_background

        StyleImage {
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            imageConfig: control.downConfig.indicator_down_icon
        }
    }

    up.indicator: StyleImage {
        x: control.mirrored ? control.config.rightPadding + (control.down.indicator ? control.down.indicator.width : 0)
                            : control.width - width - control.config.rightPadding
        y: control.topPadding
        height: control.availableHeight
        imageConfig: control.upConfig.indicator_up_background

        StyleImage {
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            imageConfig: control.upConfig.indicator_up_icon
        }
    }

    background: StyleImage {
        imageConfig: control.config.background
        Item {
            visible: control.activeFocus
            width: parent.width
            height: 2
            y: parent.height - height
            FocusStroke {
                width: parent.width
                height: parent.height
                radius: control.config.background.bottomOffset
                color: control.palette.accent
            }
        }
    }
}
