// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwSessionData.h"
#include <QGlobalStatic>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QcwSessionData, sessionDataInstance);

QcwSessionData* QcwSessionData::instance()
{
    return sessionDataInstance();
}

QT_END_NAMESPACE
