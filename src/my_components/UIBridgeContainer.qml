import QtQuick 2.0

Rectangle
{
    property var bridge: null

    width: column.width
    height: column.height

    color: "transparent"

    Column {
        id: column
    }

    Component.onCompleted: {
        if (!bridge) {
            console.log("Bridge is null");
            return;
        }

        var Types = {
            Label: 10,
            LineEdit: 20,
            Button: 30,
            Switch: 32,
            Slider: 40,
        }

        var container = column

        var labelComponent = Qt.createComponent("../my_components/BridgedLabel.qml")
        var lineEditComponent = Qt.createComponent("../my_components/BridgedLineEdit.qml")
        var buttonComponent = Qt.createComponent("../my_components/BridgedButton.qml")
        var switchComponent = Qt.createComponent("../my_components/BridgedSwitch.qml")
        var sliderComponent = Qt.createComponent("../my_components/UIBridgeSlider.qml")

        for (var i = 0; i < bridge.getElementsCount(); ++i)
        {
            var type = bridge.getElementType(i)
            switch (type)
            {
            case Types.Label:
                if (labelComponent.status === Component.Ready) {
                    bridge.bindQuickItem(i, labelComponent.createObject(container))
                }
                else {
                    console.log("Bridge component type " + type + " not ready, status =", labelComponent.status)
                }
                break

            case Types.LineEdit:
                if (lineEditComponent.status === Component.Ready) {
                    bridge.bindQuickItem(i, lineEditComponent.createObject(container))
                }
                else {
                    console.log("Bridge component type " + type + " not ready, status =", lineEditComponent.status)
                }
                break

            case Types.Button:
                if (buttonComponent.status === Component.Ready) {
                    bridge.bindQuickItem(i, buttonComponent.createObject(container))
                }
                else {
                    console.log("Bridge component type " + type + " not ready, status =", buttonComponent.status)
                }
                break

            case Types.Switch:
                if (switchComponent.status === Component.Ready) {
                    bridge.bindQuickItem(i, switchComponent.createObject(container))
                }
                else {
                    console.log("Bridge component type " + type + " not ready, status =", switchComponent.status)
                }
                break

            case Types.Slider:
                if (sliderComponent.status === Component.Ready) {
                    bridge.bindQuickItem(i, sliderComponent.createObject(container))
                }
                else {
                    console.log("Bridge component type " + type + " not ready, status =", sliderComponent.status)
                }
                break

            default:
                console.error("Unknown bridge component type ", type)
                break
            }
        }
    }
}
