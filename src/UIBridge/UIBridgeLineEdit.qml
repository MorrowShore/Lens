import QtQuick 2.0
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.12
import AxelChat.ChatService 1.0
import '.' as MyComponents

Row {
    id: row
    spacing: 6

    property string name: "LineEdit"
    property string placeholderText: ""
    property string text: ""
    property bool passwordEcho: false

    Label {
        text: parent.name + ":"
        font.pixelSize: 18
        anchors.verticalCenter: parent.verticalCenter
        font.bold: true
        color: Material.accentColor
    }

    MyComponents.MyTextField {
        id: textField

        property bool enableSignal: true

        width: 400
        anchors.verticalCenter: parent.verticalCenter
        placeholderText: parent.placeholderText
        echoMode: parent.passwordEcho ? TextInput.Password : TextInput.Normal

        Component.onCompleted: {
            text = parent.text
        }

        onTextChanged: {
            if (enableSignal) {
                parent.text = text
            }
        }

        Connections {
            target: row
            function onTextChanged() {
                textField.enableSignal = false
                textField.text = row.text
                textField.enableSignal = true
            }
        }
    }

    Button {
        //highlighted: " + (i == 0 ? "true" : "false") + "
        anchors.verticalCenter: parent.verticalCenter
        display: AbstractButton.TextBesideIcon
        icon.source: 'qrc:/resources/images/clipboard-paste-button.svg'
        text: qsTr("Paste")
        onClicked: {
            if (clipboard.text.length !== 0)
            {
                textField.text = clipboard.text;
                textField.deselect();
            }
        }
    }
}
