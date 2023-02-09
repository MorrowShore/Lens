#include "oauth2.h"
#include <QDesktopServices>
#include <QUrlQuery>
#include <QDebug>
#include <QCoreApplication>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

OAuth2::OAuth2(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : QObject{parent}
    , network(network_)
    , accessToken(Setting<QString>(settings, settingsGroupPath + "/access_token", QString(), true))
    , refreshToken(Setting<QString>(settings, settingsGroupPath + "/refresh_token", QString(), true))
{
    setNewState(State::NotLoggedIn);

    QObject::connect(&timerValidateToken, &QTimer::timeout, this, [this]()
    {
        validate();
        refresh();
    });
    timerValidateToken.setInterval(10 * 60 * 1000);
}

void OAuth2::setConfig(const Config &config_)
{
    if (configSetted)
    {
        qWarning() << Q_FUNC_INFO << "don't set config more than once";
    }

    config = config_;
    configSetted = true;

    timerValidateToken.start();

    setNewState(State::LoginInProgress);

    validate();
    refresh();
}

bool OAuth2::isLoggedIn() const
{
    return state == State::LoggedIn;
}

OAuth2::State OAuth2::getState() const
{
    return state;
}

TcpReply OAuth2::processTcpRequestAuthCode(const TcpRequest &request)
{
    const QString code = request.getUrlQuery().queryItemValue("code");
    const QString errorDescription = request.getUrlQuery().queryItemValue("error_description").replace('+', ' ');

    if (code.isEmpty())
    {
        if (errorDescription.isEmpty())
        {
            setNewState(State::NotLoggedIn);
            return TcpReply::createTextHtmlError("Code is empty");
        }
        else
        {
            setNewState(State::NotLoggedIn);
            return TcpReply::createTextHtmlError(errorDescription);
        }
    }

    requestOAuthTokenByCode(code);

    return TcpReply::createTextHtmlOK(tr("Now you can close the page and return to %1").arg(QCoreApplication::applicationName()));
}

QString OAuth2::getLogin() const
{
    return _login;
}

QString OAuth2::getAccessToken() const
{
    return accessToken.get();
}

void OAuth2::login()
{
    if (!configSetted)
    {
        qCritical() << Q_FUNC_INFO << "config not setted";
        clear();
        setNewState(State::NotLoggedIn);
        return;
    }

    QUrlQuery query;
    query.addQueryItem("response_type", "code");
    query.addQueryItem("client_id", config.clientId);
    query.addQueryItem("redirect_uri", config.redirectUrl.toString());
    query.addQueryItem("scope", config.scope);

    if (!QDesktopServices::openUrl(config.authorizationCodeRequestPageUrl.toString() + "?" + query.toString()))
    {
        qCritical() << Q_FUNC_INFO << "QDesktopServices: failed to open url";
    }

    setNewState(State::LoginInProgress);
}

void OAuth2::logout()
{
    revoke();
    setNewState(State::NotLoggedIn);
}

void OAuth2::validate()
{
    if (accessToken.get().isEmpty())
    {
        setNewState(State::NotLoggedIn);
        qWarning() << Q_FUNC_INFO << "access token is empty";
        return;
    }

    QNetworkRequest request(config.validateTokenUrl);
    request.setRawHeader("Authorization", QString("OAuth %1").arg(accessToken.get()).toUtf8());

    QNetworkReply* reply = network.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        if (root.contains("login"))
        {
            _login = root.value("login").toString();
            setNewState(State::LoggedIn);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "login not contains in reply";
            revoke();
            setNewState(State::NotLoggedIn);
        }
    });
}

void OAuth2::refresh()
{
    if (refreshToken.get().isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "refresh token is empty";
        revoke();
        return;
    }

    qDebug() << Q_FUNC_INFO << "refreshing token...";

    QNetworkRequest request(config.refreshTokenUrl);
    request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery query;
    query.addQueryItem("grant_type", "refresh_token");
    query.addQueryItem("client_id", config.clientId);
    query.addQueryItem("client_secret", config.clientSecret);
    query.addQueryItem("refresh_token", refreshToken.get());
    query.addQueryItem("redirect_uri", config.redirectUrl.toString());

    QNetworkReply* reply = network.post(request, query.toString().toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            qDebug() << Q_FUNC_INFO << "failed to refresh token, bad reply";
            revoke();
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        const QString accessToken_ = root.value("access_token").toString();
        if (accessToken_.isEmpty())
        {
            qCritical() << Q_FUNC_INFO << "failed to refresh token, access token is empty";
            revoke();
            return;
        }

        const QString refreshToken_ = root.value("refresh_token").toString();
        if (refreshToken_.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "failed to refresh token, refresh token is empty";
            revoke();
            return;
        }

        accessToken.set(accessToken_);
        refreshToken.set(refreshToken_);

        qDebug() << Q_FUNC_INFO << "token successful refreshed";

        validate();
    });
}

void OAuth2::revoke()
{
    if (!accessToken.get().isEmpty())
    {
        QNetworkRequest request(config.revokeTokenUrl);
        request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, "application/x-www-form-urlencoded");

        QUrlQuery query;
        query.addQueryItem("client_id", config.clientId);
        query.addQueryItem("token", accessToken.get());

        QNetworkReply* reply = network.post(request, query.toString().toUtf8());
        connect(reply, &QNetworkReply::finished, this, [this, reply]()
        {
            QByteArray data;
            if (!checkReply(reply, Q_FUNC_INFO, data))
            {
                return;
            }
        });
    }

    clear();

    setNewState(State::NotLoggedIn);
}

void OAuth2::requestOAuthTokenByCode(const QString &code)
{
    setNewState(State::LoginInProgress);

    QNetworkRequest request(config.requestTokenUrl);
    request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery query;
    query.addQueryItem("grant_type", "authorization_code");
    query.addQueryItem("client_id", config.clientId);
    query.addQueryItem("client_secret", config.clientSecret);
    query.addQueryItem("redirect_uri", config.redirectUrl.toString());
    query.addQueryItem("code", code);

    QNetworkReply* reply = network.post(request, query.toString().toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        const QString accessToken_ = root.value("access_token").toString();
        if (accessToken_.isEmpty())
        {
            qCritical() << Q_FUNC_INFO << "access token is empty";
            return;
        }

        const QString refreshToken_ = root.value("refresh_token").toString();
        if (refreshToken_.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "refresh token is empty";
        }

        setNewState(State::LoginInProgress);

        accessToken.set(accessToken_);
        refreshToken.set(refreshToken_);
        validate();
    });
}

bool OAuth2::checkReply(QNetworkReply *reply, const char *tag, QByteArray &resultData)
{
    if (!reply)
    {
        qWarning() << tag << tag << ": !reply";
        return false;
    }

    int statusCode = 200;
    const QVariant rawStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    resultData = reply->readAll();

    reply->deleteLater();

    if (rawStatusCode.isValid())
    {
        statusCode = rawStatusCode.toInt();

        if (statusCode == 401)
        {
            accessToken.set(QString());

            refresh();
            return false;
        }
        else if (statusCode != 200)
        {
            qWarning() << tag << ": status code:" << statusCode;
        }
    }

    if (resultData.isEmpty() && statusCode != 200)
    {
        qWarning() << tag << ": data is empty";
        return false;
    }

    return true;
}

void OAuth2::clear()
{
    accessToken.set(QString());
    refreshToken.set(QString());
    _login = QString();

    emit stateChanged();
}

void OAuth2::setNewState(const State newState)
{
    if (newState != state)
    {
        state = newState;
        emit stateChanged();
    }
}
