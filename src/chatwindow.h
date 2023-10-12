#pragma once

#include "i18n.h"
#include "clipboardqml.h"
#include "ChatManager.h"
#include "githubapi.h"
#include "appsponsormanager.h"
#include "commandseditor.h"
#include "BackendManager.h"
#include "log/logdialog.h"
#include "setting.h"
#include "UIBridge/UIBridge.h"
#include <QQuickView>
#include <QSystemTrayIcon>

class ChatWindow : public QQuickView
{
    Q_OBJECT
public:
    static void declareQml();

    explicit ChatWindow(QNetworkAccessManager& network, BackendManager& backend, QWindow *parent = nullptr);

    virtual bool event(QEvent *event) override;

    Q_INVOKABLE UIBridge* getUiBridge() const { return &const_cast<UIBridge&>(ui); }

signals:

public slots:
    void hideAll();

private slots:
    void toogleVisible();
    void updateWindow();
    void saveWindowSize();
    void loadWindowSize();

private:
    QSettings settings;

    Setting<int> normalSizeWidthSetting;
    Setting<int> normalSizeHeightSetting;

    cweqt::Manager web;
    QNetworkAccessManager& network;
    AppSponsorManager appSponsorManager;
    BackendManager& backend;
    I18n i18n;
    ClipboardQml qmlClipboard;
    GitHubApi github;
    ChatManager chatManager;
    CommandsEditor commandsEditor;
    LogDialog logWindow;

    QSystemTrayIcon tray;

    UIBridge ui;

    QAction* actionHideToTray = nullptr;

    Setting<bool> hideToTrayOnMinimize;
    Setting<bool> hideToTrayOnClose;
    Setting<bool> runMinimizedToTray;

    Setting<bool> transparentForInput;
    Setting<bool> stayOnTop;
    Setting<bool> windowFrame;

    Setting<double> backgroundOpacity;
    Setting<double> windowOpacity;
};
