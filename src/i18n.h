#pragma once

#include <QObject>
#include <QTranslator>
#include <QSettings>
#include <QQmlEngine>

class I18n : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)

public:
    explicit I18n(QSettings& settings, const QString& settingsGroup, QQmlEngine* qml = nullptr, QObject *parent = nullptr);
    Q_INVOKABLE bool setLanguage(const QString& shortTag);
    Q_INVOKABLE QString systemLanguage() const;
    ~I18n();
    static void declareQml();
    QString language() const;

    static I18n* getInstance();

signals:
    void languageChanged();

private:
    QTranslator* _appTranslator = nullptr;
    QSettings& settings;
    const QString SettingsGroupPath = "i18n";

    QQmlEngine* qml = nullptr;

    const QString SETTINGNAME_LANGUAGETAG = "language_tag";
    QString _languageTag;
};

