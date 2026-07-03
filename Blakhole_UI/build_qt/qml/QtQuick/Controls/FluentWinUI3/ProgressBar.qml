// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import QtQuick.Effects

T.ProgressBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

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
        control.indeterminate && "indeterminate"
    ].filter(Boolean).join("_") || "normal"
    readonly property var config: Config.controls.progressbar[__currentState] || {}

    contentItem: Item {
        implicitWidth: control.indeterminate ? parent.availableWidth : progress.implicitWidth
        implicitHeight: control.indeterminate ? control.config.track.height : progress.implicitHeight
        scale: control.mirrored ? -1 : 1
        clip: control.indeterminate

        readonly property Rectangle progress: Rectangle {
            x: control.background.groove?.x - 1
            y: control.background.groove?.y - 1
            parent: control.contentItem
            visible: !control.indeterminate && control.value
            implicitWidth: control.config.track.width
            implicitHeight: control.config.track.height
            width: control.position * parent.width
            height: control.config.track.height
            radius: control.config.track.height * 0.5
            color: control.palette.accent
        }

        readonly property Rectangle animatedProgress: Rectangle {
            parent: control.contentItem
            implicitWidth: parent.width
            implicitHeight: control.config.track.height
            radius: control.config.track.height * 0.5
            clip: true
            visible: false
            color: "transparent"
            Rectangle {
                width: 0.5 * parent.width
                height: control.config.track.height
                radius: control.config.track.height * 0.5
                color: control.palette.accent
                SequentialAnimation on x {
                    loops: Animation.Infinite
                    running: control.indeterminate && control.visible
                    NumberAnimation {
                        from: -control.contentItem.animatedProgress.width
                        to: control.contentItem.width
                        easing.type: Easing.InOutCubic
                        duration: control.width * 8
                    }
                    NumberAnimation {
                        from: -control.contentItem.animatedProgress.width * 0.5
                        to: control.contentItem.width
                        easing.type: Easing.InOutCubic
                        duration: control.width * 5
                    }
                }
            }
        }

        readonly property Rectangle mask: Rectangle {
            parent: control.contentItem
            width: control.availableWidth
            height: control.contentItem.animatedProgress.height
            radius: control.contentItem.animatedProgress.radius
            visible: false
            color: control.palette.accent
            layer.enabled: true
            antialiasing: false
        }

        MultiEffect {
            visible: control.indeterminate
            source: control.contentItem.animatedProgress
            width: control.contentItem.animatedProgress.width
            height: control.contentItem.animatedProgress.height
            maskEnabled: true
            maskSource: control.contentItem.mask
        }
    }

    background: Item {
        implicitWidth: groove.width
        property Item groove: StyleImage {
            imageConfig: control.config.groove
            visible: !control.indeterminate
            parent: control.background
            height: implicitHeight
            width: parent.width
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
        }
    }
}
