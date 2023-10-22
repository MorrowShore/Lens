#include "chatbot.h"
#include "models/message.h"
#include "utils/QtStringUtils.h"
#include "Backend/BackendManager.h"
#include <QSoundEffect>
#include <QJsonObject>
#include <QJsonArray>
#include <QMediaContent>
#include <QFileInfo>

ChatBot::ChatBot(QSettings& settings_, const QString& settingsGroup, QObject *parent)
    : QObject(parent)
    , settings(settings_)
    , SettingsGroupPath(settingsGroup)

{
    initBuiltinCommands();
    loadCommands();

    setVolume(settings.value(SettingsGroupPath + "/" + _settingsKeyVolume, _volume).toInt());

    setEnabledCommands(settings.value(SettingsGroupPath + "/" + _settingsKeyEnabledCommands, _enabledCommands).toBool());

    setIncludeBuiltInCommands(settings.value(SettingsGroupPath + "/" + _settingsKeyIncludeBuiltInCommands, _includeBuiltInCommands).toBool());
}

int ChatBot::volume() const
{
    return _volume;
}

void ChatBot::setEnabledCommands(bool enabledCommands)
{
    if (_enabledCommands != enabledCommands)
    {
        _enabledCommands = enabledCommands;

        settings.setValue(SettingsGroupPath + "/" + _settingsKeyEnabledCommands, enabledCommands);

        emit enabledCommandsChanged();
    }
}

void ChatBot::setIncludeBuiltInCommands(bool includeBuiltInCommands)
{
    if (_includeBuiltInCommands != includeBuiltInCommands)
    {
        _includeBuiltInCommands = includeBuiltInCommands;

        settings.setValue(SettingsGroupPath + "/" + _settingsKeyIncludeBuiltInCommands, includeBuiltInCommands);

        emit includedBuiltInCommandsChanged();
    }
}

QList<BotAction *> ChatBot::actions() const
{
    return _actions;
}

void ChatBot::addAction(BotAction *action)
{
    if (!action)
    {
        qDebug() << "!action";
        return;
    }

    _actions.append(action);
    saveCommands();
}

void ChatBot::rewriteAction(int index, BotAction *action)
{
    if (!action)
    {
        qDebug() << "!action";
        return;
    }

    if (index < 0)
    {
        qDebug() << "index < 0";
        return;
    }

    if (index >= _actions.count())
    {
        qDebug() << "index >= _actions.count()";
        return;
    }

    _actions[index] = action;

    saveCommands();
}

void ChatBot::deleteAction(int index)
{
    if (index < 0)
    {
        qDebug() << "index < 0";
        return;
    }

    if (index >= _actions.count())
    {
        qDebug() << "index >= _actions.count()";
        return;
    }

    delete _actions[index];
    _actions.removeAt(index);

    saveCommands();
}

bool ChatBot::executeAction(int index)
{
    if (index < 0)
    {
        qDebug() << "index < 0";
        return false;
    }

    if (index >= _actions.count())
    {
        qDebug() << "index >= _actions.count()";
        return false;
    }

    return execute(*_actions[index]);
}

void ChatBot::setVolume(int volume)
{
    if (_volume != volume)
    {
        _volume = volume;
        _player.setVolume(volume);

        settings.setValue(SettingsGroupPath + "/" + _settingsKeyVolume, volume);

        emit volumeChanged();
    }
}

void ChatBot::processMessage(const std::shared_ptr<Message>& message)
{
    if (!message)
    {
        qWarning() << "message is null";
        return;
    }

    if (!_enabledCommands)
    {
        return;
    }

    BackendManager::getInstance()->addUsedFeature("ChatBot");

    for (BotAction* action : qAsConst(_actions))
    {
        if (canExecute(*action, *message) && execute(*action))
        {
            message->setFlag(Message::Flag::BotCommand, true);
        }
    }

    if (_includeBuiltInCommands)
    {
        for (BotAction* action : qAsConst(_builtInActions))
        {
            if (canExecute(*action, *message) && execute(*action))
            {
                message->setFlag(Message::Flag::BotCommand, true);
            }
        }
    }
}

bool ChatBot::canExecute(BotAction& action, const Message &message)
{
    const QString& trimmed = message.toHtml().trimmed();
    const QString& lowered = trimmed.toLower();

    if (action.caseSensitive())
    {
        for (const QString& keyword : action.keywords())
        {
            if (keyword == trimmed)
            {
                return true;
            }
        }
    }
    else
    {
        for (const QString& keyword : action.keywords())
        {
            if (keyword.toLower() == lowered)
            {
                return true;
            }
        }
    }

    return false;
}

bool ChatBot::execute(BotAction &action)
{
    if (!action._active)
    {
        qDebug() << "The period of inactivity has not yet passed";
        return false;
    }

    switch (action._type)
    {
    case BotAction::ActionType::SoundPlay:
    {
        qDebug() << "Playing" << action.soundUrl().toString();

        _player.setVolume(_volume);
        _player.setMedia(QMediaContent(action.soundUrl()));
        _player.play();
    }
        break;
    case BotAction::ActionType::Unknown:
    {
        qDebug() << "unknown action type";
    }
        break;
    }

    if (action._inactivityPeriod > 0)
    {
        action._active = false;
        if (action.exclusiveInactivityPeriod())
        {
            if (action._inactivityPeriod > 0)
            {
                action._inactivityTimer.start(action._inactivityPeriod * 1000);
            }
            else
            {
                action._active = true;
            }
        }
        else
        {
            action._inactivityTimer.start(BotAction::DEFAULT_INACTIVITY_TIME * 1000);
        }
    }

    return true;
}

