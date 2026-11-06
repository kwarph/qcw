// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QPersistentModelIndex>
#include <QStringList>
#include <QTreeView>

#include <functional>

/**
 * @brief A QTreeView that reports the rows it just moved by a drag and drop.
 *
 * @details QFileSystemModel performs a drop itself (QFile::rename for an internal move) and
 * only lets the file watcher report the result, so nothing says which paths were moved where.
 * This view reads the drop target and the URLs before delegating, then hands both to
 * dropHandler() when the model accepted a move.
 */
class ProjectTreeView : public QTreeView
{
public:
    explicit ProjectTreeView(QWidget* parent = nullptr);

    /// Called after an accepted move with the drop target and the moved local paths.
    std::function<void(const QPersistentModelIndex&, const QStringList&)> dropHandler;

protected:
    void dropEvent(QDropEvent* event) override;
};
