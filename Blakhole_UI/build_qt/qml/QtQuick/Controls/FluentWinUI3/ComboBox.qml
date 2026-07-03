// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls.impl
import QtQuick.Controls.FluentWinUI3.impl

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                            implicitIndicatorHeight + topPadding + bottomPadding)

    spacing: config.contentItem.spacing || 0

    topPadding: config.topPadding || 0
    bottomPadding: config.bottomPadding || 0
    leftPadding: (config.leftPadding + (!control.mirrored || !indicator || !indicator.visible ? 0 : indicator.width + spacing)) || 0
    rightPadding: (config.rightPadding + (control.mirrored || !indicator || !indicator.visible ? 0 : indicator.width + spacing)) || 0

    topInset: -config.topInset || 0
    bottomInset: -config.bottomInset || 0
    leftInset: -config.leftInset || 0
    rightInset: -config.rightInset || 0

    readonly property string __currentState: [
        !control.enabled && "disabled",
        control.enabled && !control.pressed && control.hovered && "hovered",
        control.down && control.popup.visible && "open",
        control.pressed && "pressed"
    ].filter(Boolean).join("_") || "normal"
    readonly property var config: (control.editable && control.down && control.popup.visible // editable combobox differs from normal one only in opened state
                                    ? Config.controls.editablecombobox[__currentState]
                                    : Config.controls.combobox[__currentState]) || {}

    readonly property Item __focusFrameTarget: control.editable ? null : control

    delegate: ItemDelegate {
        required property var model
        required property int index

        width: ListView.view.width
        text: model[control.textRole]
        palette.highlightedText: control.palette.highlightedText
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
    }

    indicator: Image {
        x: control.mirrored ? control.config.leftPadding : control.width - width - control.config.rightPadding
        y: (control.topPadding + (control.availableHeight - height) / 2) + (control.pressed ? 1 : 0)
        source: Qt.resolvedUrl(control.config.indicator.filePath)

        Behavior on y {
            NumberAnimation{ easing.type: Easing.OutCubic; duration: 167 }
        }
    }

    contentItem: T.TextField {
        text: control.editable ? control.editText : control.displayText

        topPadding: control.config.label_contentItem.topPadding || 0
        leftPadding: control.config.label_contentItem.leftPadding || 0
        rightPadding: control.config.label_contentItem.rightPadding || 0
        bottomPadding: control.config.label_contentItem.bottomPadding || 0

        implicitWidth: (implicitBackgroundWidth + leftInset + rightInset)
                        || contentWidth + leftPadding + rightPadding
        implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                 contentHeight + topPadding + bottomPadding)

        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        selectByMouse: control.selectTextByMouse

        readonly property color __pressedText: Application.styleHints.colorScheme == Qt.Light
                                                ? Qt.rgba(control.palette.text.r, control.palette.text.g, control.palette.text.b, 0.62)
                                                : Qt.rgba(control.palette.text.r, control.palette.text.g, control.palette.text.b, 0.7725)

        color: control.down ? __pressedText : control.palette.text
        selectionColor: control.palette.highlight
        selectedTextColor: control.palette.highlightedText
        horizontalAlignment: control.config.label_text.textHAlignment
        verticalAlignment: control.config.label_text.textVAlignment

        readonly property Item __focusFrameControl: control
    }

    background: StyleImage {
        imageConfig: control.config.background
        Item {
            visible: control.editable && ((control.down && control.popup.visible) || control.activeFocus)
            width: parent.width
            height: 2
            y: parent.height - height
            FocusStroke {
                width: parent.width
                height: parent.height
                radius: control.down && control.popup.visible ? 0 : control.config.background.bottomOffset
                color: control.palette.accent
            }
        }
    }

    popup: T.Popup {
        topPadding: control.config.popup_contentItem.topPadding || 0
        leftPadding: control.config.popup_contentItem.leftPadding || 0
        rightPadding: control.config.popup_contentItem.rightPadding || 0
        bottomPadding: control.config.popup_contentItem.bottomPadding || 0

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            highlightMoveDuration: 0

            model: control.delegateModel
            currentIndex: control.highlightedIndex
        }

        y: control.editable ? control.height
                            : -0.25 * Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                                contentHeight + topPadding + bottomPadding)
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, control.Window.height - topMargin - bottomMargin)
        width: control.width
        topMargin: 8
        bottomMargin: 8
        palette: control.palette

        enter: Transition {
            NumberAnimation { property: "height"; from: control.popup.height / 3; to: control.popup.height; easing.type: Easing.OutCubic; duration: 250 }
        }

        background: StyleImage {
            imageConfig: control.config.popup_background.filePath ? control.config.popup_background : Config.controls.popup["normal"].background // fallback to regular popup
        }
    }
}
