import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.12
import AxelChat.ChatHandler 1.0
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
        chatService = chatHandler.getServiceAtIndex(Global.windowSettingsServiceIndex)
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

                    return chatService.stateDescription
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
                    Global.windowSettings.showInfo(qsTr("Allow to display emotes from %1. To display channel (custom) emotes, login to a Twitch account that is linked to these services").arg(parent.servicesNames))
                }
            }
        }
    }
}
