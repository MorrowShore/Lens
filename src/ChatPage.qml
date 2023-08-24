import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.0
import Qt.labs.settings 1.1
import AxelChat.ChatHandler 1.0
import AxelChat.ChatService 1.0
import AxelChat.UpdateChecker 1.0
import QtQuick.Window 2.15
import "my_components" as MyComponents
import "setting_pages" as SettingPages
import "."

Item {
    property var listView: listMessages

    ListView {
        id: listMessages

        anchors.fill: parent
        interactive: false

        property bool needAutoScrollToBottom: false

        Button {
            id: roundButtonScrollToBottom
            anchors.horizontalCenter: listMessages.horizontalCenter
            y: 420
            text: "↓"
            width: 38
            height: width
            opacity: down ? 0.8 : 1.0

            background: Rectangle {
                anchors.fill: parent
                color: "#03A9F4"
                radius: Math.min(width, height) / 2
            }

            MouseArea {
                function containsMouseRound() {
                    var x1 = width / 2;
                    var y1 = height / 2;
                    var x2 = mouseX;
                    var y2 = mouseY;
                    var distanceFromCenter = Math.pow(x1 - x2, 2) + Math.pow(y1 - y2, 2);
                    var radiusSquared = Math.pow(Math.min(width, height) / 2, 2);
                    var isWithinOurRadius = distanceFromCenter < radiusSquared;
                    return isWithinOurRadius;
                }

                anchors.fill: parent
                hoverEnabled: true
                propagateComposedEvents: true
                cursorShape: {
                    if (containsMouseRound())
                    {
                        return Qt.PointingHandCursor;
                    }
                }
                onClicked: {
                    Qt.callLater(listMessages.positionViewAtEnd);
                    state = "hiden";
                }
            }

            state: "hiden"

            states: [
                State {
                    name: "hiden"
                    PropertyChanges {
                        y: listMessages.height + 8 + (bottomPanel.visible ? bottomPanel.height : 0)
                        target: roundButtonScrollToBottom;
                    }
                },
                State {
                    name: "shown"
                    PropertyChanges {
                        y: listMessages.height - width - 20;
                        target: roundButtonScrollToBottom;
                    }
                }
            ]

            transitions: Transition {
                NumberAnimation {
                    properties: "y";
                    easing.type: Easing.InOutQuad;
                    duration: 400;
                }
            }
        }

        ScrollBar.vertical: ScrollBar {
            contentItem: Rectangle {
                color: "#03A9F4"
                opacity: 0
                onOpacityChanged: {
                    if (opacity > 0.5)
                    {
                        opacity = 0.5
                    }
                }
            }
        }

        spacing: 2
        width: parent.width
        model: messagesModel
        delegate: messageDelegate

        visibleArea.onHeightRatioChanged: {
            if (scrollbarOnBottom())
            {
                roundButtonScrollToBottom.state = "hiden"
            }
            else
            {
                roundButtonScrollToBottom.state = "shown"
            }
        }

        visibleArea.onYPositionChanged: {
            if (scrollbarOnBottom())
            {
                roundButtonScrollToBottom.state = "hiden"
            }
            else
            {
                roundButtonScrollToBottom.state = "shown"
            }
        }

        function autoScroll()
        {
            if (needAutoScrollToBottom)
            {
                listMessages.positionViewAtEnd();
                listMessages.needAutoScrollToBottom = false;
            }
        }

        Connections {
            target: chatHandler
            function onMessagesDataChanged() {
                listMessages.needAutoScrollToBottom = listMessages.scrollbarOnBottom();
                Qt.callLater(listMessages.autoScroll);
                deferredAutoScrollTimer.running = true
                deferredAutoScrollTimer.couterTimes = 5
            }
        }

        Timer {
            id: deferredAutoScrollTimer
            property int couterTimes: 5
            interval: 50
            running: false
            repeat: true
            onTriggered: {
                // additional scroll to fix the problem when the first time the scroll does not reach the end
                listMessages.needAutoScrollToBottom = listMessages.scrollbarOnBottom();
                listMessages.autoScroll()
                couterTimes -= 1
                if (couterTimes <= 0)
                {
                    running = false
                }
            }
        }

        function scrollbarOnBottom()
        {
            return visibleArea.yPosition * contentHeight + listMessages.height + 160 >= contentHeight;
        }
    }
    function openAuthorWindow(authorId) {
        if (root.authorWindow === null) {
            root.authorWindow = Qt.createComponent("qrc:/author_window.qml").createObject(root)
        }

        root.authorWindow.authorId = authorId
        root.authorWindow.show()
    }

    function redrawTextEdit(_textEditObject)
    {
        // this non-obvious code solves the bug https://github.com/3dproger/AxelChat/issues/193

        var selectionStart  =_textEditObject.selectionStart
        var selectionEnd =_textEditObject.selectionEnd

        _textEditObject.selectAll()
        _textEditObject.deselect()

        _textEditObject.select(selectionStart, selectionEnd)
    }

    Component {
        id: messageDelegate

        Rectangle {
            id: messageContent

            width: listMessages.width
            height: {
                if (avatarImage.visible) {
                    return Math.max(Math.max(textEditMessageText.y + textEditMessageText.height + 4, 40), avatarImage.height + 4)
                }
                else {
                    return Math.max(textEditMessageText.y + textEditMessageText.height + 4, 40)
                }
            }

            state: "hiden"

            Component.onCompleted: {
                state = "shown"
            }

            states: [
                State {
                    name: "hiden"
                    PropertyChanges {
                        x: listMessages.width;
                        target: messageContent;
                    }
                },
                State {
                    name: "shown"
                    PropertyChanges {
                        x: 0;
                        target: messageContent;
                    }
                }
            ]

            transitions: Transition {
                NumberAnimation {
                    properties: "x";
                    easing.type: Easing.InOutQuad;
                    duration: 500;
                }
            }

            border.width: Global.windowChatMessageFrameBorderWidth
            border.color: Global.windowChatMessageFrameBorderColor
            color: {
                if (messageBodyBackgroundForcedColor !== "")
                {
                    return messageBodyBackgroundForcedColor
                }

                if (messageIsDonateSimple || messageIsDonateWithText || messageIsDonateWithImage)
                {
                    return "#1DE9B6"
                }

                if (messageIsYouTubeChatMembership)
                {
                    return "#0F9D58"
                }

                return Global.windowChatMessageFrameBackgroundColor
            }

            radius: Global.windowChatMessageFrameCornerRadius

            Rectangle {
                id: authorBackground
                x: authorRow.x - 6
                y: authorRow.y - 1
                width: authorNameText.width + 12
                height: authorNameText.height + 2
                radius: 4
                color: authorHasCustomNicknameBackgroundColor ? authorCustomNicknameBackgroundColor : "transparent"
                visible: authorRow.visible
            }

            Row {
                id: authorRow
                y: 3
                anchors.left:  avatarImage.visible ? avatarImage.right : parent.left
                anchors.right: labelTime.visible ? labelTime.left : messageContent.right
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                anchors.leftMargin: 4 + authorBackground.visible * 6
                anchors.rightMargin: 4
                spacing: 4
                visible: Global.windowChatMessageShowAuthorName && !messageMarkedAsDeleted

                //Author Name
                Text {
                    id: authorNameText
                    style: Text.Outline
                    color: authorHasCustomNicknameColor ? authorCustomNicknameColor : "#03A9F4"
                    font.bold: true
                    font.pointSize: Global.windowChatMessageAuthorNameFontSize

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: authorServiceType !== Global._SoftwareServiceType && !messageIsServiceMessage;
                        cursorShape: {
                            if (hoverEnabled)
                                return Qt.PointingHandCursor;
                        }
                        onClicked: {
                            if (hoverEnabled) {
                                openAuthorWindow(authorId);
                            }
                        }
                    }

                    textFormat: Text.RichText

                    property int badgePixelSize: font.pixelSize * 1.25
                    text: {
                        var prefix = "";
                        var postfix = "";

                        if (Global.windowChatMessageShowPlatformIcon && authorServiceType !== Global._SoftwareServiceType)
                        {
                            prefix += createImgHtmlTag(chatHandler.getServiceIconUrl(authorServiceType), badgePixelSize) + " "
                        }

                        var i;

                        for (i = 0; i < authorLeftBadgesUrls.length; i++)
                        {
                            prefix += createImgHtmlTag(authorLeftBadgesUrls[i], badgePixelSize) + " "
                        }

                        for (i = 0; i < authorRightBadgesUrls.length; i++)
                        {
                            postfix += createImgHtmlTag(authorRightBadgesUrls[i], badgePixelSize) + " "
                        }

                        if (postfix.length > 0)
                        {
                            postfix = " " + postfix
                        }

                        var name = ""

                        if (name.length === 0) {
                            name = messageCustomAuthorName
                        }

                        if (name.length === 0) {
                            name = authorName
                        }

                        if (name.length === 0 && messageIsServiceMessage) {
                            name = chatHandler.getServiceName(authorServiceType)
                        }

                        return prefix + name + postfix
                    }
                }

                //Message destination
                Text {
                    id: destination
                    style: Text.Outline
                    anchors.verticalCenter: authorNameText.verticalCenter
                    text: {
                        var t = " → "
                        for (let i = 0; i < messageDestination.length; i++) {
                            var dest = messageDestination[i]
                            t += dest

                            if (i < messageDestination.length - 1) {
                                t += " / "
                            }
                        }

                        return t
                    }
                    color: "#ededed"
                    font.pointSize: authorNameText.font.pointSize * 0.8
                    visible: messageDestination.length > 0
                }
            }

            //Time
            Label {
                id: labelTime
                visible: Global.windowChatMessageShowTime
                color: "#039BE5"
                anchors.right: messageContent.right
                anchors.margins: 4
                text: messagePublishedAt.toLocaleTimeString(Qt.locale(), "hh:mm")
                font.pixelSize: Global.windowChatMessageTimeFontSize
                style: Text.Outline
            }

            //Author Avatar
            MyComponents.ImageRounded {
                id: avatarImage
                visible: Global.windowChatMessageShowAvatar && !messageMarkedAsDeleted
                //cache: false // TODO: need check
                fillMode: Image.PreserveAspectCrop

                rounded: authorServiceType !== Global._SoftwareServiceType &&
                         source !== chatHandler.getServiceIconUrl(authorServiceType);

                width: Global.windowChatMessageAvatarSize
                height: Global.windowChatMessageAvatarSize
                sourceSize.width: width
                sourceSize.height: height
                mipmap: true

                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.margins: 4

                asynchronous: true
                source: {
                    if (!visible) {
                        return ""
                    }

                    if (messageCustomAuthorAvatarUrl.toString() !== "") {
                        return messageCustomAuthorAvatarUrl;
                    }

                    if (authorAvatarUrl.toString() !== "") {
                        return authorAvatarUrl;
                    }

                    return "" // chatHandler.getServiceIconUrl(authorServiceType)
                }

                MouseArea {
                    anchors.fill: parent;
                    hoverEnabled: authorServiceType !== Global._SoftwareServiceType && !messageIsServiceMessage;
                    acceptedButtons: Qt.LeftButton;
                    cursorShape: {
                        if (hoverEnabled)
                            return Qt.PointingHandCursor;
                    }
                    onClicked: {
                        if (hoverEnabled) {
                            openAuthorWindow(authorId);
                        }
                    }
                }
            }

            //Text Message
            TextEdit {
                id: textEditMessageText

                anchors.left: avatarImage.visible ? avatarImage.right : parent.left
                anchors.right: parent.right
                anchors.top: authorRow.visible ? authorRow.bottom : parent.top
                anchors.margins: 4
                wrapMode: Text.Wrap
                textFormat: TextEdit.RichText
                text: messageHtml
                font.weight: messageIsBotCommand ? Font.Black : Font.DemiBold
                font.italic: messageMarkedAsDeleted || messageIsTwitchAction
                font.bold: authorServiceType === Global._SoftwareServiceType
                selectByMouse: true
                selectByKeyboard: true
                readOnly: true
                color: "#00000000"

                font.letterSpacing: 0.5
                font.pointSize: Global.windowChatMessageTextFontSize

                onLinkActivated: Qt.openUrlExternally(link)
                onHoveredLinkChanged: {
                    // here the code can be empty. The slot is created to change the shape of the cursor when hovering over a link
                }

                onTextChanged: {
                    if (textEditMessageTextTimer.running)
                    {
                        textEditMessageTextTimer.couterTimes = 20
                        textEditMessageTextTimer.running = true
                    }
                }

                Timer {
                    id: textEditMessageTextTimer
                    property int couterTimes: 20
                    interval: 500
                    running: true
                    repeat: true
                    onTriggered: {
                        // this non-obvious code solves the bug https://github.com/3dproger/AxelChat/issues/193
                        redrawTextEdit(parent)
                        couterTimes -= 1
                        if (couterTimes <= 0)
                        {
                            running = false
                        }
                    }
                }

                Text {
                    anchors.fill: parent
                    text: parent.text
                    textFormat: parent.textFormat
                    wrapMode: parent.wrapMode
                    style: Text.Outline
                    visible: true
                    color: {
                        if (messageMarkedAsDeleted)
                        {
                            return "gray";
                        }
                        else
                        {
                            if (messageIsBotCommand)
                            {
                                return "yellow";
                            }
                            else
                            {
                                //Normal message
                                return "white";
                            }
                        }
                    }

                    font: parent.font
                    //font.pointSize: parent.font.pointSize
                }
            }
        }
    }
}
