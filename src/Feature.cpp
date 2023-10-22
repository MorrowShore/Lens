#include "Feature.h"
#include "Backend/BackendManager.h"

Feature::Feature(BackendManager& backend_, const QString& name_, QObject *parent)
    : QObject(parent)
    , backend(backend_)
    , name(name_)
{

}

void Feature::setAsUsed() const
{
    Feature* this_ = const_cast<Feature*>(this);

    if (settedAsUsed)
    {
        return;
    }

    this_->backend.addUsedFeature(name);

    this_->settedAsUsed = true;
}
