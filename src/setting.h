#pragma once

#include "crypto/crypto.h"
#include <QSettings>
#include <QDebug>
#include <QHash>
#include <utility>

template<typename T>
class Setting
{
public:
    Setting(QSettings& settings_, const QString& key_, const T& defaultValue = T(), const bool encrypt_ = false)
        : settings(settings_)
        , key(key_)
        , value(defaultValue)
        , encrypt(encrypt_)
    {
        bool loaded = false;

        if (!key.isEmpty() && settings.contains(key))
        {
            QVariant variant = settings.value(key);
            if (encrypt)
            {
                variant = Crypto::decrypt(variant.toByteArray()).value_or(QByteArray());
            }

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
            if (encrypt)
            {
                settings.setValue(key, Crypto::encrypt(QVariant::fromValue(value).toByteArray()).value_or(QByteArray()));
            }
            else
            {
                settings.setValue(key, QVariant::fromValue(value).toByteArray());
            }
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
    const bool encrypt;

    std::function<void(const T& value)> valueChangedCallback = nullptr;
};
