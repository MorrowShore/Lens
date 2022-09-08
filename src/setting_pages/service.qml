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
    horizontalPadding: 10
    verticalPadding: 6
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
                text: chatService.getName()
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

        Column {
            id: parametersColumn
            spacing: 6
        }

        Component.onCompleted: {
            for (var i = 0; i < chatService.getParametersCount(); ++i)
            {
                var row = Qt.createQmlObject("import QtQuick 2.0; Row { spacing: 6 }", parametersColumn)

                var type = chatService.getParameterType(i)
                if (type === Global._StringParameterType)
                {//isParameterHasFlag _PasswordEchoParameterFlag
                    Qt.createQmlObject("
                        import QtQuick 2.0
                        import QtQuick.Controls 2.15
                        import QtQuick.Controls.Material 2.12

                        Label {
                            text: chatService.getParameterName(" + String("%1").arg(i) + ") + \":\"
                            font.pixelSize: 20
                            anchors.verticalCenter: parent.verticalCenter
                            font.bold: true
                            color: \"" + Material.accentColor + "\"
                        }", row)

                    Qt.createQmlObject("
                        import QtQuick 2.0
                        import QtQuick.Controls 2.15
                        import QtQuick.Controls.Material 2.12
                        import AxelChat.ChatService 1.0
                        import \"../my_components\" as MyComponents

                        MyComponents.MyTextField {
                            width: 400
                            anchors.verticalCenter: parent.verticalCenter
                            echoMode: chatService.isParameterHasFlag(" + String("%1").arg(i) + ", " + String("%1").arg(Global._PasswordEchoParameterFlag) + ") ? TextInput.Password : TextInput.Normal

                            Component.onCompleted: {
                                text = chatService.getParameterValue(" + String("%1").arg(i) + ")
                                placeholderText = chatService.getParameterPlaceholder(" + String("%1").arg(i) + ")
                            }

                            onTextChanged: {
                                chatService.setParameterValue(" + String("%1").arg(i) + ", text)
                            }
                        }
                        ", row)
                }
                else if (type === Global._ButtonUrlParameterType)
                {
                    Qt.createQmlObject("
                        import QtQuick 2.0
                        import QtQuick.Controls 2.15
                        import QtQuick.Controls.Material 2.12

                        Button {
                            text: chatService.getParameterName(" + String("%1").arg(i) + ")
                            anchors.verticalCenter: parent.verticalCenter
                            display: AbstractButton.TextBesideIcon

                            onClicked: {
                                Qt.openUrlExternally(chatService.getParameterValue(" + String("%1").arg(i) + "))
                            }
                        }", row)
                }
                else
                {
                    console.log("Unknown parameter type ", type)
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
