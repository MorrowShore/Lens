pragma Singleton
import QtQuick 2.0

QtObject {
    id: qtObject

    readonly property int _UnknownServiceType: -1
    readonly property int _SoftwareServiceType: 10
    readonly property int _YouTubeServiceType: 100
    readonly property int _TwitchServiceType: 101
    readonly property int _TrovoServiceType: 102
    readonly property int _GoodGameServiceType: 103
    readonly property int _VkPlayLiveServiceType: 104
    readonly property int _TelegramServiceType: 105
    readonly property int _DiscordServiceType: 106

    readonly property int _NotConnectedConnectionStateType: 10
    readonly property int _ConnectingConnectionStateType: 20
    readonly property int _ConnectedConnectionStateType: 30

    readonly property int _UnknownParameterType: -1
    readonly property int _LineEditParameterType: 10
    readonly property int _ButtonParameterType: 20
    readonly property int _LabelParameterType: 21
    readonly property int _SwitchParameterType: 22

    readonly property int _VisibleParameterFlag: 1
    readonly property int _PasswordEchoParameterFlag: 10

    property var windowChat
    property var windowSettings

    property int windowSettingsServiceIndex: 0

    property bool windowChatTransparentForInput : false

    property bool windowChatShowViewersCount : true

    property bool windowChatStayOnTop : true
    property bool windowChatSystemWindowFrame : true
    property real windowChatWindowOpacity: 1
    property real windowChatBackgroundOpacity: 0.40

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
}
