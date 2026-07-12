// IdleListConfig.qml — 空闲检测名单管理
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Root
import "../components" as Components

Item {
    id: listPage

    anchors.fill: parent
    anchors.margins: 20

    property var bhCore: null

    property var whitelist: {
        if (bhCore)
            return bhCore.idleWhitelist
        return []
    }
    property var blacklist: {
        if (bhCore)
            return bhCore.idleBlacklist
        return ["vlc", "mpv", "potplayer", "mpc", "wmplayer", "bilibili", "iqiyi", "youku", "芒果TV", "腾讯视频"]
    }
    property var forceBlocklist: {
        if (bhCore)
            return bhCore.idleForceBlocklist
        return []
    }

    function containsIgnoreCase(values, candidate) {
        var lowerCandidate = candidate.toLowerCase()
        for (var i = 0; i < values.length; i++) {
            if (String(values[i]).toLowerCase() === lowerCandidate)
                return true
        }
        return false
    }

    property string pickerTarget: ""

    function valuesForTarget(target) {
        if (target === "whitelist") return listPage.whitelist
        if (target === "mediaHints") return listPage.blacklist
        return listPage.forceBlocklist
    }

    function commitTarget(target, values) {
        if (target === "whitelist") {
            listPage.whitelist = values
            if (bhCore) bhCore.idleWhitelist = values
        } else if (target === "mediaHints") {
            listPage.blacklist = values
            if (bhCore) bhCore.idleBlacklist = values
        } else {
            listPage.forceBlocklist = values
            if (bhCore) bhCore.idleForceBlocklist = values
        }
    }

    function addEntry(target, processName) {
        var value = String(processName).trim()
        var current = valuesForTarget(target)
        if (value === "" || containsIgnoreCase(current, value)) return
        var next = []
        for (var i = 0; i < current.length; i++) next.push(String(current[i]))
        next.push(value)
        commitTarget(target, next)
    }

    function removeEntry(target, processName) {
        var current = valuesForTarget(target)
        var next = []
        for (var i = 0; i < current.length; i++) {
            if (String(current[i]).toLowerCase() !== String(processName).toLowerCase())
                next.push(String(current[i]))
        }
        commitTarget(target, next)
    }

    function openRunningPicker(target) {
        if (!bhCore) return
        pickerTarget = target
        runningPicker.open(bhCore.runningApplications())
    }

    function chooseExecutable(target) {
        if (!bhCore) return
        var application = bhCore.chooseExecutable()
        if (application && application.processName)
            addEntry(target, application.processName)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        // === 标题 ===
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "\uf502"
                font.family: iconFont.name
                font.pixelSize: 22
                color: theme.focusColor
            }
            Text {
                text: "空闲检测名单"
                font.pixelSize: 22
                font.bold: true
                color: theme.focusColor
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Components.IdleListPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "始终允许触发"
                description: "前台命中时忽略媒体与游戏检测"
                accentColor: theme.focusColor
                values: listPage.whitelist
                onManualAddRequested: function(name) { listPage.addEntry("whitelist", name) }
                onRunningPickerRequested: listPage.openRunningPicker("whitelist")
                onExecutablePickerRequested: listPage.chooseExecutable("whitelist")
                onRemoveRequested: function(name) { listPage.removeEntry("whitelist", name) }
            }

            Components.IdleListPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "媒体识别提示"
                description: "仅在实际播放且检测到媒体信号时阻止"
                accentColor: theme.focusColor
                values: listPage.blacklist
                onManualAddRequested: function(name) { listPage.addEntry("mediaHints", name) }
                onRunningPickerRequested: listPage.openRunningPicker("mediaHints")
                onExecutablePickerRequested: listPage.chooseExecutable("mediaHints")
                onRemoveRequested: function(name) { listPage.removeEntry("mediaHints", name) }
            }

            Components.IdleListPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "前台强制不触发"
                description: "前台命中时无条件阻止，切到后台后失效"
                accentColor: "#ff6b6b"
                values: listPage.forceBlocklist
                onManualAddRequested: function(name) { listPage.addEntry("forceBlocklist", name) }
                onRunningPickerRequested: listPage.openRunningPicker("forceBlocklist")
                onExecutablePickerRequested: listPage.chooseExecutable("forceBlocklist")
                onRemoveRequested: function(name) { listPage.removeEntry("forceBlocklist", name) }
            }
        }
    }

    Components.RunningApplicationPicker {
        id: runningPicker
        onRefreshRequested: {
            if (bhCore) runningPicker.open(bhCore.runningApplications())
        }
        onApplicationSelected: function(application) {
            if (application && application.processName)
                listPage.addEntry(listPage.pickerTarget, application.processName)
        }
    }
}
