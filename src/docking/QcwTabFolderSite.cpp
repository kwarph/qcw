// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTabFolderSite.h"
#include "QcwTabFolder.h"
#include <QVBoxLayout>

QT_BEGIN_NAMESPACE

static void setupFolder(QcwTabFolder* folder)
{
    QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
    sizePolicy.setHeightForWidth(folder->sizePolicy().hasHeightForWidth());

    folder->setSizePolicy(sizePolicy);
}

// -----------------------------------------------------------------------------

QcwTabFolderSite::QcwTabFolderSite(QcwTabFolder* folder, QWidget* parent) :
    QFrame { parent }, mFolder { folder }
{
    QVBoxLayout* vLayout = new QVBoxLayout;
    vLayout->setContentsMargins(0, 0, 0, 0);

    if (mFolder)
    {
        setupFolder(mFolder);
        vLayout->addWidget(mFolder);
    }

    setLayout(vLayout);
}

QcwTabFolder* QcwTabFolderSite::setFolder(QcwTabFolder* newFolder)
{
    auto old = mFolder;
    mFolder = newFolder;

    auto vLayout = qobject_cast<QVBoxLayout*>(layout());
    while (auto item = vLayout->takeAt(0))
    {
        delete item;
    }

    // A widget that leaves the layout stays a child of this site and keeps its last
    // geometry, so it would be painted over whatever takes the slot next. Hand it to
    // the caller instead, unless it is the folder being installed again.
    if (old && old != newFolder && old->parentWidget() == this)
    {
        old->setParent(nullptr);
    }

    if (mFolder)
    {
        setupFolder(mFolder);
        vLayout->addWidget(mFolder);

        // Reparenting hides a widget, and the folder detached above may come back in
        // the same call, so the visibility the layout alone cannot restore is set here.
        mFolder->show();
    }

    return old;
}

QT_END_NAMESPACE
