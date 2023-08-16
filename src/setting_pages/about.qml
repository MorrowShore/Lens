import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import AxelChat.UpdateChecker 1.0
import AxelChat.QMLUtils 1.0
import "../"

ScrollView {
    id: root
    clip: true
    contentHeight: 540
    contentWidth: 640
    ScrollBar.horizontal.policy: width < contentWidth ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: height < contentHeight ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

    Item {
        id: element5
        width:  Math.max(root.width,  root.contentWidth)
        height: Math.max(root.height, root.contentHeight)

        Dialog {
            id: noNewVersionsDialog
            anchors.centerIn: parent
            modal: true
            standardButtons: Dialog.Ok
        }

        Text {
            id: textVersion
            y: 76
            text: Qt.application.version
            anchors.left: textApplicationName.right
            anchors.leftMargin: 6
            font.pixelSize: 20
            color: Material.foreground
        }

        Text {
            id: textApplicationName
            x: 256
            y: 84
            height: 29
            text: Qt.application.name
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: 24
            color: Material.foreground
        }

        Text {
            id: textBuildInfo
            x: 309
            y: 119
            text:  {
                if (qmlUtils.buildCpuArchitecture === "i386")
                    return qsTr("32-bit Build");
                else if (qmlUtils.buildCpuArchitecture === "x86_64")
                    return qsTr("64-bit Build");
                else
                    return qsTr("Build Architecture: %1").arg(qmlUtils.buildCpuArchitecture);
            }
            font.pixelSize: 12
            anchors.horizontalCenter: textApplicationName.horizontalCenter
            color: Material.foreground
        }

        Image {
            id: image
            x: 275
            y: 147
            width: 100
            height: 100
            anchors.horizontalCenter: parent.horizontalCenter
            mipmap: true
            source: "qrc:/resources/images/axelchat.svg"
            fillMode: Image.PreserveAspectFit
        }

        Button {
            id: buttonCheckUpdates
            x: 460
            y: 8
            text: qsTr("Check for Updates")
            anchors.right: parent.right
            highlighted: true
            anchors.rightMargin: 8
            icon.source: "qrc:/resources/images/refresh.svg"

            onClicked: {
                updateChecker.checkForNewVersion();
            }
        }

        Text {
            id: element3
            x: 380
            y: 20
            text: "(づ ◕‿◕ )づ"
            anchors.right: buttonCheckUpdates.left
            anchors.rightMargin: 6
            font.pixelSize: 20
            color: Material.foreground
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter

            y: 450
            spacing: 10

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10

                Column {
                    Text {
                        id: element4

                        text: qsTr("subscribe")
                        font.pixelSize: 12
                        color: Material.foreground
                    }

                    Text {
                        id: element1
                        text: "(⌒ω⌒)ﾉ"
                        font.pixelSize: 20
                        color: Material.foreground
                    }
                }

                Button {
                    text: qsTr("YouTube")
                    icon.color: "transparent"
                    icon.source: "qrc:/resources/images/youtube-icon.svg"

                    onClicked: {
                        Qt.openUrlExternally("https://www.youtube.com/channel/UCujkj8ZgVkscm34GA1Z8wTQ");
                    }
                }

                Button {
                    text: qsTr("Twitch")
                    icon.color: "transparent"
                    icon.source: "qrc:/resources/images/twitch-icon.svg"

                    onClicked: {
                        Qt.openUrlExternally("https://www.twitch.tv/axe1_k");
                    }
                }

                Button {
                    text: qsTr("GitHub")
                    icon.source: "qrc:/resources/images/github.svg"

                    onClicked: {
                        Qt.openUrlExternally("https://github.com/3dproger");
                    }
                }
            }

            Text {
                textFormat: Text.RichText
                text: "Some icons made by <a href=\"http://www.freepik.com/\" title=\"Freepik\">Freepik</a> and <a href=\"https://www.flaticon.com/authors/google\" title=\"Google\">Google</a> from <a href=\"https://www.flaticon.com/\" title=\"Flaticon\"> www.flaticon.com</a> and <a href=\"https://icons8.com\">https://icons8.com</a>"
                font.pixelSize: 12

                onLinkActivated: {
                    Qt.openUrlExternally(link)
                }

                color: Material.foreground
            }
        }

        Button {
            id: buttonReleases
            x: 500
            y: 62
            text: qsTr("Releases")
            anchors.right: parent.right
            anchors.rightMargin: 8
            icon.source: "qrc:/resources/images/new-release.svg"

            onClicked: {
                Qt.openUrlExternally("https://github.com/3dproger/AxelChat/releases");
            }
        }

        Text {
            id: element6
            y: 290
            height: 66
            text: qsTr("%1 is a free and cross-platform app for adding interactivity to live-streaming and interacting with viewers simultaneously for multiple live-streaming platforms").arg(Qt.application.name)
                + "<br><br>" + qsTr("For feedback or cooperation, write to email %1").arg("<u><b>" + APP_INFO_EMAIL_STR + "</b></u>")
                + "<br><br>" + APP_INFO_LEGALCOPYRIGHT_STR_U
            anchors.left: parent.left
            anchors.right: parent.right
            font.pixelSize: 17
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            anchors.rightMargin: 8
            anchors.leftMargin: 8
            color: Material.foreground
        }

        Button {
            id: buttonSite
            x: 8
            y: 8
            text: Qt.application.name
            icon.source: "qrc:/resources/images/internet-globe.svg"
            highlighted: true

            onClicked: {
                Qt.openUrlExternally("https://3dproger.github.io/AxelChat/");
            }
        }

        Button {
            id: sponsorButton
            x: 8
            y: 62
            text: qsTr("Support the developer")
            icon.source: "qrc:/resources/images/heart.svg"
            Material.background: "#DB61A2"

            onClicked: {
                Qt.openUrlExternally(APP_INFO_SUPPORT_URL_STR)
            }
        }

        Button {
            id: buttonIssues
            x: 8
            y: 116
            text: qsTr("Complaints and suggestions (Issues)")
            icon.source: "qrc:/resources/images/feedback.svg"

            onClicked: {
                Qt.openUrlExternally("https://github.com/3dproger/AxelChat/issues");
            }
        }

        Button {
            id: buttonSource
            x: 10
            y: 170
            text: qsTr("Source Code")
            icon.source: "qrc:/resources/images/github.svg"

            onClicked: {
                Qt.openUrlExternally("https://github.com/3dproger/AxelChat");
            }
        }

        Button {
            id: buttonLicense
            x: 10
            y: 224
            text: qsTr("License")

            onClicked: {
                Qt.openUrlExternally("https://raw.githubusercontent.com/3dproger/AxelChat/main/LICENSE");
            }
        }
    }

    Component.onCompleted: {
        var component = Qt.createComponent("qrc:/updatesnotification.qml");
        root.updatesWindow = component.createObject(root);
    }

    property var updatesWindow;

    Connections {
        target: updateChecker

        property bool showUpdateWindowIfNotNewVersionAvailable: false;

        function onReplied() {
            if (updateChecker.replyState === UpdateChecker.NewVersionAvailable
                    || updateChecker.replyState === UpdateChecker.NewVersionAvailableButSkipped)
            {
                root.updatesWindow.showSkipVersionCheckBox = false;
                root.updatesWindow.show();
            }
            else if (updateChecker.replyState === UpdateChecker.LatestVersionYouHave)
            {
                noNewVersionsDialog.title = qsTr("You are using the most current version!\n\nCurrent version: %1\nLatest version available: %2")
                    .arg(Qt.application.version)
                    .arg(updateChecker.lastVersionName);
                noNewVersionsDialog.open();

            }
            else if (updateChecker.replyState === UpdateChecker.FailedToConnect)
            {
                noNewVersionsDialog.title = qsTr("Failed to connect to server. Check your internet connection!");
                noNewVersionsDialog.open();
            }
            else if (updateChecker.replyState === UpdateChecker.RequestLimitsExceeded)
            {
                noNewVersionsDialog.title = qsTr("Request limits exceeded! Please try again later");
                noNewVersionsDialog.open();
            }
        }
    }
}

/*##^##
Designer {
    D{i:0;autoSize:true;formeditorZoom:1.100000023841858;height:480;width:640}D{i:2}D{i:3}
D{i:4}D{i:5}D{i:6}D{i:7}D{i:8}D{i:9}D{i:10}D{i:11}D{i:12}D{i:13}D{i:14}D{i:15}D{i:16}
D{i:17}D{i:18}D{i:19}D{i:20}D{i:1}D{i:21}
}
##^##*/
