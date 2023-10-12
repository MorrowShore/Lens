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

    function createComponent(container, index, type, component) {
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

        const Types = {
            Label:      10,
            LineEdit:   20,
            Button:     30,
            Switch:     32,
            Slider:     40,
            ComboBox:   50,
        }

        const container = column

        const labelComponent      = Qt.createComponent("UIBridgeLabel.qml")
        const lineEditComponent   = Qt.createComponent("UIBridgeLineEdit.qml")
        const buttonComponent     = Qt.createComponent("UIBridgeButton.qml")
        const switchComponent     = Qt.createComponent("UIBridgeSwitch.qml")
        const sliderComponent     = Qt.createComponent("UIBridgeSlider.qml")
        const comboBoxComponent   = Qt.createComponent("UIBridgeComboBox.qml")

        for (var i = 0; i < bridge.getElementsCount(); ++i)
        {
            const type = bridge.getElementType(i)

            switch (type)
            {
            case Types.Label:       createComponent(container, i, type, labelComponent);       break
            case Types.LineEdit:    createComponent(container, i, type, lineEditComponent);    break
            case Types.Button:      createComponent(container, i, type, buttonComponent);      break
            case Types.Switch:      createComponent(container, i, type, switchComponent);      break
            case Types.Slider:      createComponent(container, i, type, sliderComponent);      break
            case Types.ComboBox:    createComponent(container, i, type, comboBoxComponent);    break

            default:
                console.error("Unknown bridge component type ", type)
                break
            }
        }
    }
}
