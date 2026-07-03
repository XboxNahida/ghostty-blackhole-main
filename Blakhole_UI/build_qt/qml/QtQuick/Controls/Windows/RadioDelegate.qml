// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick.NativeStyle as NativeStyle

NativeStyle.DefaultRadioDelegate {
    contentItem: NativeStyle.DefaultItemDelegateIconLabel {
        color: control.highlighted ? control.palette.button : control.palette.windowText

        readonly property bool __ignoreNotCustomizable: true
    }
}
