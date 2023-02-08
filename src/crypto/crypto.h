#pragma once

#include <QByteArray>
#include <optional>

class Crypto
{
public:
    static std::optional<QByteArray> encrypt(const QByteArray &data);
    static std::optional<QByteArray> decrypt(const QByteArray &data);
    static bool test();

private:
    Crypto(){}

    static bool test(const QByteArray& data);
};
