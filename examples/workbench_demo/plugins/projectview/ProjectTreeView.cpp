// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ProjectTreeView.h"

#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

ProjectTreeView::ProjectTreeView(QWidget* parent) :
    QTreeView { parent }
{
    setFrameShape(QFrame::NoFrame);
}

void ProjectTreeView::dropEvent(QDropEvent* event)
{
    const QPersistentModelIndex target { indexAt(event->position().toPoint()) };

    QStringList paths;
    if (event->mimeData()->hasUrls())
    {
        for (const auto& url : event->mimeData()->urls())
        {
            if (url.isLocalFile())
            {
                paths.append(url.toLocalFile());
            }
        }
    }

    QTreeView::dropEvent(event);

    if (dropHandler && !paths.isEmpty() && event->isAccepted() && event->dropAction() == Qt::MoveAction)
    {
        dropHandler(target, paths);
    }
}
