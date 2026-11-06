// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "NotesViewCreator.h"

#include "DemoNotes.h"
#include "NotesView.h"
#include "PluginInfo.h"
#include "QcwThemeIcons.h"

QString NotesViewCreator::defaultWindowTitle() const
{
    return tr("Notes");
}

QIcon NotesViewCreator::defaultViewIcon() const
{
    return qcw::themeIcon(qcw::Icon::View);
}

QcwView* NotesViewCreator::doCreate(QWidget* parent, const QList<QVariant>& args)
{
    Q_UNUSED(args);
    return new NotesView { parent };
}

QCW_PLUGIN_IMPLEMENT(NotesViewCreator, "NotesView")
