#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

class Crypto
{
public:
    static std::optional<QString> encrypt(const QByteArray &data);
    static std::optional<QByteArray> decrypt(const QString &data);
    static bool test();

private:
    Crypto(){}

    static bool test(const QByteArray& data);
};
