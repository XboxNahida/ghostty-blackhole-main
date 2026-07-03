// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Templates as T

T.ToolSeparator {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 2
    topPadding: vertical ? config.topPadding : padding
    bottomPadding: vertical ? config.bottomPadding : padding
    leftPadding: vertical ? padding : config.topPadding
    rightPadding: vertical ? padding : config.bottomPadding

    readonly property var config: Config.controls.toolbutton["normal"] || {}

    contentItem: Rectangle {
        implicitWidth: control.vertical ? 1 : control.config.background.height
        implicitHeight: control.vertical ? control.config.background.height : 1
        color: Application.styleHints.colorScheme === Qt.Light ? "#0F000000" : "#15FFFFFF"
    }
}
