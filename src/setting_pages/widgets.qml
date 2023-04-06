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
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded
    ScrollBar.vertical.policy: ScrollBar.AlwaysOn

    Column {
        id: rootColumn

        Label {
            font.pixelSize: 20
            color: "red"

            text: qsTr("Part of the functionality is not yet implemented or may work with errors")
        }

        Rectangle {
            width: 10
            height: 30
            color: "transparent"
        }

        Label {
            font.pixelSize: 20

            text: qsTr("Copy the widget link and paste it into your streaming software")
        }

        Rectangle {
            width: 10
            height: 30
            color: "transparent"
        }

        Label {
            font.pixelSize: 20
            font.bold: true
            color: Material.accentColor

            text: qsTr("Messages") + ":"
        }

        Row {
            spacing: 10

            MyComponents.MyTextField {
                id: chatTextField
                anchors.verticalCenter: parent.verticalCenter
                width: 400
                readOnly: true
                text: "file:///" + applicationDirPath + "/widgets/index.html?widget=messages"
            }

            Button {
                anchors.verticalCenter: parent.verticalCenter
                display: AbstractButton.TextBesideIcon
                icon.source: 'qrc:/resources/images/copy-content.svg'
                text: qsTr("Copy")
                onClicked: {
                    chatTextField.selectAll()
                    clipboard.text = chatTextField.text
                }
            }
        }

        Rectangle {
            width: 10
            height: 30
            color: "transparent"
        }

        Label {
            font.pixelSize: 20
            font.bold: true
            color: Material.accentColor

            text: qsTr("Viewers counter") + ":"
        }

        Row {
            spacing: 10

            MyComponents.MyTextField {
                id: statesTextField
                anchors.verticalCenter: parent.verticalCenter
                width: 400
                readOnly: true
                text: "file:///" + applicationDirPath + "/widgets/index.html?widget=states"
            }

            Button {
                anchors.verticalCenter: parent.verticalCenter
                display: AbstractButton.TextBesideIcon
                icon.source: 'qrc:/resources/images/copy-content.svg'
                text: qsTr("Copy")
                onClicked: {
                    statesTextField.selectAll()
                    clipboard.text = statesTextField.text
                }
            }
        }
    }
}
