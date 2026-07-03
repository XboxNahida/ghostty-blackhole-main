// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

T.Popup {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)

    topPadding: config.topPadding || 0
    bottomPadding: config.bottomPadding || 0
    leftPadding: config.leftPadding || 0
    rightPadding: config.rightPadding || 0

    topInset: -config.topInset || 0
    bottomInset: -config.bottomInset || 0
    leftInset: -config.leftInset || 0
    rightInset: -config.rightInset || 0

    readonly property string __currentState: "normal"
    readonly property var config: Config.controls.popup[__currentState] || {}

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; easing.type: Easing.Linear; duration: 83 }
        NumberAnimation { property: "scale"; from: control.modal ? 1.05 : 1; to: 1; easing.type: Easing.OutCubic; duration: 167 }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; easing.type: Easing.Linear; duration: 83 }
        NumberAnimation { property: "scale"; from: 1; to: control.modal ? 1.05 : 1; easing.type: Easing.OutCubic; duration: 167 }
    }

    background: StyleImage {
        implicitWidth: 320
        implicitHeight: 72
        imageConfig: control.config.background
    }

    T.Overlay.modal: Rectangle {
        color: Color.transparent(control.palette.shadow, 0.3)
    }

    T.Overlay.modeless: Rectangle {
        color: "transparent"
    }
}
