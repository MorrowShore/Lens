QT += widgets gui quick multimedia websockets network svg

include(CefWebEngineQt/CefWebEngineQt.pri)

CONFIG += c++17

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Refer to the documentation for the
# deprecated API to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_MESSAGELOGCONTEXT

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

HEADERS += \
    BackendManager.h \
    ChatManager.h \
    Feature.h \
    UIBridge/UIBridge.h \
    UIBridge/UIBridgeElement.h \
    appsponsormanager.h \
    chat_services/discord/Channel.h \
    chat_services/discord/Guild.h \
    chat_services/discord/User.h \
    chat_services/discord/GuildsStorage.h \
    chat_services/dlive.h \
    chat_services/donatepay.h \
    chat_services/donationalerts.h \
    chat_services/kick.h \
    chat_services/odysee.h \
    chat_services/rumble.h \
    chat_services/vkvideo.h \
    chat_services/wasd.h \
    chat_services/youtube/youtubebrowser.h \
    chat_services/youtube/youtubehtml.h \
    chat_services/youtube/youtubeutils.h \
    chatwindow.h \
    crypto/aes.h \
    crypto/crypto.h \
    crypto/obfuscator.h \
    emote_services/betterttv.h \
    emote_services/emoteservice.h \
    emote_services/frankerfacez.h \
    emote_services/seventv.h \
    emotesprocessor.h \
    log/logdialog.h \
    log/loghandler.h \
    log/logmessage.h \
    log/logmodel.h \
    oauth2.h \
    ssemanager.h \
    tcpreply.h \
    tcprequest.h \
    chat_services/chatservice.h \
    chat_services/chatservicestypes.h \
    chat_services/discord/Discord.h \
    chat_services/telegram.h \
    chat_services/trovo.h \
    chat_services/vkplaylive.h \
    chat_services/goodgame.h \
    chat_services/twitch.h \
    models/author.h \
    models/message.h \
    models/messagesmodel.h \
    secrets.h \
    applicationinfo.h \
    authorqmlprovider.h \
    botaction.h \
    chatbot.h \
    clipboardqml.h \
    commandseditor.h \
    commandsingleeditor.h \
    githubapi.h \
    i18n.h \
    outputtofile.h \
    setting.h \
    tcpserver.h \
    utils/QmlUtils.h \
    utils/QtAxelChatUtils.h \
    utils/QtMiscUtils.h \
    utils/QtStringUtils.h \
    websocket.h

SOURCES += \
    BackendManager.cpp \
    ChatManager.cpp \
    Feature.cpp \
    UIBridge/UIBridge.cpp \
    UIBridge/UIBridgeElement.cpp \
    appsponsormanager.cpp \
    chat_services/chatservice.cpp \
    chat_services/discord/Guild.cpp \
    chat_services/discord/Discord.cpp \
    chat_services/discord/GuildsStorage.cpp \
    chat_services/dlive.cpp \
    chat_services/donatepay.cpp \
    chat_services/donationalerts.cpp \
    chat_services/kick.cpp \
    chat_services/odysee.cpp \
    chat_services/rumble.cpp \
    chat_services/telegram.cpp \
    chat_services/trovo.cpp \
    chat_services/vkplaylive.cpp \
    chat_services/goodgame.cpp \
    chat_services/twitch.cpp \
    chat_services/vkvideo.cpp \
    chat_services/wasd.cpp \
    chat_services/youtube/youtubebrowser.cpp \
    chat_services/youtube/youtubehtml.cpp \
    chat_services/youtube/youtubeutils.cpp \
    chatwindow.cpp \
    crypto/aes.cpp \
    crypto/crypto.cpp \
    emote_services/betterttv.cpp \
    emote_services/frankerfacez.cpp \
    emote_services/seventv.cpp \
    emotesprocessor.cpp \
    log/logdialog.cpp \
    log/loghandler.cpp \
    models/author.cpp \
    log/logmodel.cpp \
    models/message.cpp \
    models/messagesmodel.cpp \
    authorqmlprovider.cpp \
    botaction.cpp \
    chatbot.cpp \
    clipboardqml.cpp \
    commandseditor.cpp \
    commandsingleeditor.cpp \
    githubapi.cpp \
    i18n.cpp \
    main.cpp \
    oauth2.cpp \
    outputtofile.cpp \
    ssemanager.cpp \
    tcpserver.cpp \
    utils/QmlUtils.cpp \
    utils/QtAxelChatUtils.cpp \
    utils/QtMiscUtils.cpp \
    utils/QtStringUtils.cpp \
    websocket.cpp

RESOURCES += qml.qrc \
    resources.qrc

contains(QT_ARCH, i386)|contains(QT_ARCH, x86_64) {
    RESOURCES += builtin_commands.qrc
}

TRANSLATIONS += \
    Translation_ru_RU.ts

CONFIG += lrelease
CONFIG += embed_translations

RC_FILE = rc.rc

FORMS += \
    commandseditor.ui \
    commandsingleeditor.ui \
    log/logdialog.ui

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

ANDROID_EXTRA_LIBS += \
    $$_PRO_FILE_PWD_/../app_files/arm/libcrypto_1_1.so \
    $$_PRO_FILE_PWD_/../app_files/arm/libssl_1_1.so

win32: {
    LIBS += -lDwmapi

    #Подключаем SSL для Windows. Соответствующий модуль должен быть установлён!!!

    contains(QT_ARCH, i386) {
        #Для Windows x32
        #INCLUDEPATH += $$(QTDIR)/../../Tools/OpenSSL/Win_x86/include
        #LIBS += -L$$(QTDIR)/../../Tools/OpenSSL/Win_x86/lib -llibcrypto -llibssl
    } else {
        #Для Windows x64
        #INCLUDEPATH += $$(QTDIR)/../../Tools/OpenSSL/Win_x64/include
        #LIBS += -L$$(QTDIR)/../../Tools/OpenSSL/Win_x64/lib -llibcrypto -llibssl
    }


    #Сборка файлов релизной версии

    CONFIG(debug, debug|release) {
        #debug
    } else {
        #release
        contains(QT_ARCH, i386) {
            #Для Windows x32
            DESTDIR = $$_PRO_FILE_PWD_/../release_win32
        } else {
            #Для Windows x64
            DESTDIR = $$_PRO_FILE_PWD_/../release_win64
        }

        #QMAKE_POST_LINK += $$(QTDIR)/bin/windeployqt --release --qmldir $$(QTDIR)/qml $$DESTDIR $$escape_expand(\\n\\t) # In Qt 5.15 with --release not working
        QMAKE_POST_LINK += $$(QTDIR)/bin/windeployqt --qmldir $$(QTDIR)/qml $$DESTDIR $$escape_expand(\\n\\t)
    }
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
