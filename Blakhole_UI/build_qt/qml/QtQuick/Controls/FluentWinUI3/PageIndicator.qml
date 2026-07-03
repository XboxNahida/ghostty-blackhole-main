// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

T.PageIndicator {
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

    topInset: -config.topInset || 0
    bottomInset: -config.bottomInset || 0
    leftInset: -config.leftInset || 0
    rightInset: -config.rightInset || 0

    readonly property string __currentState: [
        !control.enabled && "disabled",
        control.enabled && control.hovered && "hovered",
    ].filter(Boolean).join("_") || "normal"
    readonly property var config: Config.controls.pageindicator[__currentState] || {}

    delegate: StyleImage {
        required property int index

        property alias hovered: hoverHandler.hovered

        readonly property string __currentState: [
            !control.enabled && "disabled",
            control.enabled && (index === control.currentIndex || pressed) && "delegate",
            control.enabled && index === control.currentIndex && "current",
            control.enabled && hovered && !pressed && "hovered",
            control.enabled && control.interactive && pressed && "pressed",
        ].filter(Boolean).join("_") || "normal"
        readonly property var config: Config.controls.pageindicatordelegate[__currentState].indicator || {}

        imageConfig: config

        HoverHandler {
            id: hoverHandler
            enabled: control.interactive
        }
    }

    contentItem: Row {
        spacing: control.spacing

        Repeater {
            model: control.count
            delegate: control.delegate
        }
    }

    background: StyleImage {
        imageConfig: control.config.background
    }
}
