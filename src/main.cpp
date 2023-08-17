#if defined(AXELCHAT_LIBRARY)
#include "axelchatlib_api.h"
#else
#include "tray.h"
#include "applicationinfo.h"
#include "chathandler.h"
#include "githubapi.h"
#include "qmlutils.h"
#include "commandseditor.h"
#include "uielementbridge.h"
#include "appsponsormanager.h"
#include "chatwindow.h"
#include "CefWebEngineQt/Manager.h"
#include "crypto/crypto.h"
#include <QApplication>
#include <QIcon>
#include <QStandardPaths>
#include <QDir>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QSplashScreen>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName   (APP_INFO_PRODUCTNAME_STR);
    QCoreApplication::setOrganizationName  (APP_INFO_COMPANYNAME_STR);
    QCoreApplication::setOrganizationDomain(APP_INFO_COMPANYDOMAIN_STR);
    QCoreApplication::setApplicationVersion(APP_INFO_PRODUCTVERSION_STR);

    QSettings settings;

    ChatWindow::declareQml();

    //QMLUtils::declareQml();
    //QMLUtils qmlUtils(settings, "qml_utils");

    QApplication app(argc, argv);

    cweqt::Manager web(QCoreApplication::applicationDirPath() + "/CefWebEngine/CefWebEngine.exe");

    if (!Crypto::test())
    {
        qCritical() << Q_FUNC_INFO << "crypto test failed";
    }

    QSplashScreen* splashScreen = new QSplashScreen(QPixmap(":/icon.ico"));
    splashScreen->show();

    //QNetworkAccessManager network;

    app.setWindowIcon(QIcon(":/icon.ico"));

    /*I18n::declareQml();
    I18n i18n(settings, "i18n", nullptr);
    Q_UNUSED(i18n);*/

    //AppSponsorManager appSponsorManager(network);

    //UIElementBridge::declareQml();

    //ChatHandler::declareQml();
    //ChatHandler chatHandler(settings, network, web);

    //GitHubApi::declareQml();
    //GitHubApi github(settings, "update_checker", network);

    //ChatWindow::declareQml();
    ChatWindow chatWindow;
    chatWindow.show();

    /*QQmlApplicationEngine engine;

    i18n.setQmlApplicationEngine(&engine);
    qmlUtils.setQmlApplicationEngine(&engine);

    ClipboardQml::declareQml();
    ClipboardQml clipboard;

    CommandsEditor::declareQml();
    CommandsEditor commandsEditor(chatHandler.getBot());

    Tray::declareQml();
    Tray tray(&engine);

    engine.rootContext()->setContextProperty("applicationDirPath", QGuiApplication::applicationDirPath());

    engine.rootContext()->setContextProperty("i18n",               &i18n);
    engine.rootContext()->setContextProperty("chatHandler",        &chatHandler);
    engine.rootContext()->setContextProperty("outputToFile",       &chatHandler.getOutputToFile());
    engine.rootContext()->setContextProperty("chatBot",            &chatHandler.getBot());
    engine.rootContext()->setContextProperty("authorQMLProvider",  &chatHandler.getAuthorQMLProvider());
    engine.rootContext()->setContextProperty("updateChecker",      &github);
    engine.rootContext()->setContextProperty("clipboard",          &clipboard);
    engine.rootContext()->setContextProperty("qmlUtils",           &qmlUtils);
    engine.rootContext()->setContextProperty("messagesModel",      &chatHandler.getMessagesModel());
    engine.rootContext()->setContextProperty("appSponsorsModel",   &appSponsorManager.model);
    engine.rootContext()->setContextProperty("commandsEditor",     &commandsEditor);
    engine.rootContext()->setContextProperty("tray",               &tray);

    engine.rootContext()->setContextProperty("APP_INFO_LEGALCOPYRIGHT_STR_U", APP_INFO_LEGALCOPYRIGHT_STR_U);
    engine.rootContext()->setContextProperty("APP_INFO_EMAIL_STR", APP_INFO_EMAIL_STR);
    engine.rootContext()->setContextProperty("APP_INFO_SUPPORT_URL_STR", APP_INFO_SUPPORT_URL_STR);
    engine.rootContext()->setContextProperty("APP_INFO_SITE_URL_STR", APP_INFO_SITE_URL_STR);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *obj, const QUrl &objUrl)
    {
        if (!obj && url == objUrl)
        {
            QCoreApplication::exit(-1);
        }

    },
    Qt::QueuedConnection);

    engine.load(url);*/

    splashScreen->close();
    delete splashScreen;
    splashScreen = nullptr;

    return app.exec();
}

#endif
