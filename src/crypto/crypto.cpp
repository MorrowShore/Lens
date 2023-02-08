#include "crypto.h"
#include "secrets.h"
#include "obfuscator.h"
#include "aes.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QStringList>
#include <QDebug>

// Encryption parts:
// encrypted payload
// padding the payload with random bytes so that the payload size becomes a multiple of 16
// 14 bytes - random bytes
// 1 byte - number of bytes to trim to leave only the payload
// 1 byte - checksum

namespace
{

static const AESKeyLength AESKeyLengthType = AESKeyLength::AES_256;

static std::vector<unsigned char> validateKey(const char* rawKey)
{
    static const unsigned char FillChar = 'A';
    size_t length = 0;
    switch (AESKeyLengthType)
    {
    case AESKeyLength::AES_128:
        length = 128 / 8;
        break;
    case AESKeyLength::AES_192:
        length = 192 / 8;
        break;
    case AESKeyLength::AES_256:
        length = 256 / 8;
        break;
    default:
        qCritical() << Q_FUNC_INFO << "invalid key length type";
        abort();
        return std::vector<unsigned char>();
    }

    const size_t rawKeySize = strlen(rawKey);
    if (rawKeySize != length)
    {
        qWarning() << Q_FUNC_INFO << "key length =" << rawKeySize << ", expected =" << length << ", the key will be truncated or padded";
    }

    std::vector<unsigned char> result;
    result.reserve(length);

    for (size_t i = 0; i < length; ++i)
    {
        if (result.size() < rawKeySize)
        {
            result.push_back(rawKey[i]);
        }
        else
        {
            result.push_back(FillChar);
        }
    }

    return result;
}

static const std::vector<unsigned char> CryptoKey = validateKey(OBFUSCATE(CRYPTO_KEY_32_BYTES));

static unsigned char calcChecksumWithoutLastByte(const std::vector<unsigned char>& data)
{
    if (data.empty())
    {
        return 0;
    }

    unsigned char checksum = 0;

    for (size_t i = 0; i < data.size() - 1; ++i)
    {
        checksum += data[i];
    }

    return checksum;
}

}

std::optional<QByteArray> Crypto::encrypt(const QByteArray &rawData)
{
    std::vector<unsigned char> data;
    data.reserve(rawData.length() + 32);
    for (int i = 0; i < rawData.length(); ++i)
    {
        data.push_back((unsigned char)rawData[i]);
    }

    const int needAddSymbols = (16 - (data.size() % 16)) + 16;

    for (int i = 0; i < needAddSymbols; ++i)
    {
        data.push_back((unsigned char)(33 + QRandomGenerator::global()->generate() % 143));
    }

    if (data.size() >= 2)
    {
        data[data.size() - 2] = (unsigned char)needAddSymbols;
        data[data.size() - 1] = calcChecksumWithoutLastByte(data);
    }

    AES aes(AESKeyLengthType);

    try
    {
        const std::vector<unsigned char> rawResult = aes.EncryptECB(data, CryptoKey);
        QByteArray result;
        result.reserve(rawResult.size());

        for (const unsigned char& c : rawResult)
        {
            result.append((char)c);
        }

        return result;
    }
    catch (const std::length_error& error)
    {
        qWarning() << Q_FUNC_INFO << "failed to encrypt, what =" << error.what();
    }
    catch (...)
    {
        qWarning() << Q_FUNC_INFO << "failed to encrypt";

    }

    return std::nullopt;
}

std::optional<QByteArray> Crypto::decrypt(const QByteArray &rawData)
{
    std::vector<unsigned char> data;
    data.reserve(rawData.length());
    for (int i = 0; i < rawData.length(); ++i)
    {
        data.push_back((unsigned char)rawData[i]);
    }

    AES aes(AESKeyLengthType);

    try
    {
        const std::vector<unsigned char> rawResult = aes.DecryptECB(data, CryptoKey);
        if (rawResult.size() < 2)
        {
            qWarning() << Q_FUNC_INFO << "failed to decrypt, encryption size less than 2";
            return std::nullopt;
        }

        const unsigned char addedSymbols = rawResult[rawResult.size() - 2];
        const unsigned char checksum = rawResult[rawResult.size() - 1];

        if (checksum != calcChecksumWithoutLastByte(rawResult))
        {
            qWarning() << Q_FUNC_INFO << "failed to decrypt, wrong checksum";
            return std::nullopt;
        }

        const int resultLength = rawResult.size() - addedSymbols;

        if (resultLength < 0)
        {
            qWarning() << Q_FUNC_INFO << "failed to decrypt, added symbols less 0";
            return std::nullopt;
        }

        QByteArray result;
        result.reserve(resultLength);

        for (int i = 0; i < resultLength; ++i)
        {
            result.append((char)rawResult[i]);
        }

        return result;
    }
    catch (...)
    {
        qWarning() << Q_FUNC_INFO << "failed to decrypt";
    }

    return std::nullopt;
}

bool Crypto::test()
{
    bool ok = true;

    if (!test("Test")) { ok = false; }
    if (!test("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")) { ok = false; }
    if (!test("Test string")) { ok = false; }
    if (!test("Test string ")) { ok = false; }
    if (!test("Test string Test string Test string Test string Test string Test string Test string Test string Test string Test string Test string")) { ok = false; }
    if (!test("")) { ok = false; }
    if (!test("\0")) { ok = false; }
    if (!test("Test string 1 \0 Test string 2")) { ok = false; }
    if (!test("Test string 1 \0 Test string 2")) { ok = false; }
    if (!test("\t\n\r\\\u3829\u9482\u2766\u1F440")) { ok = false; }

    return ok;
}

bool Crypto::test(const QByteArray &data)
{
    const std::optional<QByteArray> encrypted = encrypt(data);
    if (!encrypted)
    {
        qCritical() << Q_FUNC_INFO << "failed to encrypt" << QString::fromUtf8(data);
        return false;
    }

    const std::optional<QByteArray> decrypted = decrypt(*encrypted);
    if (!decrypted)
    {
        qCritical() << Q_FUNC_INFO << "failed to decrypt" << *encrypted << ", source =" << QString::fromUtf8(data);
        return false;
    }

    if (*decrypted != data)
    {
        qCritical() << Q_FUNC_INFO << "decrypted data does not match, source =" << QString::fromUtf8(data) << ", decrypted =" << QString::fromUtf8(*decrypted);
        return false;
    }

    return true;
}
