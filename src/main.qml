import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.0
import Qt.labs.settings 1.1
import AxelChat.ChatManager 1.0
import AxelChat.ChatService 1.0
import AxelChat.UpdateChecker 1.0
import QtQuick.Window 2.15
import "my_components" as MyComponents
import "setting_pages" as SettingPages
import "."

Item {
    id: root
    visible: true

    onVisibleChanged: {
        if (visible) {
            qmlUtils.updateWindowStyle(this)
        }
    }

    property var authorWindow: null
    property var updatesWindow;

    Settings {
        category: "chat_window"
    }

    function loadGlobalSettings() {
        Global.windowChatMessageShowAvatar          = qmlUtils.valueBool("windowChatMessageShowAvatar",             Global.windowChatMessageShowAvatar)
        Global.windowChatMessageShowAuthorName      = qmlUtils.valueBool("windowChatMessageShowAuthorName",         Global.windowChatMessageShowAuthorName)
        Global.windowChatMessageShowTime            = qmlUtils.valueBool("windowChatMessageShowTime",               Global.windowChatMessageShowTime)
        Global.windowChatMessageShowPlatformIcon    = qmlUtils.valueBool("windowChatMessageShowPlatformIcon",       Global.windowChatMessageShowPlatformIcon)

        Global.windowChatMessageAvatarSize          = qmlUtils.valueReal("windowChatMessageAvatarSize",             Global.windowChatMessageAvatarSize)

        Global.windowChatMessageAuthorNameFontSize  = qmlUtils.valueReal("windowChatMessageAuthorNameFontSize",     Global.windowChatMessageAuthorNameFontSize)
        Global.windowChatMessageTextFontSize        = qmlUtils.valueReal("windowChatMessageTextFontSize",           Global.windowChatMessageTextFontSize)
        Global.windowChatMessageTimeFontSize        = qmlUtils.valueReal("windowChatMessageTimeFontSize",           Global.windowChatMessageTimeFontSize)

        Global.windowChatShowViewersCount           = qmlUtils.valueBool("windowChatShowViewersCount",              Global.windowChatShowViewersCount)
    }

    function saveGlobalSettings() {
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

    Component.onDestruction: {
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
        target: qmlUtils
        function onTriggered(action) {
            if (action === "open_settings_window") {
                Global.openSettingsWindow()
            }
            else {
                console.log("Unknown triggered action '", action, "'")
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: {
            Global.openSettingsWindow()
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
        visible: !titlesPage.visible
    }

    WaitAnimationPage {
        anchors.fill: parent
        visible: chatPage.listView.count == 0 && !titlesPage.visible
    }

    TitlesPage {
        id: titlesPage
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: bottomPanel.visible ? bottomPanel.top : parent.bottom
        visible: false
        duration: 4000
        property bool shown: false

        onCountChanged: {
            if (count > 0 && !shown) {
                visible = true
                shown = true
                start()
            }
        }

        onStopped: () => {
            visible = false
        }
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
            for (var i = 0; i < chatManager.getServicesCount(); i++)
            {

            Qt.createQmlObject(
"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.12

Row {
    property var chatService: chatManager.getServiceAtIndex(" + String("%1").arg(i) + ")

    anchors.verticalCenter: parent.verticalCenter
    spacing: 4

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
            visible: status !== Global._ConnectedConnectionState

            property var status: chatService.connectionState

            color: \"red\"
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
            visible: chatManager.viewersTotalCount !== -1 && chatManager.knownViewesServicesMoreOne

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

                text: String("%1").arg(chatManager.viewersTotalCount)
            }
        }
    }
}


/*##^##
Designer {
    D{i:0;formeditorZoom:2}
}
##^##*/
