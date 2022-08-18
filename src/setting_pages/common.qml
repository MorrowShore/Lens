import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import AxelChat.QMLUtils 1.0
import AxelChat.I18n 1.0
import "../my_components" as MyComponents
import "../"

ScrollView {
    id: root
    clip: true
    padding: 6
    contentHeight: rootColumn.implicitHeight
    contentWidth: rootColumn.implicitWidth

    Column {
        id: rootColumn

        Row {
            spacing: 6

            Label {
                text: qsTr("Language")
                anchors.verticalCenter: parent.verticalCenter
            }

            ComboBox {
                id: comboBoxLanguage
                anchors.verticalCenter: parent.verticalCenter
                width: 210

                model: ListModel {
                    ListElement { text: "English"; }
                    ListElement { text: "Русский"; }
                }

                property bool enableForEditing: false
                Component.onCompleted: {
                    if (i18n.language == "ru")
                        currentIndex = 1;
                    else
                        currentIndex = 0;
                    enableForEditing = true;
                }

                onCurrentIndexChanged: {
                    if (!enableForEditing)
                    {
                        return;
                    }

                    if (currentIndex == 0)
                        i18n.setLanguage("C");
                    if (currentIndex == 1)
                        i18n.setLanguage("ru");
                }
            }

            Image {
                width: 40
                height: 40
                anchors.verticalCenter: parent.verticalCenter
                mipmap: true
                fillMode: Image.PreserveAspectFit
                source: {
                    if (comboBoxLanguage.currentIndex === 0)
                        return "qrc:/resources/images/flags/usa.svg";
                    if (comboBoxLanguage.currentIndex === 1)
                        return "qrc:/resources/images/flags/russia.svg";
                }
            }
        }

        Switch {
            text: qsTr("Stay on Top Window")

            Component.onCompleted: {
                checked = Global.windowChatStayOnTop
            }

            onCheckedChanged: {
                Global.windowChatStayOnTop = checked
                Global.save()
            }
        }

        Row {
            Switch {
                text: qsTr("Click-through window")

                Component.onCompleted: {
                    checked = Global.windowChatTransparentForInput
                }
                onCheckedChanged: {
                    Global.windowChatTransparentForInput = checked
                    Global.save()
                }
            }

            RoundButton {
                anchors.verticalCenter: parent.verticalCenter
                flat: true
                icon.source: "qrc:/resources/images/help-round-button.svg"
                onClicked: {
                    Global.windowSettings.showInfo(qsTr("Mouse clicks will be ignored by the chat window. You can return to the previous state through the tray icon"))
                }
            }
        }

        Switch {
            text: qsTr("Clear Messages on Link Change")

            Component.onCompleted: {
                checked = chatHandler.enabledClearMessagesOnLinkChange;
            }

            onCheckedChanged: {
                chatHandler.enabledClearMessagesOnLinkChange = checked;
            }
        }

        Switch {
            text: qsTr("Enabled Hardware Graphics Accelerator")

            Component.onCompleted: {
                checked = qmlUtils.enabledHardwareGraphicsAccelerator;
            }

            onCheckedChanged: {
                qmlUtils.enabledHardwareGraphicsAccelerator = checked;
            }

            onClicked: {
                Global.windowSettings.showRestartDialog()
            }
        }

        Switch {
            text: qsTr("Enabled HighDpi scaling")

            Component.onCompleted: {
                checked = qmlUtils.enabledHighDpiScaling;
            }

            onCheckedChanged: {
                qmlUtils.enabledHighDpiScaling = checked;
            }

            onClicked: {
                Global.windowSettings.showRestartDialog()
            }
        }

        Switch {
            text: qsTr("Enable Sound when New Message Received")

            Component.onCompleted: {
                checked = chatHandler.enabledSoundNewMessage;
            }

            onCheckedChanged: {
                chatHandler.enabledSoundNewMessage = checked;
            }

            onClicked: {
                if (checked)
                {
                    chatHandler.playNewMessageSound();
                }
            }
        }

        Row {
            Switch {
                text: qsTr("Show when author changes name")
                enabled: outputToFile.enabled

                Component.onCompleted: {
                    checked = chatHandler.enabledShowAuthorNameChanged;
                }

                onCheckedChanged: {
                    chatHandler.enabledShowAuthorNameChanged = checked;
                }
            }

            RoundButton {
                anchors.verticalCenter: parent.verticalCenter
                flat: true
                icon.source: "qrc:/resources/images/help-round-button.svg"
                onClicked: {
                    Global.windowSettings.showInfo(qsTr("Works only when output to file is enabled"))
                }
            }
        }

        Row {
            spacing: 6

            Switch {
                text: qsTr("Proxy (SOCKS5)")
                anchors.verticalCenter: parent.verticalCenter
                Component.onCompleted: {
                    checked = chatHandler.proxyEnabled;
                }

                onCheckedChanged: {
                    chatHandler.proxyEnabled = checked;
                }
            }

            MyComponents.MyTextField {
                maximumLength: 15
                width: 120
                anchors.verticalCenter: parent.verticalCenter
                placeholderText: qsTr("Host name...")

                Component.onCompleted: {
                    text = chatHandler.proxyServerAddress
                }

                onTextChanged: {
                    chatHandler.proxyServerAddress = text
                }
            }

            Text {
                text: ":";
                anchors.verticalCenter: parent.verticalCenter
                color: Material.foreground
            }

            MyComponents.MyTextField {
                maximumLength: 5
                width: 60
                anchors.verticalCenter: parent.verticalCenter
                placeholderText: qsTr("Port...")
                validator: RegExpValidator {
                    regExp:  /^[0-9]{1,5}$/
                }

                Component.onCompleted: {
                    var port = chatHandler.proxyServerPort;
                    if (port !== -1)
                    {
                        text = port;
                    }
                    else
                    {
                        text = "";
                    }
                }

                onTextChanged: {
                    if (text == "")
                    {
                        chatHandler.proxyServerPort = -1;
                    }
                    else
                    {
                        chatHandler.proxyServerPort = text;
                    }
                }
            }
        }

        Button {
            text: qsTr("Program folder")

            onClicked: {
                chatHandler.openProgramFolder();
            }
        }
    }
}



/*##^##
Designer {
    D{i:0;formeditorZoom:0.9}
}
##^##*/
