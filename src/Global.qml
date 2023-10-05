pragma Singleton
import QtQuick 2.0

QtObject {
    id: qtObject

    readonly property int _UnknownServiceType: 0
    readonly property int _SoftwareServiceType: 1

    readonly property int _NotConnectedConnectionState: 10
    readonly property int _ConnectingConnectionState: 20
    readonly property int _ConnectedConnectionState: 30

    property var windowChat
    property var windowSettings

    property int windowSettingsServiceIndex: 0

    property bool windowChatShowViewersCount : true

    property bool windowChatMessageShowAvatar : true
    property bool windowChatMessageShowAuthorName : true
    property bool windowChatMessageShowPlatformIcon : true
    property bool windowChatMessageShowTime : false

    property real windowChatMessageAvatarSize : 40
    property string windowChatMessageAvatarShape : "round"

    property real windowChatMessageAuthorNameFontSize : 12
    property real windowChatMessageTextFontSize : 12
    property real windowChatMessageTimeFontSize : 12

    property real windowChatMessageFrameBorderWidth: 0//1
    property color windowChatMessageFrameBorderColor: "#003760"
    property color windowChatMessageFrameBackgroundColor: "transparent"
    property real windowChatMessageFrameCornerRadius: 4//0

    function save(){
        windowChat.saveGlobalSettings()
    }

    function openSettingsWindow() {
        if (typeof(windowSettings) == "undefined")
        {
            var component = Qt.createComponent("qrc:/settings.qml")
            windowSettings = component.createObject(windowChat)
        }

        windowSettings.show()
    }
}
