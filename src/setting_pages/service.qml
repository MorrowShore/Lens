import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.12
import AxelChat.ChatHandler 1.0
import AxelChat.ChatService 1.0
import "../my_components" as MyComponents
import "../"

ScrollView {
    id: root
    clip: true
    padding: 6
    contentHeight: column.implicitHeight
    contentWidth: column.implicitWidth

    property var chatService: chatHandler.getServiceAtIndex(Global.windowSettingsServiceIndex)

    Column {
        id: column
        spacing: 6

        Row {
            spacing: 12

            Image {
                mipmap: true
                height: 40
                width: height
                anchors.verticalCenter: parent.verticalCenter
                source: chatService.getIconUrl()
                fillMode: Image.PreserveAspectFit
            }

            Label {
                text: chatService.getNameLocalized()
                font.bold: true
                font.pixelSize: 25
                color: Material.foreground
                anchors.verticalCenter: parent.verticalCenter
            }

            BusyIndicator {
                id: busyIndicator
                visible: chatService.connectionStateType === Global._ConnectingConnectionStateType
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
                    if (chatService.connectionStateType === Global._NotConnectedConnectionStateType)
                    {
                        return "qrc:/resources/images/alert1.svg"
                    }
                    else if (chatService.connectionStateType === Global._ConnectedConnectionStateType)
                    {
                        return "qrc:/resources/images/tick.svg"
                    }

                    return "";
                }
            }

            Label {
                id: labelState
                anchors.verticalCenter: parent.verticalCenter
                font.pointSize: 14
                text: chatService.stateDescription
            }
        }

        Row {
            spacing: 6

            Label {
                color: Material.accentColor
                text: qsTr("Broadcast:")
                anchors.verticalCenter: parent.verticalCenter
                font.bold: true
                font.pixelSize: 20
            }

            MyComponents.MyTextField {
                id: textFieldUserSpecifiedLink
                placeholderText: qsTr("Link...")
                anchors.verticalCenter: parent.verticalCenter
                width: 400

                Component.onCompleted: {
                    text = chatService.getBroadcastLink()
                }

                onTextChanged: {
                    chatService.setBroadcastLink(text)
                }
            }

            Button {
                anchors.verticalCenter: parent.verticalCenter
                highlighted: true
                text: qsTr("Paste")
                display: AbstractButton.TextBesideIcon
                icon.source: "qrc:/resources/images/clipboard-paste-button.svg"

                onClicked: {
                    if (clipboard.text.length !== 0)
                    {
                        textFieldUserSpecifiedLink.text = clipboard.text;
                        textFieldUserSpecifiedLink.deselect();
                    }
                }
            }
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
                text: qsTr("Broadcast")
                anchors.verticalCenter: parent.verticalCenter
                visible: chatService.broadcastUrl.toString().length !== 0
                onClicked: {
                    Qt.openUrlExternally(chatService.broadcastUrl)
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
    }
}