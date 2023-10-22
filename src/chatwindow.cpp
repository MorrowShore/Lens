#include "chatwindow.h"
#include "applicationinfo.h"
#include "utils/QtMiscUtils.h"
#include "utils/QmlUtils.h"
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
    QmlUtils::declareQml();
    I18n::declareQml();
    ChatManager::declareQml();
    GitHubApi::declareQml();
    ClipboardQml::declareQml();
    CommandsEditor::declareQml();
    LogDialog::declareQml();
}

ChatWindow::ChatWindow(QNetworkAccessManager& network_, BackendManager& backend_, QWindow *parent)
    : QQuickView(parent)
    , normalSizeWidthSetting(settings, "normalSizeWidth", 350)
    , normalSizeHeightSetting(settings, "normalSizeHeight", 600)
    , web(QCoreApplication::applicationDirPath() + "/CefWebEngine/CefWebEngine.exe")
    , network(network_)
    , backend(backend_)
    , i18n(settings, "i18n", engine())
    , github(settings, "update_checker", network_)
    , chatManager(settings, network_, web, backend)
    , commandsEditor(chatManager.getBot())
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

    connect(&web, &cweqt::Manager::initialized, this, [this]()
    {
        const auto info = web.getEngineInfo();
        backend.setUsedWebEngineVersion(info.version, info.chromiumVersion);
    });

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
        }

        menu->addSeparator();

        {
            QAction* action = new QAction(QIcon(":/resources/images/link.svg"), tr("Connections"), menu);
            connect(action, &QAction::triggered, this, []()
            {
                emit QmlUtils::instance()->triggered("open_connections_window");
            });
            menu->addAction(action);
        }

        {
            QAction* action = new QAction(QIcon(":/resources/images/applications-system.png"), tr("Settings"), menu);
            connect(action, &QAction::triggered, this, []()
            {
                emit QmlUtils::instance()->triggered("open_settings_window");
            });
            menu->addAction(action);
        }

        menu->addSeparator();

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
            QAction* action = new QAction(QIcon(":/resources/images/ic-trash.png"), QTranslator::tr("Clear Messages"), menu);
            connect(action, &QAction::triggered, this, [this]()
            {
                chatManager.clearMessages();
            });
            menu->addAction(action);
        }

        menu->addSeparator();

        {
            QAction* action = new QAction(QIcon(":/resources/images/emblem-unreadable.png"), tr("Close"), menu);
            connect(action, &QAction::triggered, this, []()
            {
                QtMiscUtils::quitDeferred();
            });
            menu->addAction(action);
        }

        tray.setContextMenu(menu);
        tray.show();
    }

    {
        contextMenu = new QMenu();

        {
            QAction* action = new QAction(QIcon(":/resources/images/link.svg"), tr("Connections"), contextMenu);

            QFont font = action->font();
            font.setBold(true);
            action->setFont(font);

            connect(action, &QAction::triggered, this, []()
            {
                emit QmlUtils::instance()->triggered("open_connections_window");
            });
            contextMenu->addAction(action);
        }

        {
            QAction* action = new QAction(QIcon(":/resources/images/applications-system.png"), tr("Settings"), contextMenu);
            connect(action, &QAction::triggered, this, []()
            {
                emit QmlUtils::instance()->triggered("open_settings_window");
            });
            contextMenu->addAction(action);
        }

        contextMenu->addSeparator();

        {
            QAction* action = new QAction(QIcon(":/resources/images/ic-trash.png"), QTranslator::tr("Clear Messages"), contextMenu);
            connect(action, &QAction::triggered, this, [this]()
            {
                chatManager.clearMessages();
            });
            contextMenu->addAction(action);
        }

        {
            QAction* action = new QAction(QIcon(":/resources/images/hide.svg"), tr("Hide in tray"), contextMenu);
            connect(action, &QAction::triggered, this, [this]()
            {
                toogleVisible();
            });
            contextMenu->addAction(action);
        }
    }

    {
        QQmlEngine* qml = engine();

        qml->rootContext()->setContextProperty("applicationDirPath", QGuiApplication::applicationDirPath());

        qml->rootContext()->setContextProperty("chatWindow",         this);
        qml->rootContext()->setContextProperty("i18n",               &i18n);
        qml->rootContext()->setContextProperty("chatManager",        &chatManager);
        qml->rootContext()->setContextProperty("outputToFile",       &chatManager.getOutputToFile());
        qml->rootContext()->setContextProperty("chatBot",            &chatManager.getBot());
        qml->rootContext()->setContextProperty("authorQMLProvider",  &chatManager.getAuthorQMLProvider());
        qml->rootContext()->setContextProperty("updateChecker",      &github);
        qml->rootContext()->setContextProperty("clipboard",          &qmlClipboard);
        qml->rootContext()->setContextProperty("qmlUtils",           QmlUtils::instance());
        qml->rootContext()->setContextProperty("messagesModel",      &chatManager.getMessagesModel());
        qml->rootContext()->setContextProperty("appSponsorsModel",   &backend.sponsorship.model);
        qml->rootContext()->setContextProperty("commandsEditor",     &commandsEditor);
        qml->rootContext()->setContextProperty("logWindow",          &logWindow);

        qml->rootContext()->setContextProperty("APP_INFO_LEGALCOPYRIGHT_STR_U", APP_INFO_LEGALCOPYRIGHT_STR_U);
        qml->rootContext()->setContextProperty("APP_INFO_EMAIL_STR", APP_INFO_EMAIL_STR);
        qml->rootContext()->setContextProperty("APP_INFO_SUPPORT_URL_STR", APP_INFO_SUPPORT_URL_STR);
        qml->rootContext()->setContextProperty("APP_INFO_SITE_URL_STR", APP_INFO_SITE_URL_STR);

        setResizeMode(QQuickView::ResizeMode::SizeRootObjectToView);

        setSource(QUrl("qrc:/main.qml"));
    }

    updateWindow();

