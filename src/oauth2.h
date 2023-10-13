#pragma once

#include "setting.h"
#include "tcprequest.h"
#include "tcpreply.h"
#include <QNetworkAccessManager>
#include <QTimer>
#include <QJsonObject>

class OAuth2 : public QObject
{
    Q_OBJECT
public:
    enum class FlowType { Implicit, AuthorizationCode, /*ClientCredentials*/ };

    struct Config
    {
        QString clientId;
        QString clientSecret;
        QUrl authorizationPageUrl;
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
    TcpReply processRedirect(const TcpRequest &request);
    QJsonObject getAuthorizationInfo() const { return authorizationInfo; }
    // TODO: deffered request
    QString getAccessToken() const;
    void setToken(const QString& token);

signals:
    void stateChanged();

public slots:
    void login(const FlowType flowType, const QString& redirectUri);
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

    const Config config;
    bool configSetted = false;

    State state = State::NotLoggedIn;
    QJsonObject authorizationInfo;

    QNetworkAccessManager& network;

    Setting<QString> redirectUri;
    Setting<QString> accessToken;
    Setting<QString> refreshToken;

    QTimer timerValidateToken;
};
