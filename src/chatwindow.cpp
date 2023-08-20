#include "chatwindow.h"
#include "applicationinfo.h"
#include "qmlutils.h"
#include <QQmlEngine>
#include <QQmlContext>
#include <QGuiApplication>

void ChatWindow::declareQml()
{
    UIElementBridge::declareQml();
    QMLUtils::declareQml();
    I18n::declareQml();
    ChatHandler::declareQml();
    GitHubApi::declareQml();
    ClipboardQml::declareQml();
    CommandsEditor::declareQml();
    Tray::declareQml();
}

ChatWindow::ChatWindow(QWindow *parent)
    : QQuickView(parent)
    , web(QCoreApplication::applicationDirPath() + "/CefWebEngine/CefWebEngine.exe")
    , appSponsorManager(network)
    , tray(engine())
    , i18n(settings, "i18n", engine())
    , github(settings, "update_checker", network)
    , chatHandler(settings, network, web)
    , commandsEditor(chatHandler.getBot())
{
    QQmlEngine* qml = engine();

    qml->rootContext()->setContextProperty("applicationDirPath", QGuiApplication::applicationDirPath());

    qml->rootContext()->setContextProperty("i18n",               &i18n);
    qml->rootContext()->setContextProperty("chatHandler",        &chatHandler);
    qml->rootContext()->setContextProperty("outputToFile",       &chatHandler.getOutputToFile());
    qml->rootContext()->setContextProperty("chatBot",            &chatHandler.getBot());
    qml->rootContext()->setContextProperty("authorQMLProvider",  &chatHandler.getAuthorQMLProvider());
    qml->rootContext()->setContextProperty("updateChecker",      &github);
    qml->rootContext()->setContextProperty("clipboard",          &qmlClipboard);
    qml->rootContext()->setContextProperty("qmlUtils",           QMLUtils::instance());
    qml->rootContext()->setContextProperty("messagesModel",      &chatHandler.getMessagesModel());
    qml->rootContext()->setContextProperty("appSponsorsModel",   &appSponsorManager.model);
    qml->rootContext()->setContextProperty("commandsEditor",     &commandsEditor);
    qml->rootContext()->setContextProperty("tray",               &tray);

    qml->rootContext()->setContextProperty("APP_INFO_LEGALCOPYRIGHT_STR_U", APP_INFO_LEGALCOPYRIGHT_STR_U);
    qml->rootContext()->setContextProperty("APP_INFO_EMAIL_STR", APP_INFO_EMAIL_STR);
    qml->rootContext()->setContextProperty("APP_INFO_SUPPORT_URL_STR", APP_INFO_SUPPORT_URL_STR);
    qml->rootContext()->setContextProperty("APP_INFO_SITE_URL_STR", APP_INFO_SITE_URL_STR);

    setResizeMode(QQuickView::ResizeMode::SizeRootObjectToView);

    setColor(QColor(0, 0, 0));

    setSource(QUrl("qrc:/main.qml"));
}
