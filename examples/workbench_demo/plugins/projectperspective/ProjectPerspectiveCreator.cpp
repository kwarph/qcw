// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ProjectPerspectiveCreator.h"

#include "PluginInfo.h"
#include "ProjectPerspective.h"
#include "QcwThemeIcons.h"

QIcon ProjectPerspectiveCreator::defaultPerspectiveIcon() const
{
    return qcw::themeIcon(qcw::Icon::Perspective);
}

QString ProjectPerspectiveCreator::defaultName() const
{
    return tr("Project");
}

QcwPerspective* ProjectPerspectiveCreator::doCreate(QcwWindow* window)
{
    return new ProjectPerspective { window };
}

QCW_PLUGIN_IMPLEMENT(ProjectPerspectiveCreator, "ProjectPerspective")
