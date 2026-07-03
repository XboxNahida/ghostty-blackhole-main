// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

ColorImage {
    id: indicator

    required property T.AbstractButton control
    required property url filePath

    source: filePath
    color: control.enabled && control.checked ? control.palette.accent : defaultColor

    property Item indicatorBackground: Rectangle {
        parent: control.indicator
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: 10
        height: 10
        radius: height * 0.5
        scale: !control.checked && !control.down ? 0 : control.down && control.checked ? 0.8 : control.hovered ? 1.2 : 1

        gradient: Gradient {
            GradientStop {
                position: 0
                color: !control.checked ? "transparent" : Application.styleHints.colorScheme == Qt.Light ? "#0F000000" : "#12FFFFFF"
            }
            GradientStop {
                position: 0.5
                color: !control.checked ? "transparent" : Application.styleHints.colorScheme == Qt.Light ? "#0F000000" : "#12FFFFFF"
            }
            GradientStop {
                position: 0.95
                color: !control.checked ? "transparent" : Application.styleHints.colorScheme == Qt.Light ? "#29000000" : "#18FFFFFF"
            }
        }

        Rectangle {
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            width: parent.width - 2
            height: parent.height - 2
            radius: height * 0.5
            color: Application.styleHints.colorScheme === Qt.Dark ? "black" : "white"
        }

        Behavior on scale {
            NumberAnimation{
                duration: 167
                easing.type: Easing.OutCubic
            }
        }
    }
}
