// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwPerspectiveCreator.h"
#include "QcwPerspective.h"

QT_BEGIN_NAMESPACE

QcwPerspective* QcwPerspectiveCreator::create(QcwWindow* window)
{
    auto perspective = doCreate(window);
    if (perspective)
    {
        perspective->setProperty("PerspectiveType", type());
        perspective->setName(defaultName());
        perspective->setIcon(defaultPerspectiveIcon());
        perspective->setVisibleActionSetIds(defaultActionSetIds());
    }

    return perspective;
}

QT_END_NAMESPACE
