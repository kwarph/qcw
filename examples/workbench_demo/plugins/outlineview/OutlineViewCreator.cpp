// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "OutlineViewCreator.h"

#include "OutlineView.h"
#include "PluginInfo.h"
#include "QcwThemeIcons.h"

QString OutlineViewCreator::defaultWindowTitle() const
{
    return tr("Outline");
}

QIcon OutlineViewCreator::defaultViewIcon() const
{
    return qcw::themeIcon(qcw::Icon::View);
}

QcwView* OutlineViewCreator::doCreate(QWidget* parent, const QList<QVariant>& args)
{
    Q_UNUSED(args);
    return new OutlineView { parent };
}

QCW_PLUGIN_IMPLEMENT(OutlineViewCreator, "OutlineView")
