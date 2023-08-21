#include "uibridge.h"

UIBridge::UIBridge(QObject *parent)
    : QObject{parent}
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

void UIBridge::addElement(const std::shared_ptr<UIBridgeElement> &element)
{
    if (!element)
    {
        qCritical() << Q_FUNC_INFO << "element is null";
        return;
    }

    connect(element.get(), &UIBridgeElement::valueChanged, this, [this, element]()
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

    elements[index]->bindQuickItem(item);
}

std::shared_ptr<UIBridgeElement> UIBridge::findBySetting(const Setting<QString> &setting) const
{
    for (const std::shared_ptr<UIBridgeElement>& element : qAsConst(elements))
    {
        if (!element)
        {
            continue;
        }

        if (element->getSettingString() == &setting)
        {
            return element;
        }
    }

    return nullptr;
}
