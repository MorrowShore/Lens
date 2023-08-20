#include "uibridge.h"

UIBridge::UIBridge(QObject *parent)
    : QObject{parent}
{

}

void UIBridge::addElement(const std::shared_ptr<UIElementBridge> &element)
{
    if (!element)
    {
        qCritical() << Q_FUNC_INFO << "element is null";
        return;
    }

    connect(element.get(), &UIElementBridge::valueChanged, this, [this, element]()
    {
        emit elementChanged(element);
    });

    elements.append(element);
}

int UIBridge::getElementsCount() const
{
    return elements.count();
}

int UIBridge::getElementType(const int index) const
{
    if (index < 0 || index >= elements.count())
    {
        qCritical() << Q_FUNC_INFO << "invalid index" << index;
        return -1;
    }

    return elements[index]->getTypeInt();
}

void UIBridge::bindQuickItem(const int index, QQuickItem *item)
{
    if (index < 0 || index >= elements.count())
    {
        qCritical() << Q_FUNC_INFO << "invalid index" << index;
        return ;
    }

    elements[index]->bindQmlItem(item);
}
