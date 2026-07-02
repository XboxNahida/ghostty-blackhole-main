// BlackholeConfig.qml 闁冲厜鍋撻柍鍏夊亾 濮掓稒鍨剁粈濠囨煀瀹ュ洨鏋傚☉鎾愁煼閵嗗妫?(v2: 閻庝絻顫夌敮?C++ BlackHoleCore + 闁活亞鍠庨悿?Shader 濡澘瀚～?
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BlakholeUI 1.0
import ".." as Root
import "../components" as Components

Item {
    id: configPage

    anchors.fill: parent
    anchors.margins: 20

    // === 鐎殿喗娲滈弫?BlackHoleCore (闁?main.cpp 婵炲鍔岄崣鍡涘冀闁稓鐟愬☉鎾愁儐閺? ===
    property var bhCore: null

    // === 鐟滅増鎸告晶鐘绘焻婢跺鍘Λ鏉垮椤?===
    property int currentPresetIndex: bhCore ? bhCore.currentPresetIndex : 0

    // === 闁稿繈鍔岄惇顒傛媼閸撗呮瀭缂備焦鍨甸悾楣冨礆?BlackHoleCore ===
    property int displayMode:  bhCore ? bhCore.displayMode  : 0
    property int idleSeconds:  bhCore ? bhCore.idleSeconds  : 300
    property int playMode:     bhCore ? bhCore.playMode     : 1
    property real slotSeconds: bhCore ? bhCore.slotSeconds  : 5.25

    // === 鐟滅増鎸告晶鐘筹紣閸曨噮鍟?14 闁告瑥鍊归弳?(濞撴碍绋掔划锕傚锤濡ゅ啰鎷ㄩ悗? ===
    property real diskTemp:  bhCore ? bhCore.diskTemp  : 5500
    property real diskIncl:  bhCore ? bhCore.diskIncl  : 1.50
    property real diskRoll:  bhCore ? bhCore.diskRoll  : 0.35
    property real diskInner: bhCore ? bhCore.diskInner : 1.8
    property real diskOuter: bhCore ? bhCore.diskOuter : 8.0
    property real diskOpac:  bhCore ? bhCore.diskOpac  : 0.90
    property real diskDopp:  bhCore ? bhCore.diskDopp  : 0.60
    property real diskBeam:  bhCore ? bhCore.diskBeam  : 2.5
    property real diskGain:  bhCore ? bhCore.diskGain  : 2.2
    property real diskContr: bhCore ? bhCore.diskContr : 1.6
    property real diskWind:  bhCore ? bhCore.diskWind  : 7.0
    property real diskSpeed: bhCore ? bhCore.diskSpeed : 5.0
    property real diskExpo:  bhCore ? bhCore.diskExpo  : 1.40
    property real diskStar:  bhCore ? bhCore.diskStar  : 0.0

    Connections {
        target: bhCore
        function onCurrentPresetChanged() {
            // 閻熸瑱绠戣ぐ鍌炲箥閳ь剟寮垫径宀€鎷ㄩ悗瑙勭濞插潡寮?
            currentPresetIndex = bhCore.currentPresetIndex
            configPage.diskTemp  = bhCore.diskTemp
            configPage.diskIncl  = bhCore.diskIncl
            configPage.diskRoll  = bhCore.diskRoll
            configPage.diskInner = bhCore.diskInner
            configPage.diskOuter = bhCore.diskOuter
            configPage.diskOpac  = bhCore.diskOpac
            configPage.diskDopp  = bhCore.diskDopp
            configPage.diskBeam  = bhCore.diskBeam
            configPage.diskGain  = bhCore.diskGain
            configPage.diskContr = bhCore.diskContr
            configPage.diskWind  = bhCore.diskWind
            configPage.diskSpeed = bhCore.diskSpeed
            configPage.diskExpo  = bhCore.diskExpo
            configPage.diskStar  = bhCore.diskStar
        }
    }

    onBhCoreChanged: {
        if (bhCore && idleSpin && slotSpin) {
            idleSpin.value = configPage.idleSeconds
            slotSpin.value = Math.round(configPage.slotSeconds * 10)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // ========== 闁哄秴娲。?==========
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "\uf661"
                font.family: iconFont.name
                font.pixelSize: 22
                color: theme.focusColor
            }
            Text {
                text: "\u9ed1\u6d1e\u914d\u7f6e\u9762\u677f"
                font.pixelSize: 22
                font.bold: true
                color: theme.focusColor
            }
            Item { Layout.fillWidth: true }

            // 闁告凹鍨版慨?闁稿绮嶉娑樸€掗崣澶屽帬闁?
            Components.EButton {
                text: bhCore && bhCore.systemActive ? "\u505c\u6b62" : "\u542f\u52a8\u9ed1\u6d1e"
                size: "s"
                iconCharacter: bhCore && bhCore.systemActive ? "\uf04d" : "\uf04b"
                backgroundVisible: true
                containerColor: bhCore && bhCore.systemActive ? "#d32f2f" : theme.focusColor
                textColor: "#ffffff"
                onClicked: {
                    if (bhCore) {
                        if (bhCore.systemActive) bhCore.stopAll()
                        else bhCore.applyAndStart()
                    }
                }
            }
        }

        // ========== 闁稿繈鍔岄惇顒傛媼閸撗呮瀭闁告绱曟晶?==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 90
            radius: 14
            color: theme.secondaryColor

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 16

                // 闁哄嫬澧介妵姘熼垾宕囩
                Column { spacing: 4
                    Layout.preferredWidth: 140
                    Text { text: "\u663e\u793a\u6a21\u5f0f"; font.pixelSize: 12; color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5) }
                    Components.EDropDown {
                        width: 130; height: 32
                        model: ["\u59cb\u7ec8\u663e\u793a", "\u7a7a\u95f2\u68c0\u6d4b"]
                        currentIndex: configPage.displayMode
                        onCurrentIndexChanged: { if (bhCore) bhCore.displayMode = currentIndex }
                    }
                }

                // 缂佸本妞藉Λ鐣岀矓閹烘挻娈?
                Column { spacing: 4
                    Layout.preferredWidth: 120
                    Text { text: "\u7a7a\u95f2\u79d2\u6570"; font.pixelSize: 12; color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5) }
                    SpinBox {
                        id: idleSpin
                        width: 110; height: 32
                        from: 1; to: 1800; stepSize: 10
                        editable: true
                        value: configPage.idleSeconds
                        onValueChanged: { if (bhCore) bhCore.idleSeconds = value }
                        contentItem: TextInput {
                            text: idleSpin.displayText
                            font: idleSpin.font
                            color: theme.textColor
                            selectionColor: theme.focusColor
                            selectedTextColor: "#ffffff"
                            horizontalAlignment: Qt.AlignHCenter
                            verticalAlignment: Qt.AlignVCenter
                            readOnly: !idleSpin.editable
                            validator: idleSpin.validator
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                        }

                        background: Rectangle {
                            implicitWidth: 110
                            implicitHeight: 32
                            radius: 6
                            color: theme.secondaryColor
                            border.color: theme.borderColor
                            border.width: 1
                        }
                    }
                }

                // 闁圭虎鍘介弬浣肝熼垾宕囩
                Column { spacing: 4
                    Layout.preferredWidth: 120
                    Text { text: "\u64ad\u653e\u6a21\u5f0f"; font.pixelSize: 12; color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5) }
                    Components.EDropDown {
                        width: 110; height: 32
                        model: ["\u987a\u5e8f", "\u5faa\u73af", "\u968f\u673a"]
                        currentIndex: configPage.playMode
                        onCurrentIndexChanged: { if (bhCore) bhCore.playMode = currentIndex }
                    }
                }

                // 婵″弶鍨濈紞鍛矓閹烘挻娈?(SpinBox 濞寸姴鎳忛弫顕€骞?int, 缂傚倵鏅滈弬?x10)
                Column { spacing: 4
                    Layout.preferredWidth: 120
                    Text { text: "\u69fd\u4f4d\u79d2\u6570"; font.pixelSize: 12; color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5) }
                    SpinBox {
                        width: 110; height: 32
                        id: slotSpin
                        from: 5; to: 600; stepSize: 2
                        editable: true
                        textFromValue: function(v, l) { return (v / 10.0).toFixed(1) }
                        value: Math.round(configPage.slotSeconds * 10)
                        onValueChanged: { if (bhCore) bhCore.slotSeconds = value / 10.0 }
                        contentItem: TextInput {
                            text: slotSpin.displayText
                            font: slotSpin.font
                            color: theme.textColor
                            selectionColor: theme.focusColor
                            selectedTextColor: "#ffffff"
                            horizontalAlignment: Qt.AlignHCenter
                            verticalAlignment: Qt.AlignVCenter
                            readOnly: !slotSpin.editable
                            validator: DoubleValidator {
                                bottom: 0.5
                                top: 60.0
                                decimals: 1
                                notation: DoubleValidator.StandardNotation
                            }
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                        }

                        background: Rectangle {
                            implicitWidth: 110
                            implicitHeight: 32
                            radius: 6
                            color: theme.secondaryColor
                            border.color: theme.borderColor
                            border.width: 1
                        }
                    }
                }

                Item { Layout.fillWidth: true }

            }
        }

        

        // ========== 娑撹鍞寸€圭懓灏? 妫板嫯顫?| 閸欏倹鏆熺拫鍐Ν | 妫板嫯顔曠粻锛勬倞 ==========
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // 閳光偓閳光偓閳光偓 瀹? 姒涙垶绀婃０鍕潔 (瀵鈧冾啍鎼? 閳光偓閳光偓閳光偓
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Text {
                    text: "\u5b9e\u65f6\u9884\u89c8"
                    font.pixelSize: 14; font.bold: true
                    color: theme.textColor
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 14
                    color: "#0a0a14"
                    border.color: theme.borderColor
                    border.width: 1

                    Components.BlackholePreviewArea {
                        id: blackholePreviewFBO
                        anchors.fill: parent
                        anchors.margins: 4

                        diskTemp:  configPage.diskTemp
                        diskIncl:  configPage.diskIncl
                        diskRoll:  configPage.diskRoll
                        diskInner: configPage.diskInner
                        diskOuter: configPage.diskOuter
                        diskOpac:  configPage.diskOpac
                        diskDopp:  configPage.diskDopp
                        diskBeam:  configPage.diskBeam
                        diskGain:  configPage.diskGain
                        diskContr: configPage.diskContr
                        diskWind:  configPage.diskWind
                        diskSpeed: configPage.diskSpeed
                        diskExpo:  configPage.diskExpo
                        diskStar:  configPage.diskStar
                        running:   true

                        onEnlargeRequested: largePreview.open()
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\u70b9\u51fb\u9884\u89c8\u533a\u57df\u653e\u5927"
                    font.pixelSize: 11
                    color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5)
                }

                Text {
                    text: bhCore && bhCore.rendererRunning
                        ? "\u25cf \u6e32\u67d3\u5668\u8fd0\u884c\u4e2d"
                        : "\u25cb \u6e32\u67d3\u5668\u672a\u542f\u52a8"
                    font.pixelSize: 12
                    color: bhCore && bhCore.rendererRunning ? "#4caf50" : theme.borderColor
                }
            }

            // 閳光偓閳光偓閳光偓 娑? 閸欏倹鏆熺拫鍐Ν + 鎼存洟鍎撮幐澶愭尦 閳光偓閳光偓閳光偓
            Column {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                spacing: 6

                Text {
                    id: paramTitle
                    text: "\u53c2\u6570\u8c03\u8282"
                    font.pixelSize: 14; font.bold: true
                    color: theme.textColor
                }

                ScrollView {
                    id: scrollParent
                    width: parent.width
                    height: parent.height - paramTitle.implicitHeight - 48
                    clip: true

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        contentItem: Rectangle {
                            implicitWidth: 5
                            radius: 2
                            color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5)
                            opacity: 0.4
                        }
                        background: Rectangle { color: "transparent" }
                    }

                    Column {
                        width: scrollParent.width - 10
                        spacing: 6

                        Components.ESlider {
                            id: sTemp
                            label: "\u8272\u6e29 (K)"; from: 1000; to: 30000; stepSize: 100; value: configPage.diskTemp
                            onValueChanged: {
                                configPage.diskTemp = value
                                if (bhCore) bhCore.updateCurrentPresetParam("diskTemp", value)
                            }
                        }
                        Components.ESlider { id: sIncl;  label: "\u76d8\u9762\u503e\u89d2"; from: 0; to: 3; stepSize: 0.01; value: configPage.diskIncl
                            onValueChanged: { configPage.diskIncl = value; if (bhCore) bhCore.updateCurrentPresetParam("diskIncl", value) } }
                        Components.ESlider { id: sRoll;  label: "\u76d8\u9762\u65cb\u8f6c"; from: -1; to: 1; stepSize: 0.01; value: configPage.diskRoll
                            onValueChanged: { configPage.diskRoll = value; if (bhCore) bhCore.updateCurrentPresetParam("diskRoll", value) } }
                        Components.ESlider { id: sInner; label: "\u5185\u534a\u5f84"; from: 0.5; to: 10; stepSize: 0.1; value: configPage.diskInner
                            onValueChanged: { configPage.diskInner = value; if (bhCore) bhCore.updateCurrentPresetParam("diskInner", value) } }
                        Components.ESlider { id: sOuter; label: "\u5916\u534a\u5f84"; from: 1; to: 30; stepSize: 0.1; value: configPage.diskOuter
                            onValueChanged: { configPage.diskOuter = value; if (bhCore) bhCore.updateCurrentPresetParam("diskOuter", value) } }
                        Components.ESlider { id: sOpac;  label: "\u4e0d\u900f\u660e\u5ea6"; from: 0; to: 1; stepSize: 0.01; value: configPage.diskOpac
                            onValueChanged: { configPage.diskOpac = value; if (bhCore) bhCore.updateCurrentPresetParam("diskOpac", value) } }
                        Components.ESlider { id: sDopp;  label: "\u591a\u666e\u52d2\u6548\u5e94"; from: 0; to: 1.5; stepSize: 0.01; value: configPage.diskDopp
                            onValueChanged: { configPage.diskDopp = value; if (bhCore) bhCore.updateCurrentPresetParam("diskDopp", value) } }
                        Components.ESlider { id: sBeam;  label: "\u5149\u675f\u6307\u6570"; from: 0.5; to: 10; stepSize: 0.1; value: configPage.diskBeam
                            onValueChanged: { configPage.diskBeam = value; if (bhCore) bhCore.updateCurrentPresetParam("diskBeam", value) } }
                        Components.ESlider { id: sGain;  label: "\u4eae\u5ea6\u589e\u76ca"; from: 0; to: 5; stepSize: 0.01; value: configPage.diskGain
                            onValueChanged: { configPage.diskGain = value; if (bhCore) bhCore.updateCurrentPresetParam("diskGain", value) } }
                        Components.ESlider { id: sContr; label: "\u6761\u7eb9\u5bf9\u6bd4\u5ea6"; from: 0; to: 3; stepSize: 0.01; value: configPage.diskContr
                            onValueChanged: { configPage.diskContr = value; if (bhCore) bhCore.updateCurrentPresetParam("diskContr", value) } }
                        Components.ESlider { id: sWind;  label: "\u7f20\u7ed5\u7d27\u5ea6"; from: 1; to: 15; stepSize: 0.1; value: configPage.diskWind
                            onValueChanged: { configPage.diskWind = value; if (bhCore) bhCore.updateCurrentPresetParam("diskWind", value) } }
                        Components.ESlider { id: sSpeed; label: "\u65cb\u8f6c\u901f\u5ea6"; from: 0.5; to: 10; stepSize: 0.1; value: configPage.diskSpeed
                            onValueChanged: { configPage.diskSpeed = value; if (bhCore) bhCore.updateCurrentPresetParam("diskSpeed", value) } }
                        Components.ESlider { id: sExpo;  label: "\u66dd\u5149\u5ea6"; from: 0.1; to: 3; stepSize: 0.01; value: configPage.diskExpo
                            onValueChanged: { configPage.diskExpo = value; if (bhCore) bhCore.updateCurrentPresetParam("diskExpo", value) } }
                        Components.ESlider { id: sStar;  label: "\u661f\u7a7a\u4eae\u5ea6"; from: 0; to: 1; stepSize: 0.001; value: configPage.diskStar
                            onValueChanged: { configPage.diskStar = value; if (bhCore) bhCore.updateCurrentPresetParam("diskStar", value) } }
                    }
                }

                RowLayout {
                    spacing: 4
                    Layout.fillWidth: true

                    Components.EButton {
                        text: "\u590d\u5236"
                        size: "xs"
                        iconCharacter: "\uf0c5"
                        backgroundVisible: true
                        onClicked: { if (bhCore) bhCore.copyPreset(configPage.currentPresetIndex) }
                    }
                    Components.EButton {
                        text: "\u7c98\u8d34"
                        size: "xs"
                        iconCharacter: "\uf0ea"
                        backgroundVisible: true
                        onClicked: { if (bhCore) bhCore.pastePreset() }
                    }
                    Item { Layout.fillWidth: true }
                    Components.EButton {
                        text: "\u6062\u590d\u9ed8\u8ba4"
                        size: "xs"
                        iconCharacter: "\uf2f1"
                        backgroundVisible: true
                        onClicked: { if (bhCore) bhCore.resetDefaults() }
                    }
                    Components.EButton {
                        text: "\u52a0\u8f7d\u914d\u7f6e"
                        size: "xs"
                        iconCharacter: "\uf07c"
                        backgroundVisible: true
                        onClicked: { if (bhCore) bhCore.loadConfig() }
                    }
                    Components.EButton {
                        text: "\u4fdd\u5b58\u914d\u7f6e"
                        size: "xs"
                        iconCharacter: "\uf0c7"
                        backgroundVisible: true
                        containerColor: theme.focusColor
                        textColor: "#ffffff"
                        onClicked: { if (bhCore) bhCore.saveConfig() }
                    }
                    Components.EButton {
                        text: "\u5220\u9664\u5217\u8868"
                        size: "xs"
                        iconCharacter: "\uf1f8"
                        backgroundVisible: true
                        containerColor: "#d32f2f"
                        textColor: "#ffffff"
                        onClicked: {
                            if (bhCore && bhCore.presetListModel && bhCore.presetListModel.rowCount() > 1) {
                                bhCore.deleteCurrentList()
                            }
                        }
                    }
                }
            }

            // 閳光偓閳光偓閳光偓 閸? 妫板嫯顔曠粻锛勬倞 閳光偓閳光偓閳光偓
                        Column {
                Layout.preferredWidth: 190
                Layout.fillHeight: true
                spacing: 8

                Text {
                    text: "\u9884\u8bbe\u7ba1\u7406"
                    font.pixelSize: 14; font.bold: true
                    color: theme.textColor
                }

                Components.EButton {
                    text: "\u65b0\u5efa\u9884\u8bbe"
                    size: "xs"
                    iconCharacter: "\uf067"
                    backgroundVisible: true
                    onClicked: { if (bhCore) bhCore.createPreset() }
                }

                RowLayout {
                    width: parent.width
                    Text { text: "\u9884\u8bbe\u540d:"; font.pixelSize: 12; color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5) }
                    TextField {
                        id: renamePresetField
                        Layout.fillWidth: true
                        placeholderText: "\u9884\u8bbe\u540d\u79f0"
                        color: theme.textColor
                        background: Rectangle {
                            radius: 6
                            color: theme.secondaryColor
                            border.color: theme.borderColor
                            border.width: 1
                        }
                        onEditingFinished: { if (bhCore) bhCore.renameCurrentPreset(text) }
                    }
                }

                Components.EButton {
                    text: "\u65b0\u5efa\u5217\u8868"
                    size: "xs"
                    iconCharacter: "\uf03a"
                    backgroundVisible: true
                    onClicked: { if (bhCore) bhCore.createPresetList("\u65b0\u5217\u8868") }
                }

                RowLayout {
                    width: parent.width
                    Text { text: "\u5217\u8868\u540d:"; font.pixelSize: 12; color: Qt.rgba(theme.textColor.r, theme.textColor.g, theme.textColor.b, 0.5) }
                    TextField {
                        id: renameListField
                        Layout.fillWidth: true
                        placeholderText: "\u5217\u8868\u540d\u79f0"
                        text: bhCore ? bhCore.currentListName : ""
                        color: theme.textColor
                        background: Rectangle {
                            radius: 6
                            color: theme.secondaryColor
                            border.color: theme.borderColor
                            border.width: 1
                        }
                        onEditingFinished: { if (bhCore) bhCore.renameCurrentList(text) }
                    }
                }

                Text {
                    id: presetListTitle
                    text: "\u9884\u8bbe\u5217\u8868"
                    font.pixelSize: 14; font.bold: true
                    color: theme.textColor
                }

                ComboBox {
                    id: listCombo
                    width: parent.width - 10
                    model: bhCore ? bhCore.presetListModel : null
                    textRole: "listName"
                    currentIndex: 0
                    onCurrentIndexChanged: { if (bhCore) bhCore.switchToList(currentIndex) }

                    background: Rectangle {
                        radius: 8
                        color: listCombo.hovered
                            ? Qt.darker(theme.primaryColor, 1.15)
                            : theme.primaryColor
                        border.color: listCombo.popup.visible ? theme.focusColor : theme.borderColor
                        border.width: 1

                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    contentItem: Text {
                        leftPadding: 10
                        rightPadding: 20
                        text: listCombo.displayText
                        font.pixelSize: 13
                        color: theme.textColor
                        verticalAlignment: Text.AlignVCenter
                    }

                    indicator: Text {
                        x: listCombo.width - width - 10
                        y: listCombo.topPadding + (listCombo.availableHeight - height) / 2
                        text: listCombo.popup.visible ? "\uf077" : "\uf078"
                        font.family: iconFont.name
                        font.pixelSize: 10
                        color: theme.borderColor
                        verticalAlignment: Text.AlignVCenter
                    }

                    popup: Popup {
                        y: listCombo.height + 4
                        width: listCombo.width
                        implicitHeight: contentItem.implicitHeight + 8
                        padding: 4

                        background: Rectangle {
                            radius: 8
                            color: theme.secondaryColor
                            border.color: theme.focusColor
                            border.width: 1
                        }

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: listCombo.popup.visible ? listCombo.delegateModel : null
                            currentIndex: listCombo.highlightedIndex
                            interactive: false

                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 32
                                radius: 6
                                color: index === listCombo.currentIndex
                                    ? theme.focusColor
                                    : (itemMouse.containsMouse ? Qt.darker(theme.secondaryColor, 1.2) : "transparent")

                                Behavior on color { ColorAnimation { duration: 100 } }

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    text: model.listName
                                    color: index === listCombo.currentIndex ? "#ffffff" : theme.textColor
                                    font.pixelSize: 13
                                    font.bold: index === listCombo.currentIndex
                                    verticalAlignment: Text.AlignVCenter
                                }

                                MouseArea {
                                    id: itemMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        listCombo.currentIndex = index
                                        listCombo.popup.close()
                                    }
                                }
                            }
                        }
                    }
                }

                ListView {
                    id: presetList
                    width: 180
                    height: Math.max(80, parent.height - presetListTitle.implicitHeight - listCombo.implicitHeight - 240)
                    clip: true
                    spacing: 4
                    interactive: !presetList.isDragging

                    model: bhCore ? bhCore.presetModel : null
                    currentIndex: configPage.currentPresetIndex

                    property int dragSourceIndex: -1
                    property bool isDragging: false

                    displaced: Transition {
                        NumberAnimation { properties: "x,y"; duration: 180; easing.type: Easing.OutQuad }
                    }

                    delegate: Rectangle {
                        id: delegateItem
                        width: 146; height: 34
                        radius: 8
                        color: presetList.currentIndex === index
                               ? theme.focusColor
                               : (presetList.isDragging && index === presetList.dragSourceIndex
                                  ? Qt.rgba(1,1,1,0.12)
                                  : (hoverMa.containsMouse ? Qt.rgba(1,1,1,0.05) : "transparent"))
                        opacity: (presetList.isDragging && index === presetList.dragSourceIndex) ? 0.75 : 1.0
                        scale: (presetList.isDragging && index === presetList.dragSourceIndex) ? 1.04 : 1.0
                        z: (presetList.isDragging && index === presetList.dragSourceIndex) ? 100 : 0

                        Behavior on scale { NumberAnimation { duration: 150 } }
                        Behavior on opacity { NumberAnimation { duration: 150 } }

                        TextEdit {
                            anchors.centerIn: parent
                            width: parent.width - 36
                            text: model.presetName
                            font.pixelSize: 13
                            color: theme.textColor
                            readOnly: true
                            selectByMouse: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            clip: true
                        }
                        Timer {
                            id: longPressTimer
                            interval: 400
                            onTriggered: {
                                presetList.isDragging = true
                                presetList.dragSourceIndex = index
                            }
                        }

                        MouseArea {
                            id: hoverMa
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            cursorShape: presetList.isDragging ? Qt.ClosedHandCursor : Qt.PointingHandCursor

                            onPressed: longPressTimer.start()

                            onPositionChanged: function(mouse) {
                                if (presetList.isDragging && bhCore) {
                                    var pt = delegateItem.mapToItem(presetList.contentItem, mouse.x, mouse.y)
                                    var newIdx = Math.floor(pt.y / (delegateItem.height + presetList.spacing))
                                    newIdx = Math.max(0, Math.min(newIdx, presetList.count - 1))
                                    if (newIdx !== presetList.dragSourceIndex) {
                                        bhCore.movePreset(presetList.dragSourceIndex, newIdx)
                                        presetList.dragSourceIndex = newIdx
                                    }
                                }
                            }

                            onReleased: {
                                longPressTimer.stop()
                                if (presetList.isDragging) {
                                    presetList.isDragging = false
                                    presetList.currentIndex = presetList.dragSourceIndex
                                    configPage.currentPresetIndex = presetList.dragSourceIndex
                                } else {
                                    presetList.currentIndex = index
                                    configPage.currentPresetIndex = index
                                    if (bhCore) bhCore.selectPreset(index)
                                }
                            }

                            onCanceled: {
                                longPressTimer.stop()
                                presetList.isDragging = false
                            }
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: "\uf00d"
                            font.family: iconFont.name
                            font.pixelSize: 14
                            color: "#ff5252"
                            visible: hoverMa.containsMouse
                            opacity: deleteMouse.containsMouse ? 1.0 : 0.5

                            Behavior on opacity { NumberAnimation { duration: 100 } }

                            MouseArea {
                                id: deleteMouse
                                anchors.fill: parent
                                anchors.margins: -4
                                cursorShape: Qt.PointingHandCursor
                                onClicked: { if (bhCore) bhCore.removePreset(index) }
                            }
                        }
                    }
                }
            }
        }
    }

// ========== 闁衡偓閹佷海濡澘瀚～宥咁嚕閸︻厾宕?==========
    Components.BlackholeLargePreview {
        id: largePreview

        diskTemp:  configPage.diskTemp
        diskIncl:  configPage.diskIncl
        diskRoll:  configPage.diskRoll
        diskInner: configPage.diskInner
        diskOuter: configPage.diskOuter
        diskOpac:  configPage.diskOpac
        diskDopp:  configPage.diskDopp
        diskBeam:  configPage.diskBeam
        diskGain:  configPage.diskGain
        diskContr: configPage.diskContr
        diskWind:  configPage.diskWind
        diskSpeed: configPage.diskSpeed
        diskExpo:  configPage.diskExpo
        diskStar:  configPage.diskStar
    }
}
