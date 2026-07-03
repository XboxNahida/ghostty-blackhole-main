// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

T.Slider {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitHandleWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitHandleHeight + topPadding + bottomPadding)

    topPadding: horizontal ? config.topPadding : config.leftPadding || 0
    leftPadding: horizontal ? config.leftPadding : config.bottomPadding || 0
    rightPadding: horizontal ? config.rightPadding : config.topPadding || 0
    bottomPadding: horizontal ? config.bottomPadding : config.rightPadding || 0

    readonly property string __currentState: [
        !control.enabled && "disabled",
        control.enabled && !control.pressed && control.hovered && "hovered",
        control.pressed && "pressed"
    ].filter(Boolean).join("_") || "normal"
    readonly property var config: Config.controls.slider[__currentState] || {}

    readonly property Item __focusFrameTarget: control

    readonly property real __steps: Math.abs(to - from) / stepSize
    readonly property bool __isDiscrete: stepSize >= Number.EPSILON
        && Math.abs(Math.round(__steps) - __steps) < Number.EPSILON

    handle: StyleImage {
        x: Math.round(control.leftPadding + (control.horizontal
            ? control.visualPosition * (control.availableWidth - width)
            : (control.availableWidth - width) / 2))
        y: Math.round(control.topPadding + (control.horizontal
            ? (control.availableHeight - height) / 2
            : control.visualPosition * (control.availableHeight - height)))

        imageConfig: control.config.handle

        property Rectangle indicator: Rectangle {
            property real diameter: !control.enabled ? 10 : control.pressed ? 8 : control.hovered ? 14 : 10
            parent: control.handle
            width: diameter
            height: diameter
            radius: diameter * 0.5
            x: (control.config.handle.width - width) / 2
            y: (control.config.handle.height - height) / 2

            color: control.enabled ? (control.hovered ? Qt.rgba(control.palette.accent.r, control.palette.accent.g, control.palette.accent.b, 0.9020)
                                   : control.pressed ? Qt.rgba(control.palette.accent.r, control.palette.accent.g, control.palette.accent.b, 0.8)
                                   : control.palette.accent)
                                   : control.palette.accent
            Behavior on diameter {
                // From WindowsUI 3 Animation Values
                NumberAnimation{
                    duration: 167
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    background: Item {
        implicitWidth: control.horizontal
            ? (_background.implicitWidth || _background.groove.implicitWidth)
            : (_background.implicitHeight || _background.groove.implicitHeight)
        implicitHeight: control.horizontal
            ? (_background.implicitHeight || _background.groove.implicitHeight)
            : (_background.implicitWidth || _background.groove.implicitWidth)

        property Item _background: StyleImage {
            parent: control.background
            width: parent.width
            height: parent.height
            imageConfig: control.config.background

            property Item groove: StyleImage {
                parent: control.background._background
                x: control.leftPadding - control.leftInset + (control.horizontal
                    ? control.config.handle.width / 2
                    : (control.availableWidth - width) / 2)
                y: control.topPadding - control.topInset + (control.horizontal
                    ? ((control.availableHeight - height) / 2)
                    : control.config.handle.height / 2)

                width: control.horizontal
                    ? control.availableWidth - control.config.handle.width
                    : implicitWidth
                height: control.horizontal
                    ? implicitHeight
                    : control.availableHeight - control.config.handle.width
                imageConfig: control.config.groove
                horizontal: control.horizontal

                property Rectangle track: Rectangle {
                    parent: control.background._background.groove
                    y: control.horizontal ? 0 : parent.height - (parent.height * control.position)
                    implicitWidth: control.horizontal ? control.config.track.width : control.config.track.height
                    implicitHeight: control.horizontal ? control.config.track.height : control.config.track.width
                    width: control.horizontal ? parent.width * control.position : parent.width
                    height: control.horizontal ? parent.height : parent.height * control.position
                    radius: control.config.track.height * 0.5
                    color: control.palette.accent
                }
            }

            property Repeater ticksTop: Repeater {
                parent: control.background._background.groove
                model: control.__isDiscrete ? Math.floor(control.__steps) + 1 : 0
                delegate: Rectangle {
                    width: control.horizontal ? 1 : 4
                    height: control.horizontal ? 4 : 1
                    x: control.horizontal
                        ? 6 + index * (parent.width - 2 * 6 - width) / (control.background._background.ticksTop.model - 1)
                        : -4 - width
                    y: control.horizontal
                        ? -4 - height
                        : 6 + index * (parent.height - 2 * 6 - height) / (control.background._background.ticksTop.model - 1)
                    color: Application.styleHints.colorScheme == Qt.Light ? "#9C000000" : "#9AFFFFFF"

                    required property int index
                }
            }

            property Repeater ticksBottom: Repeater {
                parent: control.background._background.groove
                model: control.__isDiscrete ? Math.floor(control.__steps) + 1 : 0
                delegate: Rectangle {
                    width: control.horizontal ? 1 : 4
                    height: control.horizontal ? 4 : 1
                    x: control.horizontal
                        ? 6 + index * (parent.width - 2 * 6 - width) / (control.background._background.ticksBottom.model - 1)
                        : parent.width + 4
                    y: control.horizontal
                        ? parent.height + 4
                        : 6 + index * (parent.height - 2 * 6 - height) / (control.background._background.ticksBottom.model - 1)
                    color: Application.styleHints.colorScheme == Qt.Light ? "#9C000000" : "#9AFFFFFF"

                    required property int index
                }
            }
        }
    }
}
