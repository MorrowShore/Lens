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
}

ChatWindow::ChatWindow(QWindow *parent)
    : QQuickView(parent)
    , web(QCoreApplication::applicationDirPath() + "/CefWebEngine/CefWebEngine.exe")
    , appSponsorManager(network)
    , i18n(settings, "i18n", engine())
    , github(settings, "update_checker", network)
    , chatHandler(settings, network, web)
    , commandsEditor(chatHandler.getBot())
    , tray(QIcon(":/resources/images/axelchat-16x16.png"))

    , hideToTrayOnMinimize(settings, "hideToTrayOnMinimize", true)
    , hideToTrayOnClose(settings, "hideToTrayOnClose", false)
{
    {
        tray.setToolTip(QCoreApplication::applicationName());

        connect(&tray, QOverload<QSystemTrayIcon::ActivationReason>::of(&QSystemTrayIcon::activated), this, [this](QSystemTrayIcon::ActivationReason reason)
        {
            if (reason == QSystemTrayIcon::ActivationReason::Trigger)
            {
                show();
                requestActivate();
            }
        });

        tray.show();
    }

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

        qml->rootContext()->setContextProperty("APP_INFO_LEGALCOPYRIGHT_STR_U", APP_INFO_LEGALCOPYRIGHT_STR_U);
        qml->rootContext()->setContextProperty("APP_INFO_EMAIL_STR", APP_INFO_EMAIL_STR);
        qml->rootContext()->setContextProperty("APP_INFO_SUPPORT_URL_STR", APP_INFO_SUPPORT_URL_STR);
        qml->rootContext()->setContextProperty("APP_INFO_SITE_URL_STR", APP_INFO_SITE_URL_STR);

        setResizeMode(QQuickView::ResizeMode::SizeRootObjectToView);

        setColor(QColor(0, 0, 0)); // TODO opacity
        //setOpacity(0.5); // TODO

        setSource(QUrl("qrc:/main.qml"));
    }

#ifdef Q_OS_WINDOWS
    AxelChat::setDarkWindowFrame(winId());
#endif
}

bool ChatWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange &&
        windowState() & Qt::WindowMinimized &&
        hideToTrayOnMinimize.get())
    {
        QTimer::singleShot(250, this, &ChatWindow::hide);
        return true;
    }

    if (event->type() == QEvent::Close &&
        hideToTrayOnClose.get())
    {
        QTimer::singleShot(250, this, &ChatWindow::hide);
        return true;
    }

    return QQuickView::event(event);
}
