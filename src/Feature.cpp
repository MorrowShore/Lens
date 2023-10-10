#include "Feature.h"
#include "BackendManager.h"

Feature::Feature(BackendManager& backend_, const QString& name_, QObject *parent)
    : QObject(parent)
    , backend(backend_)
    , name(name_)
{

}

void Feature::setAsUsed()
{
    if (settedAsUsed)
    {
        return;
    }

    backend.addUsedFeature(name);

    settedAsUsed = true;
}
