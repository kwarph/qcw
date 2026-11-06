// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwViewCreator.h"
#include "QcwView.h"
#include "QcwTabPaneManager.h"

QT_BEGIN_NAMESPACE

bool QcwViewCreator::singleInstancePerWindow() const
{
    return false;
}

QcwView* QcwViewCreator::create(QWidget* parent, bool assignId, const QList<QVariant>& args)
{
    auto view = doCreate(parent, args);
    if (view)
    {
        view->setType(type());
        view->setCreatingArguments(args);
        // The baseline a language change re-resolves the tab's title from. Whichever name the
        // caller gives the tab, this is what the creator asked for, so whoever made the view —
        // a perspective, the New View menu, another plugin — has it recorded.
        view->setDefaultTitle(defaultWindowTitle());
        if (assignId)
        {
            view->setId(tabPaneManager->nextPaneId());
        }
    }
    return view;
}

QT_END_NAMESPACE
