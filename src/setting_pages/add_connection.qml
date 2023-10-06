import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.12
import QtQuick.Window 2.15
import AxelChat.ChatHandler 1.0
import QtQuick.Layouts 1.12
import Qt.labs.settings 1.1
import "../my_components" as MyComponents
import "../"

ScrollView {
    id: root
    clip: true
    padding: 6
    ScrollBar.horizontal.policy: width < contentWidth ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: height < contentHeight ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

    ListView {
        id: listViewCategories
        Layout.fillWidth: true
        Layout.fillHeight: true
        keyNavigationWraps: true
        clip: true
        focus: true

        Component.onCompleted: {
            for (var i = chatHandler.getServicesCount() - 1; i >= 0; --i) {
                var service = chatHandler.getServiceAtIndex(i)
                model.insert(0,
                             {
                                 name: service.getName(),
                                 category: "service",
                                 iconSource: service.getIconUrl().toString(),
                                 serviceIndex: i
                             })
            }

            currentIndex = settings.category_index
        }

        model: ListModel {}

        delegate: ItemDelegate {
            id: categoryDelegate
            width: listViewCategories.width
            text: model.name
            property var iconSource: model.iconSource
            property string category: model.category
            highlighted: ListView.isCurrentItem

            onClicked: {
                listViewCategories.forceActiveFocus();
                listViewCategories.currentIndex = model.index;
            }

            contentItem: Row {
                anchors.fill: parent

                spacing: 8
                leftPadding: 8

                Rectangle {
                    visible: categoryDelegate.highlighted
                    width: 4
                    color: categoryDelegate.Material.accentColor
                }

                Image {
                    mipmap: true
                    height: 24
                    width: height
                    anchors.verticalCenter: parent.verticalCenter
                    source: categoryDelegate.iconSource
                    fillMode: Image.PreserveAspectFit
                    visible: categoryDelegate.iconSource.length > 0

                    Rectangle {
                        property var chatService: chatHandler.getServiceAtIndex(serviceIndex)

                        width: 14
                        height: width
                        x: parent.width - width / 2
                        y: parent.height - height / 2
                        border.width: 2
                        radius: width / 2
                        //border.color: root.color
                        visible: chatService !== null && chatService.enabled

                        color: {
                            if (chatService !== null && chatService.connectionState === Global._ConnectedConnectionState) {
                                return "lime"
                            }

                            return "red"
                        }
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter

                    text: categoryDelegate.text
                    font: categoryDelegate.font
                    color: categoryDelegate.enabled ? categoryDelegate.Material.primaryTextColor
                                          : categoryDelegate.Material.hintTextColor
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
