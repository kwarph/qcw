// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "FocusPerspectiveCreator.h"

#include "FocusPerspective.h"
#include "PluginInfo.h"
#include "QcwThemeIcons.h"

QIcon FocusPerspectiveCreator::defaultPerspectiveIcon() const
{
    return qcw::themeIcon(qcw::Icon::Perspective);
}

QString FocusPerspectiveCreator::defaultName() const
{
    return tr("Focus");
}

QcwPerspective* FocusPerspectiveCreator::doCreate(QcwWindow* window)
{
    return new FocusPerspective { window };
}

QCW_PLUGIN_IMPLEMENT(FocusPerspectiveCreator, "FocusPerspective")
