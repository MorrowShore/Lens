import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.12
import "my_components" as MyComponents
import AxelChat.AuthorQMLProvider 1.0

Window {
    id: root
    title: qsTr("Participant Information")

    Material.theme: Material.Dark
    Material.accent :     "#03A9F4"
    Material.background : "black"
    Material.foreground : "#03A9F4"
    Material.primary :    "#03A9F4"

    color: Material.background

    flags: Qt.Dialog |
           Qt.CustomizeWindowHint |
           Qt.WindowTitleHint |
           Qt.WindowCloseButtonHint

    property string authorId: ""

    onAuthorIdChanged: {
        authorQMLProvider.setSelectedAuthorId(authorId)
    }

    width: 480
    height: 256

    ScrollView {
        id: scrollView
        anchors.fill: parent

        Row {
            x: 6
            y: 6
            spacing: 6
            width: root.width - 12

            Column {
                id: leftColumnt
                spacing: 6

                MyComponents.ImageRounded {
                    id: avatarImage
                    rounded: false
                    height: 240
                    width:  height
                    mipmap: true
                    source: {
                        if (authorQMLProvider.serviceType === Global._YouTubeServiceType) {
                            return typeof(authorQMLProvider.avatarUrl) == "object" ? youTube.createResizedAvatarUrl(authorQMLProvider.avatarUrl, height) : ""
                        }

                        return authorQMLProvider.avatarUrl
                    }
                }
            }

            Column {
                spacing: 4
                width: parent.width - leftColumnt.width

                MyComponents.MyTextField {
                    id: labelAuthorName
                    width: parent.width
                    text: authorQMLProvider.name
                    color: "#03A9F4";
                    font.bold: true
                    selectByMouse: true
                    readOnly: true
                    wrapMode: Text.Wrap
                }

                /*Label {
                    id: labelAuthorType
                    text: {
                        var typeName = "123";
                        return typeName;
                    }

                }*/

                Label {
                    id: labelMessagesSent
                    text: qsTr("Messages: %1")
                        .arg(authorQMLProvider.messagesCount)
                }

                Button {
                    text: qsTr("Folder")
                    icon.source: "qrc:/resources/images/forward-arrow.svg"
                    highlighted: false
                    flat: true

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                    }

                    onClicked: {
                        if (!authorQMLProvider.openFolder()) {
                            dialog.title = qsTr("The folder does not exist or an error occurred")
                            dialog.open()
                        }
                    }
                }

                Button {
                    text: qsTr("Avatar")
                    icon.source: "qrc:/resources/images/forward-arrow.svg"
                    highlighted: false
                    flat: true

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                    }

                    onClicked: {
                        if (!authorQMLProvider.openAvatar()) {
                            dialog.title = qsTr("Unknown error")
                            dialog.open()
                        }
                    }
                }

                Button {
                    text: qsTr("Page")

                    icon.source: "qrc:/resources/images/forward-arrow.svg"
                    highlighted: true
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                    }

                    onClicked: {
                        if (!authorQMLProvider.openPage()) {
                            dialog.title = qsTr("Unknown error")
                            dialog.open()
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: dialog
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok
    }
}

/*##^##
Designer {
    D{i:8}D{i:9}
}
##^##*/