#ifdef Q_OS_WINDOWS
    QtMiscUtils::setDarkWindowFrame(winId());
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
        actionHideToTray->setIcon(QIcon(":/resources/images/hide.svg"));
    }
    else
    {
        actionHideToTray->setText(tr("Show"));
        actionHideToTray->setIcon(QIcon(":/resources/images/show.svg"));
    }

    if (event->type() == QEvent::WindowStateChange)
    {
        if (windowState() & Qt::WindowMinimized && hideToTrayOnMinimize.get())
        {
            QTimer::singleShot(250, this, [this](){ hideAll(false); });
            return true;
        }
    }

    if (event->type() == QEvent::Close)
    {
        saveWindowSize();

        if (hideToTrayOnClose.get())
        {
            QTimer::singleShot(250, this, [this](){ hideAll(false); });
            return true;
        }
        else
        {
            QtMiscUtils::quitDeferred();
        }
    }

    return QQuickView::event(event);
}

void ChatWindow::toogleVisible()
{
    if (isVisible())
    {
        hideAll(false);
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

void ChatWindow::hideAll(const bool includeTray)
{
    const QWindowList windows = QApplication::topLevelWindows();

    for (QWindow *window : windows)
    {
        window->hide();
    }

    if (includeTray)
    {
        tray.hide();
    }
}

void ChatWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MouseButton::RightButton)
    {
        event->accept();

        if (!contextMenu)
        {
            qCritical() << "context menu is null";
            return;
        }

        contextMenu->exec(event->globalPos());

        return;
    }

    QQuickView::mouseReleaseEvent(event);
}

void ChatWindow::saveWindowSize()
{
    const QSize size = geometry().size();
    normalSizeWidthSetting.set(size.width());
    normalSizeHeightSetting.set(size.height());
}

void ChatWindow::loadWindowSize()
{
    setGeometry(QRect(0, 0, normalSizeWidthSetting.get(), normalSizeHeightSetting.get()));

    int width = geometry().width();
    int height = geometry().height();

    if (QScreen *screen = QApplication::primaryScreen(); screen)
    {
        int screenWidth = screen->geometry().width();
        int screenHeight = screen->geometry().height();

        setGeometry((screenWidth / 2) - (width / 2), (screenHeight / 2) - (height / 2), width, height);
    }
}
