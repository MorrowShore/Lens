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

        Row {
            spacing: 10

            Label {
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 20
                font.bold: true
                color: Material.accentColor

                text: qsTr("Chat") + ":"
            }

            MyComponents.MyTextField {
                anchors.verticalCenter: parent.verticalCenter
                width: 400
                readOnly: true
                text: "http://127.0.0.1:3000"
            }
        }

        Row {
            spacing: 10

            Label {
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 20
                font.bold: true
                color: Material.accentColor

                text: qsTr("Viewers") + ":"
            }

            MyComponents.MyTextField {
                anchors.verticalCenter: parent.verticalCenter
                width: 400
                readOnly: true
                text: "http://127.0.0.1:3000/viewers"
            }
        }
    }
}
