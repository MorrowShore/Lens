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

    function createComponent(index, type, component) {
        var container = column

        if (component.status === Component.Ready) {
            bridge.bindQuickItem(index, component.createObject(container))
        }
        else {
            console.log("Bridge component type " + type + " not ready, status =", component.status, ", errorString =", component.errorString())
        }
    }

    Component.onCompleted: {
        if (!bridge) {
            console.log("Bridge is null");
            return;
        }

        var Types = {
            Label:      10,
            LineEdit:   20,
            Button:     30,
            Switch:     32,
            Slider:     40,
        }

        var labelComponent      = Qt.createComponent("UIBridgeLabel.qml")
        var lineEditComponent   = Qt.createComponent("UIBridgeLineEdit.qml")
        var buttonComponent     = Qt.createComponent("UIBridgeButton.qml")
        var switchComponent     = Qt.createComponent("UIBridgeSwitch.qml")
        var sliderComponent     = Qt.createComponent("UIBridgeSlider.qml")

        for (var i = 0; i < bridge.getElementsCount(); ++i)
        {
            var type = bridge.getElementType(i)

            switch (type)
            {
            case Types.Label:       createComponent(i, type, labelComponent);       break
            case Types.LineEdit:    createComponent(i, type, lineEditComponent);    break
            case Types.Button:      createComponent(i, type, buttonComponent);      break
            case Types.Switch:      createComponent(i, type, switchComponent);      break
            case Types.Slider:      createComponent(i, type, sliderComponent);      break

            default:
                console.error("Unknown bridge component type ", type)
                break
            }
        }
    }
}
