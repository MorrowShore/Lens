import QtQuick 2.0
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.12
import "../my_components" as MyComponents

Window {
    id: root
    width: 610
    height: 407
    minimumHeight: 200
    minimumWidth:  200

    Material.theme: Material.Dark
    Material.accent: "#03A9F4"
    Material.foreground: "#FFFFFF"
    color: "#202225"

    onVisibleChanged: {
        if (visible) {
            qmlUtils.updateWindowStyle(this)
        }
    }

    ScrollView {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: buttonCopy.top
        anchors.leftMargin: 0
        anchors.rightMargin: 0
        anchors.bottomMargin: 0
        anchors.topMargin: 0
        ScrollBar.horizontal.policy: width < contentWidth ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: height < contentHeight ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

        TextArea {
            id: textInputCommands

            text: ""
            anchors.left: root.left
            anchors.right: root.right
            anchors.top: root.top
            //anchors.bottom: button.top
            font.pixelSize: 14
            anchors.bottomMargin: 6
            anchors.rightMargin: 14
            anchors.topMargin: 13
            anchors.leftMargin: 7
            readOnly: true
            background.visible: false
            wrapMode: Text.WordWrap
        }
    }

    Component.onCompleted: {
        textInputCommands.text = chatBot.commandsText();
    }

    Button {
        id: buttonCopy
        x: 526
        y: 340
        text: qsTr("Copy")
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.rightMargin: 8
        highlighted: true

        onClicked: {
            clipboard.text = textInputCommands.text;
        }
    }
}

/*##^##
Designer {
    D{i:0;formeditorZoom:1.100000023841858}D{i:1}
}
##^##*/
