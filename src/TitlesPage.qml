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
    Text {
        style: Text.Outline
        color: "#03A9F4"
        font.bold: true
        font.pointSize: Global.windowChatMessageTextFontSize
        wrapMode: Text.Wrap
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 4
        horizontalAlignment: Text.AlignHCenter

        text: qsTr("Thanks to these people, %1 gets support:").arg(Qt.application.name)
    }
}
