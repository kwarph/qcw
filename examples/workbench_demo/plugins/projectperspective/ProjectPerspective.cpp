// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ProjectPerspective.h"

#include "DemoServices.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwWindow.h"

#include <QSplitter>

ProjectPerspective::ProjectPerspective(QcwWindow* window) :
    QcwPerspective { window }
{
}

void ProjectPerspective::createInitialLayout()
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
    auto* outlineFolder = container->createFolder(centralFolder, qcw::RightDockArea);
    auto* logFolder = container->createFolder(centralFolder, qcw::BottomDockArea);

    // A view the window already has is reused, so the two perspectives show one ProjectView;
    // an unknown type or a failed creator simply leaves that folder empty.
    const auto addView = [this](QcwTabFolder* folder, QLatin1String type)
    {
        if (!folder)
        {
            return;
        }

        auto viewInfo = getOrCreateView(QString(type));
        if (viewInfo.first)
        {
            folder->addTab(viewInfo.first, viewInfo.first->windowTitle(), viewInfo.second);
        }
    };

    addView(leftFolder, demo::ProjectViewType);
    addView(outlineFolder, demo::OutlineViewType);
    addView(logFolder, demo::LogViewType);

    if (auto* splitter = container->parentSplitter(leftFolder))
    {
        splitter->setSizes({ 240, qMax(320, window()->width() - 480), 240 });
    }

    if (auto* splitter = container->parentSplitter(logFolder))
    {
        splitter->setSizes({ qMax(200, window()->height() - 300), 300 });
    }
}
