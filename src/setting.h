#pragma once

#include <QSettings>
#include <QDebug>
#include <QHash>
#include <utility>

template<typename T>
class Setting
{
public:
    Setting(QSettings& settings_, const QString& key_, const T& defaultValue = T())
        : settings(settings_)
        , key(key_)
        , value(defaultValue)
    {
        bool loaded = false;

        if (!key.isEmpty() && settings.contains(key))
        {
            const QVariant variant = settings.value(key);
            if (variant.canConvert<T>())
            {
                value = variant.value<T>();
                loaded = true;
            }
            else
            {
                qWarning() << QString("Cannot be converted QVariant to ") + typeid(T).name();
            }
        }

        if (!loaded)
        {
            value = defaultValue;
        }
    }

    inline const T& get() const
    {
        return value;
    }

    bool set(const T& value_)
    {
        if (value_ == value)
        {
            return false;
        }

        value = value_;

        if (!key.isEmpty())
        {
            settings.setValue(key, QVariant::fromValue(value));
        }

        if (valueChangedCallback)
        {
            valueChangedCallback(value);
        }

        return true;
    }

    inline const QString& getKey() const
    {
        return key;
    }

    inline void setCallbackValueChanged(const std::function<void(const T& value)> valueChangedCallback_)
    {
        valueChangedCallback = valueChangedCallback_;
    }

private:
    QSettings& settings;
    const QString key;
    T value;

    std::function<void(const T& value)> valueChangedCallback = nullptr;
};
