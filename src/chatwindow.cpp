#include "chatwindow.h"
#include "applicationinfo.h"
#include "qmlutils.h"
#include <QQmlEngine>
#include <QQmlContext>
#include <QGuiApplication>
#include <QApplication>
#include <QMenu>

void ChatWindow::declareQml()
{
    qmlRegisterUncreatableType<UIBridge> ("AxelChat.ChatWindow", 1, 0, "ChatWindow", "Type cannot be created in QML");

    UIBridge::declareQml();
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

    , transparentForInput(settings, "transparentForInput", false)
    , stayOnTop(settings, "stayOnTop", false)
    , windowFrame(settings, "windowFrame", true)
    , hideToTrayOnMinimize(settings, "hideToTrayOnMinimize", true)
    , hideToTrayOnClose(settings, "hideToTrayOnClose", false)
{
    {
        //ui.addElement(std::shared_ptr<UIBridgeElement>(UIBridgeElement::createSwitch(&transparentForInput, tr("Ignore Mouse"))));
    }

    {
        tray.setToolTip(QCoreApplication::applicationName());

        connect(&tray, QOverload<QSystemTrayIcon::ActivationReason>::of(&QSystemTrayIcon::activated), this, [this](QSystemTrayIcon::ActivationReason reason)
        {
            if (reason == QSystemTrayIcon::ActivationReason::Trigger)
            {
                toogleVisible();
            }
        });

        QMenu* menu = new QMenu();

        {
            actionHideToTray = new QAction(menu);

            QFont font = actionHideToTray->font();
            font.setBold(true);
            actionHideToTray->setFont(font);

            connect(actionHideToTray, &QAction::triggered, this, [this]()
            {
                toogleVisible();
            });
            menu->addAction(actionHideToTray);

            menu->addSeparator();
        }

        {
            QAction* action = new QAction(QIcon(":/resources/images/applications-system.png"), tr("Settings"), menu);
            connect(action, &QAction::triggered, this, []()
            {
                emit QMLUtils::instance()->triggered("open_settings_window");
            });
            menu->addAction(action);
        }

        {
            QAction* action = new QAction(tr("Ignore Mouse"), menu);
            action->setCheckable(true);

            connect(action, &QAction::triggered, this, [this, action]()
            {
                transparentForInput.set(action->isChecked());
                updateFlags();
            });
            menu->addAction(action);

            action->setChecked(transparentForInput.get());
        }

        {
            QAction* action = new QAction(tr("Stay on top"), menu);
            action->setCheckable(true);

            connect(action, &QAction::triggered, this, [this, action]()
            {
                stayOnTop.set(action->isChecked());
                updateFlags();
            });
            menu->addAction(action);

            action->setChecked(stayOnTop.get());
        }

        {
            QAction* action = new QAction(tr("Window frame"), menu);
            action->setCheckable(true);

            connect(action, &QAction::triggered, this, [this, action]()
            {
                windowFrame.set(action->isChecked());
                updateFlags();
            });
            menu->addAction(action);

            action->setChecked(windowFrame.get());
        }

        {
            QAction* action = new QAction(QIcon("://resources/images/ic-trash.png"), QTranslator::tr("Clear Messages"), menu);
            connect(action, &QAction::triggered, this, [this]()
            {
                chatHandler.clearMessages();
            });
            menu->addAction(action);
        }

        menu->addSeparator();

        {
            QAction* action = new QAction(QIcon(":/resources/images/emblem-unreadable.png"), tr("Close"), menu);
            connect(action, &QAction::triggered, this, []()
            {
                QApplication::quit();
            });
            menu->addAction(action);
        }

        tray.setContextMenu(menu);
        tray.show();
    }

    {
        QQmlEngine* qml = engine();

        qml->rootContext()->setContextProperty("applicationDirPath", QGuiApplication::applicationDirPath());

        qml->rootContext()->setContextProperty("chatWindow",         this);
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

    updateFlags();

#ifdef Q_OS_WINDOWS
    AxelChat::setDarkWindowFrame(winId());
#endif
}

bool ChatWindow::event(QEvent *event)
{
    if (isVisible())
    {
        actionHideToTray->setText(tr("Hide"));
    }
    else
    {
        actionHideToTray->setText(tr("Show"));
    }

    if (event->type() == QEvent::WindowStateChange)
    {
        if (windowState() & Qt::WindowMinimized && hideToTrayOnMinimize.get())
        {
            QTimer::singleShot(250, this, &ChatWindow::hide);
            return true;
        }
    }

    if (event->type() == QEvent::Close)
    {
        if (hideToTrayOnClose.get())
        {
            QTimer::singleShot(250, this, &ChatWindow::hide);
            return true;
        }
        else
        {
            QApplication::quit();
        }
    }

    return QQuickView::event(event);
}

void ChatWindow::toogleVisible()
{
    if (isVisible())
    {
        const QWindowList windows = QApplication::topLevelWindows();

        for (QWindow *window : windows)
        {
            window->hide();
        }
    }
    else
    {
        show();
        requestActivate();
    }
}

void ChatWindow::updateFlags()
{
    Qt::WindowFlags flags;

    flags.setFlag(Qt::WindowType::Window);
    flags.setFlag(Qt::WindowType::CustomizeWindowHint);

    flags.setFlag(Qt::WindowType::WindowTitleHint);
    flags.setFlag(Qt::WindowType::WindowSystemMenuHint);
    flags.setFlag(Qt::WindowType::WindowMinMaxButtonsHint);
    flags.setFlag(Qt::WindowType::WindowContextHelpButtonHint);
    flags.setFlag(Qt::WindowType::WindowCloseButtonHint);

    flags.setFlag(Qt::WindowType::WindowTransparentForInput, transparentForInput.get());
    flags.setFlag(Qt::WindowType::WindowStaysOnTopHint, stayOnTop.get());
    flags.setFlag(Qt::WindowType::FramelessWindowHint, !windowFrame.get());

    setFlags(flags);
}
