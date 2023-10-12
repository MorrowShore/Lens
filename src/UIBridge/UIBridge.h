#pragma once

#include "setting.h"
#include <QQuickItem>

class UIBridgeElement;

class UIBridge : public QObject
{
    Q_OBJECT
public:
    explicit UIBridge(QObject *parent = nullptr);

    void addElement(const std::shared_ptr<UIBridgeElement>& element);
    std::shared_ptr<UIBridgeElement> addLabel(const QString& text);
    std::shared_ptr<UIBridgeElement> addButton(const QString& text, std::function<void()> invokeCallback);
    std::shared_ptr<UIBridgeElement> addLineEdit(Setting<QString>* setting, const QString& name, const QString& placeHolder = QString(), const bool passwordEcho = false);
    std::shared_ptr<UIBridgeElement> addSwitch(Setting<bool>* setting, const QString& name);
    std::shared_ptr<UIBridgeElement> addSlider(Setting<double>* setting, const QString& name, const double minValue, const double maxValue, const bool valueShowAsPercent = false);
    std::shared_ptr<UIBridgeElement> addComboBox(Setting<int>* setting, const QString& name, const QStringList& items);

    Q_INVOKABLE int getElementsCount() const;
    Q_INVOKABLE int getElementType(const int index) const;

    std::shared_ptr<UIBridgeElement> findBySetting(const Setting<QString>& setting) const;

    const QList<std::shared_ptr<UIBridgeElement>>& getElements() const { return elements; }

#ifdef QT_QUICK_LIB
    Q_INVOKABLE void bindQuickItem(const int index, QQuickItem* item);

    static void declareQml();
#endif

signals:
    void elementChanged(const std::shared_ptr<UIBridgeElement>& element);

private:

    QList<std::shared_ptr<UIBridgeElement>> elements;
};
