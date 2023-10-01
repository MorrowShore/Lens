#if defined(AXELCHAT_LIBRARY)
#include "axelchatlib_api.h"
#else
#include "applicationinfo.h"
#include "qmlutils.h"
#include "chatwindow.h"
#include "loghandler.h"
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
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName   (APP_INFO_PRODUCTNAME_STR);
    QCoreApplication::setOrganizationName  (APP_INFO_COMPANYNAME_STR);
    QCoreApplication::setOrganizationDomain(APP_INFO_COMPANYDOMAIN_STR);
    QCoreApplication::setApplicationVersion(APP_INFO_PRODUCTVERSION_STR);

    LogHandler::initialize(false);

    QSettings settings;

    ChatWindow::declareQml();

    QMLUtils::declareQml();
    QMLUtils qmlUtils(settings, "qml_utils");

    QApplication app(argc, argv);

    QApplication::setQuitOnLastWindowClosed(false);

    if (!Crypto::test())
    {
        qCritical() << "crypto test failed";
    }

    QSplashScreen* splashScreen = new QSplashScreen(QPixmap(":/icon.ico"));
    splashScreen->show();

    app.setWindowIcon(QIcon(":/icon.ico"));

    ChatWindow chatWindow;

    splashScreen->close();
    delete splashScreen;
    splashScreen = nullptr;

    return app.exec();
}

#endif
