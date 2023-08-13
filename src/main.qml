import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.0
import Qt.labs.settings 1.1
import AxelChat.ChatHandler 1.0
import AxelChat.ChatService 1.0
import AxelChat.UpdateChecker 1.0
import AxelChat.Tray 1.0
import QtQuick.Window 2.15
import "my_components" as MyComponents
import "setting_pages" as SettingPages
import "."

ApplicationWindow {
    id: root
    visible: true
    width: 300
    height: 600
    minimumHeight: 200
    minimumWidth:  150
    title: Qt.application.name

    onVisibleChanged: {
        if (visible) {
            qmlUtils.updateWindowStyle(this)
        }
    }

    flags: {
        var windowFlags = Qt.Window
        if (_customizeWindowHint) {
            windowFlags |= Qt.CustomizeWindowHint
        }

        if (Global.windowChatTransparentForInput) {
            windowFlags |= Qt.WindowTransparentForInput
        }

        if (Global.windowChatSystemWindowFrame) {
            windowFlags |= Qt.WindowTitleHint | Qt.WindowSystemMenuHint |
                    Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint
        }
        else {
            windowFlags |= Qt.FramelessWindowHint
        }

        if (Global.windowChatStayOnTop) {
            windowFlags |= Qt.WindowStaysOnTopHint
        }

        return windowFlags
    }

    property bool _customizeWindowHint: false
    function refreshByFlags() {
        _customizeWindowHint = !_customizeWindowHint
        _customizeWindowHint = !_customizeWindowHint
    }

    opacity: Global.windowChatWindowOpacity
    color: Qt.rgba(0, 0, 0, Global.windowChatBackgroundOpacity)

    property var settingsWindow;
    property var authorWindow: null
    property var updatesWindow;

    Settings {
        category: "chat_window"
        property alias window_width:  root.width;
        property alias window_height: root.height;
    }

    function loadGlobalSettings() {
        Global.windowChatTransparentForInput = qmlUtils.valueBool("windowChatTransparentForInput",      Global.windowChatTransparentForInput)

        Global.windowChatStayOnTop          = qmlUtils.valueBool("windowChatStayOnTop",                 Global.windowChatStayOnTop)
        Global.windowChatSystemWindowFrame  = qmlUtils.valueBool("windowChatSystemWindowFrame",         Global.windowChatSystemWindowFrame)
        Global.windowChatWindowOpacity      = qmlUtils.valueReal("windowChatWindowOpacity",             Global.windowChatWindowOpacity)
        Global.windowChatBackgroundOpacity  = qmlUtils.valueReal("windowChatBackgroundOpacity",         Global.windowChatBackgroundOpacity)

        Global.windowChatMessageShowAvatar          = qmlUtils.valueBool("windowChatMessageShowAvatar",             Global.windowChatMessageShowAvatar)
        Global.windowChatMessageShowAuthorName      = qmlUtils.valueBool("windowChatMessageShowAuthorName",         Global.windowChatMessageShowAuthorName)
        Global.windowChatMessageShowTime            = qmlUtils.valueBool("windowChatMessageShowTime",               Global.windowChatMessageShowTime)
        Global.windowChatMessageShowPlatformIcon    = qmlUtils.valueBool("windowChatMessageShowPlatformIcon",       Global.windowChatMessageShowPlatformIcon)

        Global.windowChatMessageAvatarSize          = qmlUtils.valueReal("windowChatMessageAvatarSize",             Global.windowChatMessageAvatarSize)

        Global.windowChatMessageAuthorNameFontSize  = qmlUtils.valueReal("windowChatMessageAuthorNameFontSize",     Global.windowChatMessageAuthorNameFontSize)
        Global.windowChatMessageTextFontSize        = qmlUtils.valueReal("windowChatMessageTextFontSize",           Global.windowChatMessageTextFontSize)
        Global.windowChatMessageTimeFontSize        = qmlUtils.valueReal("windowChatMessageTimeFontSize",           Global.windowChatMessageTimeFontSize)

        Global.windowChatShowViewersCount           = qmlUtils.valueReal("windowChatShowViewersCount",              Global.windowChatShowViewersCount)
    }

    function saveGlobalSettings() {
        qmlUtils.setValue("windowChatTransparentForInput",  Global.windowChatTransparentForInput)

        qmlUtils.setValue("windowChatStayOnTop",            Global.windowChatStayOnTop)
        qmlUtils.setValue("windowChatSystemWindowFrame",    Global.windowChatSystemWindowFrame)
        qmlUtils.setValue("windowChatWindowOpacity",        Global.windowChatWindowOpacity)
        qmlUtils.setValue("windowChatBackgroundOpacity",    Global.windowChatBackgroundOpacity)

        qmlUtils.setValue("windowChatMessageShowAvatar",        Global.windowChatMessageShowAvatar)
        qmlUtils.setValue("windowChatMessageShowAuthorName",    Global.windowChatMessageShowAuthorName)
        qmlUtils.setValue("windowChatMessageShowTime",          Global.windowChatMessageShowTime)
        qmlUtils.setValue("windowChatMessageShowPlatformIcon",  Global.windowChatMessageShowPlatformIcon)

        qmlUtils.setValue("windowChatMessageAvatarSize",  Global.windowChatMessageAvatarSize)

        qmlUtils.setValue("windowChatMessageAuthorNameFontSize",    Global.windowChatMessageAuthorNameFontSize)
        qmlUtils.setValue("windowChatMessageTextFontSize",          Global.windowChatMessageTextFontSize)
        qmlUtils.setValue("windowChatMessageTimeFontSize",          Global.windowChatMessageTimeFontSize)

        qmlUtils.setValue("windowChatShowViewersCount",             Global.windowChatShowViewersCount)
    }

    function createImgHtmlTag(imgurl, size)
    {
        return "<img align=\"top\" src=\"" + imgurl + "\" height=\"" + size.toString() + "\" width=\"" + size.toString() + "\"/>"
    }

    onClosing: {
        saveGlobalSettings()
    }

    Component.onCompleted: {
        Global.windowChat = this
        loadGlobalSettings()

        //Update notification window
        if (updateChecker.autoRequested)
        {
            var component = Qt.createComponent("qrc:/updatesnotification.qml");
            root.updatesWindow = component.createObject(root);
        }

        refreshByFlags()
    }

    Connections {
        target: updateChecker

        function onReplied() {
            if (updateChecker.replyState === UpdateChecker.NewVersionAvailable)
            {
                if (root.updatesWindow)
                {
                    root.updatesWindow.show()
                    root.updatesWindow = undefined;
                }
            }
        }
    }

    Connections {
        target: tray

        function onTriggered(actionName) {
            if (actionName === "settings") {
                openSettingsWindow()
            }
            else if (actionName === "input_transparent_toggle") {
                Global.windowChatTransparentForInput = !Global.windowChatTransparentForInput
            }
            else if (actionName === "close_application") {
                Qt.quit()
            }
            else if (actionName === "clear_messages") {
                chatHandler.clearMessages()
            }
            else {
                console.log("Unknown tray action '", actionName, "'")
            }
        }
    }

    Connections {
        target: Global

        function onWindowChatTransparentForInputChanged() {
            tray.ignoredMouse = Global.windowChatTransparentForInput
        }
    }

    function openSettingsWindow() {
        if (typeof(settingsWindow) == "undefined")
        {
            var component = Qt.createComponent("qrc:/settings.qml")
            settingsWindow = component.createObject(root)
        }

        settingsWindow.show()
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: {
            openSettingsWindow()
        }

        onWheel: {
            chatPage.listView.flick(wheel.angleDelta.x * 8, wheel.angleDelta.y * 8)
            wheel.accepted=true
        }
    }

    ChatPage {
        id: chatPage
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: bottomPanel.visible ? bottomPanel.top : parent.bottom
        clip: true
    }

    WaitAnimationPage {
        anchors.fill: parent
        visible: chatPage.listView.count == 0
    }

    TitlesPage {
        anchors.fill: parent
    }

    Rectangle {
        id: bottomPanel
        visible: Global.windowChatShowViewersCount
        height: 26
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: "transparent"

        Row {
            id: bottomPanelRow
            anchors.fill: parent
            spacing: 8
            leftPadding: 4
            rightPadding: 4
            topPadding: 2
            bottomPadding: 6
        }

        Component.onCompleted: {
            for (var i = 0; i < chatHandler.getServicesCount(); i++)
            {

            Qt.createQmlObject(
"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.12

Row {
    property var chatService: chatHandler.getServiceAtIndex(" + String("%1").arg(i) + ")

    anchors.verticalCenter: parent.verticalCenter
    spacing: 8

    visible: chatService.enabled

    Image {
        mipmap: true
        height: 18
        width: height
        anchors.verticalCenter: parent.verticalCenter
        source: chatService.getIconUrl()
        fillMode: Image.PreserveAspectFit

        Rectangle {
            id: stateIndicator
            width: 10
            height: width
            x: parent.width - width * 0.6
            y: parent.height - height * 0.6
            border.width: 2
            radius: width / 2
            border.color: \"black\"

            property var status: chatService.connectionStateType

            color: {
                if (status === Global._ConnectedConnectionStateType) {
                    return \"lime\"
                }

                return \"red\"
            }
        }
    }

    Text {
        anchors.verticalCenter: parent.verticalCenter
        style: Text.Outline
        font.pointSize: 10
        color: \"white\"
        visible: chatService.viewersCount != -1

        text: String(\"%1\").arg(chatService.viewersCount)
    }
}
", bottomPanelRow)

            }
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 4
            spacing: 8
            visible: chatHandler.viewersTotalCount !== -1 && chatHandler.connectedCount > 1

            Image {
                mipmap: true
                height: 20
                width: height
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/resources/images/eye-svgrepo-com.svg"
                fillMode: Image.PreserveAspectFit
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                style: Text.Outline
                font.pointSize: 10
                color: "white"

                text: String("%1").arg(chatHandler.viewersTotalCount)
            }
        }
    }
}


/*##^##
Designer {
    D{i:0;formeditorZoom:2}
}
##^##*/
