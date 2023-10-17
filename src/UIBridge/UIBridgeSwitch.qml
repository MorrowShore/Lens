import QtQuick 2.0
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.12
import "../"

Row {
    id: root

    property string text: "Switch"
    property string hint: ""
    property bool checked: switch1.checked

    spacing: 6

    Switch {
        id: switch1
        text: parent.text
    }

    RoundButton {
        anchors.verticalCenter: parent.verticalCenter
        flat: true
        visible: parent.hint.length > 0
        icon.source: "qrc:/resources/images/help-round-button.svg"
        onClicked: {
            Global.windowSettings.showInfo(parent.hint)
        }
    }
}


