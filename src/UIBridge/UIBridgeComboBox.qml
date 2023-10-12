import QtQuick 2.0
import QtQuick.Controls 2.15

Row {
    id: root

    property string name: "combobox"
    property var model: null
    property int currentIndex: -1

    spacing: 6

    Label {
        anchors.verticalCenter: parent.verticalCenter
        text: parent.name
    }

    ComboBox {
        id: comboBox
        anchors.verticalCenter: parent.verticalCenter
        model: parent.model

        property bool enableSignal: true

        Component.onCompleted: {
            currentIndex = parent.currentIndex
            console.log(parent.currentIndex)
        }

        onCurrentIndexChanged: {
            if (enableSignal) {
                parent.currentIndex = currentIndex
            }
        }

        Connections {
            target: root
            function onCurrentIndexChanged() {
                comboBox.enableSignal = false
                comboBox.currentIndex = root.currentIndex
                comboBox.enableSignal = true
            }
        }
    }
}
