#ifndef SETTING_H
#define SETTING_H

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
        if (key.isEmpty())
        {
            return false;
        }

        if (value_ == value)
        {
            return false;
        }

        value = value_;
        settings.setValue(key, QVariant::fromValue(value));

        return true;
    }

    inline const QString& getKey() const
    {
        return key;
    }

private:
    QSettings& settings;
    const QString key;
    T value;
};

#endif // SETTING_H
