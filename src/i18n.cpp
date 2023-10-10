#include "i18n.h"
#include <QApplication>
#include <QLocale>
#include <QQmlEngine>

namespace
{

static I18n* instance = nullptr;

}

I18n::I18n(QSettings& settings_, const QString& settingsGroup, QQmlEngine* qml_, QObject *parent) :
    QObject(parent),
    settings(settings_),
    SettingsGroupPath(settingsGroup),
    qml(qml_)
{
    instance = this;

    setLanguage(settings.value(SettingsGroupPath + "/" + SETTINGNAME_LANGUAGETAG, systemLanguage()).toString());
}

bool I18n::setLanguage(const QString &shortTag)
{
    QString normTag = shortTag.toLower().trimmed();
    if (normTag != _languageTag)
    {
        if (_appTranslator)
        {
            qApp->removeTranslator(_appTranslator);
            delete _appTranslator;
            _appTranslator = nullptr;
        }

        _languageTag = "C";

        if (normTag == "c" || normTag == "en") {
            settings.setValue(SettingsGroupPath + "/" + SETTINGNAME_LANGUAGETAG, normTag);

            if (qml)
            {
                qml->retranslate();
            }
            return true;
        }

        _appTranslator = new QTranslator(this);

        const QString fileName = ":/i18n/Translation_" + QLocale(normTag).name();
        if (!_appTranslator->load(fileName))
        {
            qDebug() << "Failed to load translation" << shortTag << "with file" << fileName;

            delete _appTranslator;
            _appTranslator = nullptr;
            QLocale::setDefault(QLocale("C"));
            if (qml)
            {
                qml->retranslate();
            }
            emit languageChanged();
            return false;
        }

        if (!qApp->installTranslator(_appTranslator))
        {
            qDebug() << "Failed to install translator for" << shortTag << "with file" << fileName;

            delete _appTranslator;
            _appTranslator = nullptr;
            QLocale::setDefault(QLocale("C"));
            if (qml)
            {
                qml->retranslate();
            }
            emit languageChanged();
            return false;
        }

        settings.setValue(SettingsGroupPath + "/" + SETTINGNAME_LANGUAGETAG, normTag);

        _languageTag = normTag;
        QLocale::setDefault(normTag);
        if (qml)
        {
            qml->retranslate();
        }
        emit languageChanged();
        return true;
    }

    qDebug() << "Failed set language" << shortTag;

    QLocale::setDefault(QLocale("C"));
    if (qml)
    {
        qml->retranslate();
    }
    emit languageChanged();
    return false;
}

QString I18n::systemLanguage() const
{
    return QLocale::system().bcp47Name();
}

I18n::~I18n()
{

}

void I18n::declareQml()
{
    qmlRegisterUncreatableType<I18n> ("AxelChat.I18n",
                                      1, 0, "I18n", "Type cannot be created in QML");
}

QString I18n::language() const
{
    return _languageTag;
}

I18n *I18n::getInstance()
{
    return instance;
}
