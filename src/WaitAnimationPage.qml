import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.0
import Qt.labs.settings 1.1
import AxelChat.ChatManager 1.0
import AxelChat.ChatService 1.0
import AxelChat.UpdateChecker 1.0
import QtQuick.Window 2.15
import "my_components" as MyComponents
import "setting_pages" as SettingPages
import "."

Item {
    Item {
        id: waitAnimation
        x: parent.width  / 2 - width  / 2
        y: parent.height / 2 - height / 2
        width: 100
        height: 100

        AnimatedImage {
            id: animatedImage
            x: 270
            y: 190
            anchors.fill: parent
            playing: true
            source: chatManager.connectedCount === 0 ? "qrc:/resources/images/sleeping_200_transparent.gif" : "qrc:/resources/images/cool_200_transparent.gif"
            smooth: true
            antialiasing: true
            asynchronous: true

            property real imageScaleX: 1.0
            transform: Scale {
                xScale: animatedImage.imageScaleX;
                origin.x: width  / 2;
                origin.y: height / 2;
            }
        }

        ColorOverlay {
            width: 100
            height: 100
            source: animatedImage
            smooth: animatedImage.smooth
            antialiasing: animatedImage.antialiasing
            color: "#03A9F4"
            opacity: animatedImage.opacity
            transform: Scale {
                xScale: animatedImage.imageScaleX
                origin.x: width  / 2;
                origin.y: height / 2;
            }
        }
    }

    Text {
        text: {
            var s = ""

            if (chatManager.connectedCount === 0)
            {
                s += qsTr("Nothing connected");

                if (typeof(root.settingsWindow) == "undefined" || !root.settingsWindow.visible)
                {
                   s += "\n\n" + qsTr("Right-click and select \"Connections\"");
                }
            }
            else
            {
                s += qsTr("Connected!") + "\n\n" + qsTr("But no one has written anything yet")
            }

            return s;
        }

        color: "#03A9F4"
        wrapMode: Text.Wrap
        style: Text.Outline
        font.pointSize: 12
        horizontalAlignment: Text.AlignHCenter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: waitAnimation.bottom
        anchors.topMargin: 20
    }
}
