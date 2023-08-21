#pragma once

#include "i18n.h"
#include "clipboardqml.h"
#include "chathandler.h"
#include "githubapi.h"
#include "appsponsormanager.h"
#include "commandseditor.h"
#include "setting.h"
#include "uibridge.h"
#include <QQuickView>
#include <QSystemTrayIcon>

class ChatWindow : public QQuickView
{
    Q_OBJECT
public:
    static void declareQml();

    explicit ChatWindow(QWindow *parent = nullptr);

    virtual bool event(QEvent *event) override;

    Q_INVOKABLE UIBridge* getUiBridge() const { return &const_cast<UIBridge&>(ui); }

signals:

private slots:
    void toogleVisible();
    void updateWindow();
    void hideAll();
    void saveWindowSize();
    void loadWindowSize();

private:
    QSettings settings;

    Setting<int> normalSizeWidthSetting;
    Setting<int> normalSizeHeightSetting;

    cweqt::Manager web;
    QNetworkAccessManager network;
    AppSponsorManager appSponsorManager;
    I18n i18n;
    ClipboardQml qmlClipboard;
    GitHubApi github;
    ChatHandler chatHandler;
    CommandsEditor commandsEditor;

    QSystemTrayIcon tray;

    UIBridge ui;

    QAction* actionHideToTray = nullptr;

    Setting<bool> hideToTrayOnMinimize;
    Setting<bool> hideToTrayOnClose;

    Setting<bool> transparentForInput;
    Setting<bool> stayOnTop;
    Setting<bool> windowFrame;

    Setting<double> backgroundOpacity;
    Setting<double> windowOpacity;
};
