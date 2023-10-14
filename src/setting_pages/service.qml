import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.12
import AxelChat.ChatManager 1.0
import AxelChat.ChatService 1.0
import AxelChat.UIBridgeElement 1.0
import AxelChat.UIBridge 1.0
import "../my_components" as MyComponents
import "../UIBridge" as UIBridge
import "../"

ScrollView {
    id: root
    clip: true
    horizontalPadding: 10
    verticalPadding: 6
    contentHeight: column.implicitHeight
    contentWidth: column.implicitWidth
    ScrollBar.horizontal.policy: width < contentWidth ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: height < contentHeight ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

    property var chatService: null
    Component.onCompleted: {
        chatService = chatManager.getServiceAtIndex(Global.windowSettingsServiceIndex)
    }

    Column {
        id: column
        spacing: 6

        Row {
            spacing: 12

            Switch {
                property bool enableDataChanging: true

                Component.onCompleted: {
                    enableDataChanging = false
                    checked = chatService.enabled
                    enableDataChanging = true
                }

                onCheckedChanged: {
                    if (enableDataChanging) {
                        chatService.enabled = checked
                    }
                }
            }

            Image {
                mipmap: true
                height: 40
                width: height
                anchors.verticalCenter: parent.verticalCenter
                source: chatService.getIconUrl()
                fillMode: Image.PreserveAspectFit
            }

            Label {
                text: chatService.getName()
                font.bold: true
                font.pixelSize: 25
                color: Material.foreground
                anchors.verticalCenter: parent.verticalCenter
            }

            BusyIndicator {
                id: busyIndicator
                visible:  chatService.connectionState === Global._ConnectingConnectionState
                height: 40
                width: height
                anchors.verticalCenter: parent.verticalCenter
                antialiasing: false
                smooth: false
            }

            Image {
                visible: !busyIndicator.visible
                height: busyIndicator.height
                width: height
                anchors.verticalCenter: parent.verticalCenter
                mipmap: true
                source: {
                    if (!chatService.enabled) {
                        return "qrc:/resources/images/close.svg"
                    }

                    if (chatService.connectionState === Global._NotConnectedConnectionState) {
                        return "qrc:/resources/images/error-alt-svgrepo-com.svg"
                    }
                    else if (chatService.connectionState === Global._ConnectedConnectionState) {
                        if (chatService.warnings.length !== 0) {
                            return "qrc:/resources/images/tick-with-warnings.svg"
                        }

                        return "qrc:/resources/images/tick.svg"
                    }

                    return "";
                }
            }

            Label {
                id: labelState
                anchors.verticalCenter: parent.verticalCenter
                font.pointSize: 14
                text: {
                    if (!chatService.enabled) {
                        return qsTr("Disabled")
                    }

                    if (chatService.connectionState === _NotConnectedConnectionState) {
                        return chatService.mainError
                    }

                    if (chatService.connectionState === _ConnectingConnectionState) {
                        return qsTr("Connecting...")
                    }

                    if (chatService.connectionState === _ConnectedConnectionState) {
                        if (chatService.warnings.length !== 0) {
                            return qsTr("Connected but there are problems")
                        }

                        return qsTr("Connected")
                    }

                    return "<unknown_state>"
                }
            }
        }

        UIBridge.UIBridgeContainer {
            bridge: chatService.getUiBridge()
        }

        Row {
            spacing: 6

            Button {
                text: qsTr("Control Panel")
                anchors.verticalCenter: parent.verticalCenter
                visible: chatService.controlPanelUrl.toString().length !== 0
                icon.source: "qrc:/resources/images/youtube-control-panel.svg"
                onClicked: {
                    Qt.openUrlExternally(chatService.controlPanelUrl)
                }
            }

            Button {
                text: qsTr("Stream")
                anchors.verticalCenter: parent.verticalCenter
                visible: chatService.streamUrl.toString().length !== 0
                onClicked: {
                    Qt.openUrlExternally(chatService.streamUrl)
                }
            }

            Button {
                text: qsTr("Chat")
                anchors.verticalCenter: parent.verticalCenter
                visible: chatService.chatUrl.toString().length !== 0
                onClicked: {
                    Qt.openUrlExternally(chatService.chatUrl)
                }
            }
        }

        Row {
            spacing: 6

            property string servicesNames: "BetterTTV, FrankerFaceZ, 7TV"

            Switch {
                text: qsTr("%1 emotes").arg(parent.servicesNames)

                property bool enableDataChanging: true

                Component.onCompleted: {
                    enableDataChanging = false
                    checked = chatService.enabledThirdPartyEmotes
                    enableDataChanging = true
                }

                onCheckedChanged: {
                    if (enableDataChanging) {
                        chatService.enabledThirdPartyEmotes = checked
                    }
                }
            }

            RoundButton {
                anchors.verticalCenter: parent.verticalCenter
                flat: true
                icon.source: "qrc:/resources/images/help-round-button.svg"
                onClicked: {
                    Global.windowSettings.showInfo(qsTr("Enables display of emoji from %1 for the current platform. To display custom emoji for the current platform, log into your Twitch account, which is linked to %1").arg(parent.servicesNames))
                }
            }
        }
    }
}
