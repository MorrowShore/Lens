import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.12
import AxelChat.ChatHandler 1.0
import AxelChat.ChatService 1.0
import AxelChat.UIElementBridge 1.0
import "../my_components" as MyComponents
import "../"

ScrollView {
    id: root
    clip: true
    horizontalPadding: 10
    verticalPadding: 6
    contentHeight: column.implicitHeight
    contentWidth: column.implicitWidth

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
                visible:  chatService.connectionStateType === Global._ConnectingConnectionStateType
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

                    if (chatService.connectionStateType === Global._NotConnectedConnectionStateType) {
                        return "qrc:/resources/images/error-alt-svgrepo-com.svg"
                    }
                    else if (chatService.connectionStateType === Global._ConnectedConnectionStateType) {
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

        Column {
            id: newElementsColumn
            spacing: 6
        }

        Component.onCompleted: {
            var Types = {
                Label: 10,
                LineEdit: 20,
                Button: 30,
                Switch: 32,
            }

            var container = newElementsColumn

            var labelComponent = Qt.createComponent("../my_components/BridgedLabel.qml")
            var lineEditComponent = Qt.createComponent("../my_components/BridgedLineEdit.qml")
            var buttonComponent = Qt.createComponent("../my_components/BridgedButton.qml")
            var switchComponent = Qt.createComponent("../my_components/BridgedSwitch.qml")

            for (var i = 0; i < chatService.getUIElementBridgesCount(); ++i)
            {
                var type = chatService.getUIElementBridgeType(i)
                switch (type)
                {
                case Types.Label:
                    chatService.bindQmlItem(i, labelComponent.createObject(container))
                    break

                case Types.LineEdit:
                    chatService.bindQmlItem(i, lineEditComponent.createObject(container))
                    break

                case Types.Button:
                    chatService.bindQmlItem(i, buttonComponent.createObject(container))
                    break

                case Types.Switch:
                    chatService.bindQmlItem(i, switchComponent.createObject(container))
                    break

                default:
                    console.error("Unknown bridge type ", type)
                    break
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
    }
}
