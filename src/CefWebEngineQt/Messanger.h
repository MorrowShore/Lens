#pragma once

#include <QMap>
#include <QObject>
#include <QProcess>

namespace cweqt
{

class Messanger : public QObject
{
    Q_OBJECT
public:
    struct Message
    {
        Message(const QString& type_,
                const QMap<QString, QString>& parameters_ = QMap<QString, QString>(),
                const QByteArray& data_ = QByteArray())
            : type(type_)
            , parameters(parameters_)
            , data(data_)
        {
        }

        const QString& getType() const { return type; }
        const QMap<QString, QString>& getParameters() const { return parameters; }
        const QByteArray& getData() const { return data; }

    private:
        friend class Messanger;

        Message(){}

        QString type;
        QMap<QString, QString> parameters;
        QByteArray data;

        enum class Part { Title, Parameters, Data };
        int64_t id = 0;
        int64_t size = 0;
        Part part = Part::Title;
    };

    explicit Messanger(QObject *parent = nullptr);

    void send(const Message& message, QProcess* process);
    void parseLine(const QByteArray& line);

signals:
    void messageReceived(const Message& message);

private:
    int64_t prevMessageId = 0;
    Message message;
};

}
