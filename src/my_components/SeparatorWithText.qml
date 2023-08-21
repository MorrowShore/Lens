import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15

Rectangle {
    id: root

    color: "transparent"

    property string text: "separator"

    width: row.width
    height: row.height

    Row {
        id: row

        spacing: 10

        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            height: 2
            width: 250
            color: "#626466"

            anchors.verticalCenter: parent.verticalCenter
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            opacity: 0.75

            text: root.text
        }

        Rectangle {
            height: 2
            width: 250
            color: "#626466"

            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
