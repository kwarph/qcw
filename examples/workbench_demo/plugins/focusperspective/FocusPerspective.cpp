// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "FocusPerspective.h"

#include "DemoServices.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwWindow.h"

#include <QSplitter>

FocusPerspective::FocusPerspective(QcwWindow* window) :
    QcwPerspective { window }
{
}

void FocusPerspective::createInitialLayout()
{
    auto* container = folderContainer();
    if (!container)
    {
        return;
    }

    QcwTabFolder* centralFolder = window()->sharedFolder();
    if (centralFolder)
    {
        container->addSharedFolder(centralFolder, nullptr, qcw::RightDockArea);
    }
    else
    {
        centralFolder = container->createFolder(nullptr, qcw::LeftDockArea);
    }

    container->setCentralFolder(centralFolder);

    auto* leftFolder = container->createFolder(centralFolder, qcw::LeftDockArea);
    if (!leftFolder)
    {
        return;
    }

    auto viewInfo = getOrCreateView(demo::ProjectViewType);
    if (viewInfo.first)
    {
        leftFolder->addTab(viewInfo.first, viewInfo.first->windowTitle(), viewInfo.second);
    }

    if (auto* splitter = container->parentSplitter(leftFolder))
    {
        splitter->setSizes({ 240, qMax(320, window()->width() - 240) });
    }
}
