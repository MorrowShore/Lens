#pragma once

#include "i18n.h"
#include "clipboardqml.h"
#include "chathandler.h"
#include "githubapi.h"
#include "appsponsormanager.h"
#include "commandseditor.h"
#include "setting.h"
#include <QQuickView>
#include <QSystemTrayIcon>

class ChatWindow : public QQuickView
{
    Q_OBJECT
public:
    static void declareQml();

    explicit ChatWindow(QWindow *parent = nullptr);

    virtual bool event(QEvent *event) override;

signals:

private:
    void toogleVisible();

    QSettings settings;

    cweqt::Manager web;
    QNetworkAccessManager network;
    AppSponsorManager appSponsorManager;
    I18n i18n;
    ClipboardQml qmlClipboard;
    GitHubApi github;
    ChatHandler chatHandler;
    CommandsEditor commandsEditor;

    QSystemTrayIcon tray;

    QAction* actionHideToTray = nullptr;

    Setting<bool> hideToTrayOnMinimize;
    Setting<bool> hideToTrayOnClose;
};
