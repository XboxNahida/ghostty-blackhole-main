// ETimeDisplay.qml
import QtQuick
import QtQuick.Controls

Item {
    id: timeDisplay
    width: 100
    height: 100

    property bool is24Hour: true
    property string currentHour: ""
    property string currentMinute: ""
    property bool separatorVisible: true

    Column {
        anchors.centerIn: parent
        spacing: 4
        width: parent.width

        Text {
            id: hourText
            text: currentHour
            font.pixelSize: 60
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            color: theme.focusColor
            width: parent.width
        }

        Rectangle {
            id: separatorLine
            width: 60
            height: 2
            color: theme.textColor
            anchors.horizontalCenter: parent.horizontalCenter
            visible: separatorVisible
            radius: 1
        }

        Text {
            id: minuteText
            text: currentMinute
            font.pixelSize: 60
            horizontalAlignment: Text.AlignHCenter
            color: theme.textColor
            width: parent.width
        }
    }

    Timer {
        id: updateTimer
        interval: 1000
        running: true
        repeat: true

        onTriggered: updateTime()
    }

    function updateTime() {
        var now = new Date()
        var h = now.getHours()
        if (!is24Hour) {
            h = h % 12
            if (h === 0) h = 12
        }
        currentHour = h.toString().padStart(2, "0")
        currentMinute = now.getMinutes().toString().padStart(2, "0")
    }

    Component.onCompleted: updateTime()
}
