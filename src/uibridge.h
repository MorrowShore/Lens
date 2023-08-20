#pragma once

#include "uielementbridge.h"
#include <QObject>

class UIBridge : public QObject
{
    Q_OBJECT
public:
    explicit UIBridge(QObject *parent = nullptr);

    void addElement(const std::shared_ptr<UIElementBridge>& element);

    Q_INVOKABLE int getElementsCount() const;
    Q_INVOKABLE int getElementType(const int index) const;
    Q_INVOKABLE void bindQuickItem(const int index, QQuickItem* item);

signals:
    void elementChanged(const std::shared_ptr<UIElementBridge>& element);

private:
    QList<std::shared_ptr<UIElementBridge>> elements;
};
