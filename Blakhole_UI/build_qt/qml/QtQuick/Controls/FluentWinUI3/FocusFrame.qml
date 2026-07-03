// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    // It's important that this item has a zero size. Otherwise, if the parent of the
    // targetItem is e.g a layout, we will change the layout if we parent this item inside it.
    width: 0
    height: 0
    // Stack on top of all siblings of the targetItem
    z: 100

    function moveToItem(item) {
        if (!item) {
            targetItem = null;
            parent = null;
            visible = false;
            return;
        }
        visible = true
        parent = item.parent
        targetItem = item
    }

    property Item targetItem
    property real innerFrameSize: 1
    property real outerFrameSize: 3
    property real frameRadius: 4.0

    Rectangle {
        id: focusFrame
        z: 10
        x: targetItem ? targetItem.x - root.x - outerFrameSize : 0
        y: targetItem ? targetItem.y - root.y - outerFrameSize : 0
        width: targetItem ? targetItem.width + outerFrameSize * 2 : 0
        height: targetItem ? targetItem.height + outerFrameSize * 2 : 0
        radius: frameRadius + outerFrameSize
        visible: targetItem && targetItem.visible
        color: "transparent"
        border.color: Application.styleHints.colorScheme === Qt.Light ? "black" : "white"
        border.width: outerFrameSize - (Application.styleHints.colorScheme === Qt.Light ? innerFrameSize : 0)

        Rectangle {
            id: innerFocusFrame
            z: 10
            x: outerFrameSize - innerFrameSize
            y: outerFrameSize - innerFrameSize
            width: targetItem ? targetItem.width + innerFrameSize * 2 : 0
            height: targetItem ? targetItem.height + innerFrameSize * 2 : 0
            radius: frameRadius + innerFrameSize
            visible: targetItem && targetItem.visible
            color: "transparent"
            border.color: Application.styleHints.colorScheme === Qt.Light ? "white" : "black"
            border.width: innerFrameSize
        }
    }
}