QString ChatBot::commandsText() const
{
    QString text;

    text += "=================== " + tr("Custom commands") + " ===================\n";

    for (int i = 0; i < _actions.count(); ++i)
    {
        addCommandsText(text, _actions.at(i));
    }

    text += "\n=================== " + tr("Built-in commands") + " ===================\n";

    for (int i = 0; i < _builtInActions.count(); ++i)
    {
        BotAction* action = _builtInActions.at(i);
        if (!action->soundUrl().toString().contains("2nd_channel"))
        {
            addCommandsText(text, action);
        }
    }

    return text;
}

void ChatBot::addCommandsText(QString& text, const BotAction* action) const
{

    for (int j = 0; j < action->_keywords.count(); ++j)
    {
        const QString& keyword = action->_keywords.at(j);

        text += keyword;

        if (j < action->_keywords.count() - 1)
        {
            text += " = ";
        }
    }

    text += " ";
    //text += "\n";
}

QStringList keysInGroup(QSettings& settings, const QString& path)
{
    int depth = 0;
    QString group;
    for (int i = 0; i < path.count(); ++i)
    {
        const QChar& c = path[i];

        if (c == '/')
        {
            settings.beginGroup(group);
            group.clear();
            depth++;
        }
        else
        {
            group += c;
        }
    }

    if (!group.isEmpty())
    {
        settings.beginGroup(group);
        group.clear();
        depth++;
    }

    const QStringList& keys = settings.allKeys();

    for (int i = 0; i < depth; ++i)
    {
        settings.endGroup();
    }

    return keys;
}

void ChatBot::initBuiltinCommands()
{
    static const QString FilePrefix = "qrc:/resources/sound/commands/";

    _builtInActions.append(BotAction::createSoundPlay({"!airhorn"},             QUrl(FilePrefix + "airhorn.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!applause"},            QUrl(FilePrefix + "applause.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!aww"},                 QUrl(FilePrefix + "aww.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!badumts"},             QUrl(FilePrefix + "budum_tss.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!lockAndLoad"},         QUrl(FilePrefix + "chc_lock_and_load.wav")));
    _builtInActions.append(BotAction::createSoundPlay({"!cj"},                  QUrl(FilePrefix + "cj.wav")));
    _builtInActions.append(BotAction::createSoundPlay({"!crickets"},            QUrl(FilePrefix + "crickets.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!doIt"},                QUrl(FilePrefix + "do_it.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!dramatic"},            QUrl(FilePrefix + "dramatic.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!dramatic2"},           QUrl(FilePrefix + "dramatic2.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!error"},               QUrl(FilePrefix + "error_xp.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!gandalf", "!gendalf"}, QUrl(FilePrefix + "gandalf_shallnotpass.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!gg"},                  QUrl(FilePrefix + "gg.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!missionCompleted"},    QUrl(FilePrefix + "gta-sa-done.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!quack"},               QUrl(FilePrefix + "mac-quack.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!toasty"},              QUrl(FilePrefix + "mk-toasty.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!nooo"},                QUrl(FilePrefix + "nooo.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!omaewamou"},           QUrl(FilePrefix + "omaewamou.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!fail"},                QUrl(FilePrefix + "sad_trombone.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!sad"},                 QUrl(FilePrefix + "sad-violin.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!surprise"},            QUrl(FilePrefix + "surprise-mf.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!suspense"},            QUrl(FilePrefix + "suspense.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!tobyfox"},             QUrl(FilePrefix + "tobyfox.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!tuturu"},              QUrl(FilePrefix + "tuturu.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!uwu"},                 QUrl(FilePrefix + "uwu.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!wow"},                 QUrl(FilePrefix + "wow.mp3")));
    _builtInActions.append(BotAction::createSoundPlay({"!yeay"},                QUrl(FilePrefix + "yeay-childrens.mp3")));
}

void ChatBot::saveCommands()
{
    for (int i = 0; i < _actions.count(); ++i)
    {
        BotAction* action = _actions[i];
        settings.setValue(SettingsGroupPath + "/" + _settingsGroupActions + QString("/%1").arg(i), action->toJson());
    }

    const QStringList& keysToRemove = keysInGroup(settings, SettingsGroupPath + "/" + _settingsGroupActions);

    for (int i = _actions.count(); i < keysToRemove.count(); ++i)
    {
        settings.remove(SettingsGroupPath + "/" + _settingsGroupActions + "/" + keysToRemove[i]);
    }
}

void ChatBot::loadCommands()
{
    const QStringList& commandsGroups = keysInGroup(settings, SettingsGroupPath + "/" + _settingsGroupActions);

    for (const QString& commandGroup : commandsGroups)
    {        const QJsonObject& object = settings.value(SettingsGroupPath + "/" + _settingsGroupActions + "/" + commandGroup,
                         QJsonObject())
                .toJsonObject();

        if (!object.isEmpty())
        {
            BotAction* action = BotAction::fromJson(object);
            if (action)
            {
                _actions.append(action);
            }
        }
    }
}

