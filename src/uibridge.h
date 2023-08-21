#pragma once

#include "uibridgeelement.h"
#include <QObject>

class UIBridge : public QObject
{
    Q_OBJECT
public:
    explicit UIBridge(QObject *parent = nullptr);

    void addElement(const std::shared_ptr<UIBridgeElement>& element);

    Q_INVOKABLE int getElementsCount() const;
    Q_INVOKABLE int getElementType(const int index) const;

    std::shared_ptr<UIBridgeElement> findBySetting(const Setting<QString>& setting) const;

    const QList<std::shared_ptr<UIBridgeElement>>& getElements() const { return elements; }

#ifdef QT_QUICK_LIB
    Q_INVOKABLE void bindQuickItem(const int index, QQuickItem* item);

    static void declareQml()
    {
        qmlRegisterUncreatableType<UIBridge> ("AxelChat.UIBridge", 1, 0, "UIBridge", "Type cannot be created in QML");
        UIBridgeElement::declareQml();
    }
#endif

signals:
    void elementChanged(const std::shared_ptr<UIBridgeElement>& element);

private:
    QList<std::shared_ptr<UIBridgeElement>> elements;
};
