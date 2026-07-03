// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

T.RangeSlider {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            first.implicitHandleWidth + leftPadding + rightPadding,
                            second.implicitHandleWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             first.implicitHandleHeight + topPadding + bottomPadding,
                             second.implicitHandleHeight + topPadding + bottomPadding)

    topPadding: horizontal ? config.topPadding : config.leftPadding || 0
    leftPadding: horizontal ? config.leftPadding : config.bottomPadding || 0
    rightPadding: horizontal ? config.rightPadding : config.topPadding || 0
    bottomPadding: horizontal ? config.bottomPadding : config.rightPadding || 0

    readonly property string __controlState: [
        !control.enabled && "disabled",
        control.enabled && control.hovered && !(first.pressed || second.pressed) && "hovered",
    ].filter(Boolean).join("_") || "normal"
    readonly property var config: Config.controls.rangeslider[__controlState] || {}

    readonly property real __steps: Math.abs(to - from) / stepSize
    readonly property bool __isDiscrete: stepSize >= Number.EPSILON
        && Math.abs(Math.round(__steps) - __steps) < Number.EPSILON

    property string __firstHandleState: [
        !control.enabled && "disabled",
        first.hovered && !first.pressed && "hovered",
        first.pressed && "handle_pressed",
    ].filter(Boolean).join("_") || "normal"
    readonly property var firstHandleConfig: Config.controls.rangeslider[__firstHandleState] || {}

    property string __secondHandleState: [
        !control.enabled && "disabled",
        second.hovered && !second.pressed && "hovered",
        second.pressed && "handle_pressed",
    ].filter(Boolean).join("_") || "normal"
    readonly property var secondHandleConfig: Config.controls.rangeslider[__secondHandleState] || {}

    readonly property Item __focusFrameControl: control
    readonly property Item __focusFrameTarget: control

    first.handle: StyleImage {
        x: Math.round(control.leftPadding + (control.horizontal
            ? control.first.visualPosition * (control.availableWidth - width)
            : (control.availableWidth - width) / 2))
        y: Math.round(control.topPadding + (control.horizontal
            ? (control.availableHeight - height) / 2
            : control.first.visualPosition * (control.availableHeight - height)))

        imageConfig: control.firstHandleConfig.first_handle

        readonly property Item __focusFrameTarget: control

        property Rectangle indicator: Rectangle {
            property real diameter: !control.enabled ? 10 : control.first.pressed ? 8 : control.first.hovered ? 14 : 10
            parent: control.first.handle
            width: diameter
            height: diameter
            radius: diameter * 0.5
            x: (control.secondHandleConfig.first_handle.width - width) / 2
            y: (control.secondHandleConfig.first_handle.height - height) / 2
            color: control.enabled ? (control.first.hovered ? Qt.rgba(control.palette.accent.r, control.palette.accent.g, control.palette.accent.b, 0.9020)
                                   : control.first.pressed ? Qt.rgba(control.palette.accent.r, control.palette.accent.g, control.palette.accent.b, 0.8)
                                   : control.palette.accent)
                                   : control.palette.accent
            Behavior on diameter {
                // From WindowsUI 3 Animation Values
                NumberAnimation {
                    duration: 167
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    second.handle: StyleImage {
        x: Math.round(control.leftPadding + (control.horizontal
            ? control.second.visualPosition * (control.availableWidth - width)
            : (control.availableWidth - width) / 2))
        y: Math.round(control.topPadding + (control.horizontal
            ? (control.availableHeight - height) / 2
            : control.second.visualPosition * (control.availableHeight - height)))

        imageConfig: control.secondHandleConfig.second_handle

        readonly property Item __focusFrameTarget: control

        property Rectangle indicator: Rectangle {
            property real diameter: control.second.pressed ? 8 : control.second.hovered ? 14 : 10
            parent: control.second.handle
            width: diameter
            height: diameter
            radius: diameter * 0.5
            x: (control.secondHandleConfig.second_handle.width - width) / 2
            y: (control.secondHandleConfig.second_handle.height - height) / 2
            color: control.enabled ? (control.second.hovered ? Qt.rgba(control.palette.accent.r, control.palette.accent.g, control.palette.accent.b, 0.9020)
                                   : control.second.pressed ? Qt.rgba(control.palette.accent.r, control.palette.accent.g, control.palette.accent.b, 0.8)
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
            height: parent.width
            imageConfig: control.config.background

            property Item groove: StyleImage {
                parent: control.background._background
                x: control.leftPadding - control.leftInset + (control.horizontal
                    ? control.firstHandleConfig.first_handle.width / 2
                    : (control.availableWidth - width) / 2)
                y: control.topPadding - control.rightInset + (control.horizontal
                    ? ((control.availableHeight - height) / 2)
                    : control.firstHandleConfig.first_handle.height / 2)

                width: control.horizontal
                    ? control.availableWidth
                        - (control.firstHandleConfig.first_handle.width / 2) - (control.secondHandleConfig.second_handle.width / 2)
                    : implicitWidth
                height: control.horizontal
                    ? implicitHeight
                    : control.availableHeight
                        - (control.firstHandleConfig.first_handle.width / 2) - (control.secondHandleConfig.second_handle.width / 2)
                imageConfig: control.config.groove
                horizontal: control.horizontal

                property Rectangle track: Rectangle {
                    parent: control.background._background.groove
                    x: control.horizontal ? parent.width * control.first.position : 0
                    y: control.horizontal ? 0 : parent.height - (parent.height * control.second.position)
                    implicitWidth: control.horizontal ? control.config.track.width : control.config.track.height
                    implicitHeight: control.horizontal ? control.config.track.height : control.config.track.width
                    width: control.horizontal
                        ? parent.width * (control.second.position - control.first.position)
                        : parent.width
                    height: control.horizontal
                        ? parent.height
                        : parent.height * (control.second.position - control.first.position)
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
