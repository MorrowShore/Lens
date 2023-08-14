import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.0
import Qt.labs.settings 1.1
import AxelChat.ChatHandler 1.0
import AxelChat.ChatService 1.0
import AxelChat.UpdateChecker 1.0
import "my_components" as MyComponents
import "."


Item {
    property int count: listView.count
    property int duration: 5000

    function start() {
        timerUpdateProgressBar.start()
    }

    signal stopped()

    Timer {
        id: timerUpdateProgressBar
        interval: 10
        running: false
        repeat: true
        onTriggered: {
            progressBar.value -= interval / duration

            if (progressBar.value <= 0) {
                stop()
                stopped()
            }
        }
    }

    Column {
        anchors.fill: parent
        spacing: 8

        ProgressBar {
            id: progressBar
            anchors.left: parent.left
            anchors.right: parent.right
            height: 4
            value: 1

            background: Rectangle {
                anchors.fill: progressBar
                color: "#8003A9F4"
            }

            contentItem: Rectangle {
                anchors.left: progressBar.left
                anchors.bottom: progressBar.bottom
                height: progressBar.height
                width: progressBar.width * progressBar.value
                color: "#03A9F4"
            }
        }

        Text {
            id: titleText
            style: Text.Outline
            color: "#03A9F4"
            font.bold: true
            font.pointSize: Global.windowChatMessageTextFontSize
            wrapMode: Text.Wrap
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            horizontalAlignment: Text.AlignHCenter

            text: qsTr("Thanks to these people, %1 gets support:").arg(Qt.application.name)
        }

        ListView {
            id: listView
            height: parent.parent.height

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            anchors.topMargin: 4
            interactive: true
            delegate: delegate

            model: appSponsorsModel
        }
    }

    Component {
        id: delegate

        Rectangle {
            id: content
            width: listView.width
            height: textLine.height + 10

            color: "transparent"

            Text {
                id: textLine
                style: Text.Outline
                color: "#03A9F4"
                font.pointSize: Global.windowChatMessageTextFontSize
                text: "<b>" + name + "</b> - " + tierName
                anchors.left: parent.left
                anchors.right: parent.right

                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
