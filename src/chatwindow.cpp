#include "chatwindow.h"
#include "applicationinfo.h"
#include "qmlutils.h"
#include <QQmlEngine>
#include <QQmlContext>
#include <QGuiApplication>
#include <QApplication>
#include <QMenu>
#include <QScreen>

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
    , normalSizeWidthSetting(settings, "normalSizeWidth", 350)
    , normalSizeHeightSetting(settings, "normalSizeHeight", 600)
    , web(QCoreApplication::applicationDirPath() + "/CefWebEngine/CefWebEngine.exe")
    , appSponsorManager(network)
    , i18n(settings, "i18n", engine())
    , github(settings, "update_checker", network)
    , chatHandler(settings, network, web)
    , commandsEditor(chatHandler.getBot())
    , tray(QIcon(":/resources/images/axelchat-16x16.png"))

    , hideToTrayOnMinimize(settings, "hideToTrayOnMinimize", false)
    , hideToTrayOnClose(settings, "hideToTrayOnClose", false)
    , runMinimizedToTray(settings, "runMinimizedToTray", false)
    , transparentForInput(settings, "transparentForInput", false)
    , stayOnTop(settings, "stayOnTop", false)
    , windowFrame(settings, "windowFrame", true)
    , backgroundOpacity(settings, "backgroundOpacity", 0.4)
    , windowOpacity(settings, "windowOpacity", 1.0)
{
    transparentForInput.setCallbackValueChanged([this](bool)
    {
        updateWindow();
    });

    ui.addSwitch(&hideToTrayOnMinimize, tr("Hide to tray when minimized"));
    ui.addSwitch(&hideToTrayOnClose, tr("Hide to tray on close"));

    ui.addSwitch(&runMinimizedToTray, tr("Run minimized to tray"));

    connect(ui.addSlider(&backgroundOpacity, tr("Background opacity"), 0.0, 1.0, true).get(), &UIBridgeElement::valueChanged, this, &ChatWindow::updateWindow);
    connect(ui.addSlider(&windowOpacity, tr("Window opacity"), 0.1, 1.0, true).get(), &UIBridgeElement::valueChanged, this, &ChatWindow::updateWindow);

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
            auto element = ui.addSwitch(&stayOnTop, tr("Stay on top"));

            QAction* action = new QAction(menu);
            element->bindAction(action);

            menu->addAction(action);

            connect(element.get(), &UIBridgeElement::valueChanged, this, &ChatWindow::updateWindow);
        }

        {
            auto element = ui.addSwitch(&windowFrame, tr("Window frame"));

            QAction* action = new QAction(menu);
            element->bindAction(action);

            menu->addAction(action);

            connect(element.get(), &UIBridgeElement::valueChanged, this, &ChatWindow::updateWindow);
        }

        {
            auto element = ui.addSwitch(&transparentForInput, tr("Ignore Mouse"));

            QAction* action = new QAction(menu);
            element->bindAction(action);

            menu->addAction(action);

            connect(element.get(), &UIBridgeElement::valueChanged, this, &ChatWindow::updateWindow);
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

        setSource(QUrl("qrc:/main.qml"));
    }

    updateWindow();

#ifdef Q_OS_WINDOWS
    AxelChat::setDarkWindowFrame(winId());
#endif

    loadWindowSize();

    if (!runMinimizedToTray.get())
    {
        show();
    }
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
            QTimer::singleShot(250, this, &ChatWindow::hideAll);
            return true;
        }
    }

    if (event->type() == QEvent::Close)
    {
        saveWindowSize();

        if (hideToTrayOnClose.get())
        {
            QTimer::singleShot(250, this, &ChatWindow::hideAll);
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
        hideAll();
    }
    else
    {
        show();
        requestActivate();
    }
}

void ChatWindow::updateWindow()
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

    setColor(QColor(0, 0, 0, 255 * backgroundOpacity.get()));
    setOpacity(windowOpacity.get());
}

void ChatWindow::hideAll()
{
    const QWindowList windows = QApplication::topLevelWindows();

    for (QWindow *window : windows)
    {
        window->hide();
    }
}

void ChatWindow::saveWindowSize()
{
    normalSizeWidthSetting.set(width());
    normalSizeHeightSetting.set(height());
}

void ChatWindow::loadWindowSize()
{
    setWidth(normalSizeWidthSetting.get());
    setHeight(normalSizeHeightSetting.get());

    int width = frameGeometry().width();
    int height = frameGeometry().height();

    if (QScreen *screen = QApplication::primaryScreen(); screen)
    {
        int screenWidth = screen->geometry().width();
        int screenHeight = screen->geometry().height();

        setGeometry((screenWidth / 2) - (width / 2), (screenHeight / 2) - (height / 2), width, height);
    }
}
