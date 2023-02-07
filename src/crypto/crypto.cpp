#include "crypto.h"
#include "secrets.h"
#include "obfuscator.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QStringList>
#include <QDebug>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

namespace
{

static const QString CryptoKey = OBFUSCATE(CRYPTO_KEY);

static void printError(const QString& tag, const QString& additionalText)
{
    const auto errorCode = ERR_get_error();

    static const int BufferSize = 1024;
    char* buffer = new char[BufferSize];

    ERR_error_string_n(errorCode, buffer, BufferSize);

    qCritical() << QString(tag + ": " + additionalText + ", ssl error: " + QString::fromUtf8(buffer));

    delete[] buffer;
}

static QString random(int length)
{
    static const QString charset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

    QString result;

    for (auto i = 0; i < length; ++i)
    {
        auto j = QRandomGenerator::global()->bounded(0, charset.length() - 1);
        result.append(charset.at(j));
    }

    return result;
}

static char *bytes2chara(const QByteArray &bytes)
{
    auto size = bytes.size() + 1;
    auto data = new char[size];
    strcpy_s(data, size, bytes.data());
    return data;
}

static int decrypt_aes_256(unsigned char *input, unsigned char *key, unsigned char *iv, unsigned char *output)
{
    int session, total;
    auto ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        printError(Q_FUNC_INFO, "failed to create context");
        return -1;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1)
    {
        printError(Q_FUNC_INFO, "failed to init decrypt");
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_DecryptUpdate(ctx, output, &session, input, strlen((char *) input)) != 1)
    {
        printError(Q_FUNC_INFO, "failed to update decrypt");
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    total = session;
    if (EVP_DecryptFinal_ex(ctx, output + session, &session) != 1)
    {
        printError(Q_FUNC_INFO, "failed to final decrypt");
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    total += session;
    EVP_CIPHER_CTX_free(ctx);
    return total;
}

static int encrypt_aes_256(unsigned char *input, unsigned char *key, unsigned char *iv, unsigned char *output)
{
    int session, total;
    auto ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        printError(Q_FUNC_INFO, "failed to create context");
        return -1;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1)
    {
        printError(Q_FUNC_INFO, "failed to init encrypt");
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_EncryptUpdate(ctx, output, &session, input, strlen((char *) input)) != 1)
    {
        printError(Q_FUNC_INFO, "failed to update encrypt");
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    total = session;
    if (EVP_EncryptFinal_ex(ctx, output + session, &session) != 1)
    {
        printError(Q_FUNC_INFO, "failed to final encrypt");
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    total += session;
    EVP_CIPHER_CTX_free(ctx);
    return total;
}

static QString encrypt(const QString &password, const QByteArray &input, bool& ok)
{
    const auto md5 = QCryptographicHash::hash(password.toLocal8Bit(), QCryptographicHash::Md5).toHex();
    const auto key = bytes2chara(md5);
    const auto iv = bytes2chara(random(16 /* AES IV Length */).toLocal8Bit());
    const auto plain = bytes2chara(input);
    const auto output = new unsigned char[std::max(input.size() * 2, 16)];
    const auto n = encrypt_aes_256((unsigned char*)plain, (unsigned char*)key, (unsigned char*)iv, output);

   const QString result = n != -1 ?
                QString()
                .append(QByteArray::fromRawData(iv, strlen(iv)).toBase64(QByteArray::OmitTrailingEquals))
                .append('.')
                .append(QByteArray::fromRawData((char*)output, n).toBase64(QByteArray::OmitTrailingEquals))
              : QString();

    ok = n != -1;

    delete[] key;
    delete[] iv;
    delete[] plain;
    delete[] output;

    if (!ok)
    {
        return QByteArray();
    }

    return result;
}

static QByteArray decrypt(const QString &password, const QString &input, bool& ok)
{
    if (!input.contains('.'))
    {
        qCritical() << Q_FUNC_INFO << "input not contains '.'";
        ok = false;
        return QByteArray();
    }

    const auto md5 = QCryptographicHash::hash(password.toLocal8Bit(), QCryptographicHash::Md5).toHex();
    const auto key = bytes2chara(md5);
    const auto parts = input.split('.');
    const auto iv = bytes2chara(QByteArray::fromBase64(parts[0].toLocal8Bit(), QByteArray::OmitTrailingEquals));
    const auto encrypted = bytes2chara(QByteArray::fromBase64(parts[1].toLocal8Bit(), QByteArray::OmitTrailingEquals));
    const auto output = new unsigned char[parts[1].size() * 2];
    const auto n = decrypt_aes_256((unsigned char*)encrypted, (unsigned char*)key, (unsigned char*)iv, output);

    const QByteArray result = n != -1 ? QByteArray::fromRawData((char*)output, n) : QByteArray();

    ok = n != -1;

    delete[] key;
    delete[] iv;
    delete[] encrypted;
    delete[] output;

    if (!ok)
    {
        return QByteArray();
    }

    return result;
}

}

std::optional<QString> Crypto::encrypt(const QByteArray &data)
{
    bool ok = false;
    const QString result = ::encrypt(CryptoKey, data, ok);

    if (!ok)
    {
        return std::nullopt;
    }

    return result;
}

std::optional<QByteArray> Crypto::decrypt(const QString &data)
{
    bool ok = false;
    const QByteArray result = ::decrypt(CryptoKey, data, ok);

    if (!ok)
    {
        return std::nullopt;
    }

    return result;
}

bool Crypto::test()
{
    bool ok = true;

    if (!test("Тестовая строка 1")) { ok = false; }
    if (!test("Тестовая строка 1 Тестовая строка 2")) { ok = false; }
    if (!test("Тестовая строка 1 Тестовая строка 2 Тестовая строка 3")) { ok = false; }
    if (!test("Тестовая строка 1 Тестовая строка 2 Тестовая строка 3 Тестовая строка 2 Тестовая строка 3")) { ok = false; }
    if (!test("")) { ok = false; }
    if (!test("\0")) { ok = false; }
    if (!test("Тестовая строка 1 \0 Тестовая строка 2")) { ok = false; }

    return ok;
}

bool Crypto::test(const QByteArray &data)
{
    const std::optional<QString> encrypted = encrypt(data);
    if (!encrypted)
    {
        qCritical() << Q_FUNC_INFO << "failed to encrypt" << data;
        return false;
    }

    const std::optional<QByteArray> decrypted = decrypt(*encrypted);
    if (!decrypted)
    {
        qCritical() << Q_FUNC_INFO << "failed to decrypt" << *encrypted;
        return false;
    }

    if (*decrypted != data)
    {
        qCritical() << Q_FUNC_INFO << "decrypted data does not match, source =" << data << ", decrypted =" << *decrypted;
        return false;
    }

    return true;
}
