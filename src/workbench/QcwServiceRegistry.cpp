// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwServiceRegistry.h"
#include <QGlobalStatic>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QcwServiceRegistry, serviceRegistryInstance);

QcwServiceRegistry* QcwServiceRegistry::instance()
{
    return serviceRegistryInstance;
}

bool QcwServiceRegistry::registerObject(const QString& interfaceId, QObject* service)
{
    if (interfaceId.isEmpty() || service == nullptr)
    {
        return false;
    }

    // Replacing a registered service would leave every pointer already handed out pointing at
    // the old object, so the first registration wins.
    if (mServices.contains(interfaceId))
    {
        return false;
    }

    // The registry owns the service from here on; detaching it keeps any parent from deleting
    // what is meant to live for the rest of the run.
    service->setParent(nullptr);
    mServices.insert(interfaceId, service);
    return true;
}

QObject* QcwServiceRegistry::serviceObject(const QString& interfaceId) const
{
    return mServices.value(interfaceId, nullptr);
}

bool QcwServiceRegistry::contains(const QString& interfaceId) const
{
    return mServices.contains(interfaceId);
}

QStringList QcwServiceRegistry::interfaceIds() const
{
    return mServices.keys();
}

int QcwServiceRegistry::size() const
{
    return mServices.size();
}

bool QcwServiceRegistry::empty() const
{
    return mServices.isEmpty();
}

QT_END_NAMESPACE
