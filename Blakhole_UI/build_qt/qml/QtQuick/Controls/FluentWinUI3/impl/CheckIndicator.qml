// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import QtQuick.Shapes

ColorImage {
    id: indicator

    required property T.AbstractButton control
    required property url filePath

    source: filePath
    color: control.enabled && control.checkState !== Qt.Unchecked ? control.palette.accent : defaultColor

    readonly property color indicatorColor: control.down ? Application.styleHints.colorScheme == Qt.Light
                                                ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.5)
                                                : Application.styleHints.colorScheme === Qt.Dark && !control.enabled
                                                ? Qt.rgba(1, 1, 1, 0.5302)
                                                : Application.styleHints.colorScheme === Qt.Dark ? "black" : "white"

    // TODO: Add animation for checkmark indicator
    Shape {
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: 12
        height: 12
        visible: control.checked

        antialiasing: true
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: 1
            strokeColor: indicator.indicatorColor
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            startX: 1
            startY: 6
            PathLine { x: 5; y: 10 }
            PathLine { x: 11; y: 3 }
        }
    }

    Rectangle {
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        visible: control.checkState === Qt.PartiallyChecked
        width: 8
        height: 1
        radius: height * 0.5
        color: indicator.indicatorColor
    }
}
