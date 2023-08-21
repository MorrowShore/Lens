#pragma once

#include "uielementbridge.h"
#include <QObject>

class UIBridge : public QObject
{
    Q_OBJECT
public:
    explicit UIBridge(QObject *parent = nullptr);
    ~UIBridge() { qDebug() << "delete UIBridge"; }

    void addElement(const std::shared_ptr<UIElementBridge>& element);

    Q_INVOKABLE int getElementsCount() const;
    Q_INVOKABLE int getElementType(const int index) const;

    std::shared_ptr<UIElementBridge> findBySetting(const Setting<QString>& setting) const;

    const QList<std::shared_ptr<UIElementBridge>>& getElements() const { return elements; }

#ifdef QT_QUICK_LIB
    Q_INVOKABLE void bindQuickItem(const int index, QQuickItem* item);

    static void declareQml()
    {
        qmlRegisterUncreatableType<UIBridge> ("UIBridge", 1, 0, "UIBridge", "Type cannot be created in QML");
        UIElementBridge::declareQml();
    }
#endif

signals:
    void elementChanged(const std::shared_ptr<UIElementBridge>& element);

private:
    QList<std::shared_ptr<UIElementBridge>> elements;
};
