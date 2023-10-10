#pragma once

#include <QObject>

class BackendManager;

class Feature : public QObject
{
public:
    explicit Feature(BackendManager& backend, const QString& name, QObject *parent = nullptr);
    virtual ~Feature(){}

protected:
    void setAsUsed();

private:
    BackendManager& backend;
    const QString name;
    bool settedAsUsed = false;
};
