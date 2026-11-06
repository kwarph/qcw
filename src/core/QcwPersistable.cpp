// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwPersistable.h"
#include "QcwSerializeUtils.h"

QT_BEGIN_NAMESPACE

QcwPersistable::~QcwPersistable()
{
}

void QcwPersistable::saveExtraState(QVariantMap& state) const
{
    Q_UNUSED(state)
}

void QcwPersistable::restoreExtraState(const QVariantMap& state)
{
    Q_UNUSED(state)
}

QByteArray QcwPersistable::extraStateToHex() const
{
    QVariantMap state;
    saveExtraState(state);

    return QcwSerializeUtils::serialize(state).toHex(' ');
}

QT_END_NAMESPACE
