// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

T.ItemDelegate {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    spacing: config.spacing || 0

    topPadding: config.topPadding || 0 + verticalOffset
    leftPadding: config.leftPadding || 0 + __horizontalOffset
    rightPadding: config.rightPadding || 0 + __horizontalOffset
    bottomPadding: config.bottomPadding || 0 + __verticalOffset

    topInset: -config.topInset || 0
    bottomInset: -config.bottomInset || 0
    leftInset: -config.leftInset || 0
    rightInset: -config.rightInset || 0

    icon.width: 16
    icon.height: 16
    icon.color: control.down ? __pressedText : control.palette.buttonText

    readonly property int __horizontalOffset: 4
    readonly property int __verticalOffset: 2

    readonly property color __pressedText: Application.styleHints.colorScheme == Qt.Light
                                                    ? Qt.rgba(control.palette.buttonText.r, control.palette.buttonText.g, control.palette.buttonText.b, 0.62)
                                                    : Qt.rgba(control.palette.buttonText.r, control.palette.buttonText.g, control.palette.buttonText.b, 0.7725)
    readonly property string __currentState: [
        !control.enabled && "disabled",
        control.highlighted && "highlighted",
        control.enabled && !control.down && control.hovered && "hovered",
        control.down && "pressed"
    ].filter(Boolean).join("_") || "normal"
    readonly property var config: Config.controls.itemdelegate[__currentState] || {}

    readonly property Item __focusFrameTarget: control

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: control.display === IconLabel.IconOnly || control.display === IconLabel.TextUnderIcon ? Qt.AlignCenter : Qt.AlignLeft
        icon: control.icon
        text: control.text
        font: control.font
        color: control.icon.color
    }

    background: Item {
        implicitWidth: 160
        implicitHeight: 40

        property Item backgroundImage: StyleImage {
            parent: control.background
            imageConfig: control.config.background
            implicitWidth: parent.width - control.__horizontalOffset * 2
            implicitHeight: parent.height - control.__verticalOffset * 2
            x: control.__horizontalOffset
            y: control.__verticalOffset
        }

        property Rectangle selector: Rectangle {
            parent: control.background.backgroundImage
            y: (parent.height - height) / 2
            width: 3
            height: (control.highlighted || control.activeFocus)
                        ? control.down ? 10 : 16
                        : 0
            radius: width * 0.5
            color: control.palette.accent
            visible: control.highlighted || control.activeFocus

            Behavior on height {
                NumberAnimation {
                    duration: 187
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
}
