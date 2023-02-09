#pragma once

#include "setting.h"
#include "tcprequest.h"
#include "tcpreply.h"
#include <QNetworkAccessManager>
#include <QTimer>

class OAuth2 : public QObject
{
    Q_OBJECT
public:
    struct Config
    {
        QString clientId;
        QString clientSecret;
        QUrl authorizationCodeRequestPageUrl;
        QUrl redirectUrl;
        QString scope;
        QUrl requestTokenUrl;
        QUrl validateTokenUrl;
        QUrl refreshTokenUrl;
        QUrl revokeTokenUrl;
    };

    enum class State { NotLoggedIn, LoginInProgress, LoggedIn };

    explicit OAuth2(
            QSettings& settings,
            const QString& settingsGroupPath,
            QNetworkAccessManager& network,
            QObject *parent = nullptr);

    void setConfig(const Config& config);

    bool isLoggedIn() const;
    State getState() const;
    TcpReply processTcpRequestAuthCode(const TcpRequest &request);
    QString getLogin() const;
    // TODO: deffered request
    QString getAccessToken() const;

signals:
    void stateChanged();

public slots:
    void login();
    void logout();
    void refresh();

private slots:
    void validate();
    void revoke();

private:
    void requestOAuthTokenByCode(const QString& code);
    bool checkReply(QNetworkReply *reply, const char *tag, QByteArray& resultData);
    void clear();
    void setNewState(const State state);

    Config config;
    bool configSetted = false;

    State state = State::NotLoggedIn;
    QString _login;

    QNetworkAccessManager& network;

    Setting<QString> accessToken;
    Setting<QString> refreshToken;

    QTimer timerValidateToken;
};
