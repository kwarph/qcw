// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"

#include <QString>

QT_BEGIN_NAMESPACE

/**
 * @struct QcwActionSet
 * @brief A logical group of contributions whose visibility switches with the perspective.
 *
 * @details A contribution names its action set through QcwContribution::actionSetId; a
 * perspective lists the action sets visible while it is active through
 * QcwPerspective::visibleActionSetIds(). An empty perspective list means "all visible".
 */
struct QCW_EXPORT QcwActionSet
{
    /// Stable id; see the naming convention in the design document.
    QString id;

    /// The label shown in the preferences or action set dialog.
    QString label;
};

QT_END_NAMESPACE
