// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QAbstractProxyModel>
#include <QFileInfo>
#include <QList>
#include <QPersistentModelIndex>
#include <QString>
#include <QStringList>

class QFileSystemModel;

/**
 * @brief Lists several project folders as the top-level rows of one file tree.
 *
 * @details The model wraps a QFileSystemModel and shows one row per open project; each project
 * row expands into that folder's real children, unchanged. A project is identified by its
 * canonical path, so the rows survive the source model's sorts and resets; the source's
 * rootPath is kept at the projects' common ancestor because QFileSystemModel only sorts and
 * watches the subtree below it.
 *
 * Exactly one project is current at a time. The model marks its row bold and reports the
 * current project whenever it changes; actions scoped to a project (creating files, Save All,
 * closing) read it from here. The list and the current project are remembered by the view in
 * QSettings, not by this model.
 */
class ProjectTreeModel : public QAbstractProxyModel
{
    Q_OBJECT
public:
    explicit ProjectTreeModel(QObject* parent = nullptr);

    void setSourceModel(QAbstractItemModel* sourceModel) override;

    /// Replaces the open projects. Paths that are not folders, duplicates, and projects that
    /// overlap an earlier one are dropped. Makes the first surviving project current.
    void setProjects(const QStringList& roots);

    /// Adds one project and makes it current. Returns false for a path that is not a folder
    /// or is already open.
    bool addProject(const QString& root);

    /// Removes a project from the tree. Files and open editors are left alone.
    void closeProject(const QString& root);

    QStringList projects() const;
    bool containsProject(const QString& root) const;

    QString currentProject() const;
    void setCurrentProject(const QString& root);

    /// The open project that is @p path itself or the closest project above it, or empty.
    QString projectForPath(const QString& path) const;

    /// The top-level model index of @p root, or an invalid index.
    QModelIndex projectIndex(const QString& root) const;
    bool isProjectIndex(const QModelIndex& index) const;

    QString filePath(const QModelIndex& index) const;
    QString fileName(const QModelIndex& index) const;
    QFileInfo fileInfo(const QModelIndex& index) const;
    bool isDir(const QModelIndex& index) const;

    /// Makes the file model re-list its root folder, for changes the file watcher missed.
    void refresh();

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    bool hasChildren(const QModelIndex& parent = {}) const override;
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

signals:
    void projectsChanged();
    void currentProjectChanged(const QString& root);

private:
    QFileSystemModel* fileModel() const;

    void connectSourceSignals();
    void rebuildSourceRoots();
    void setWatchRoot(const QStringList& roots);
    void schedulePrune();

    void sourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles);
    void sourceRowsAboutToBeInserted(const QModelIndex& parent, int first, int last);
    void sourceRowsInserted(const QModelIndex& parent, int first, int last);
    void sourceRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void sourceRowsRemoved(const QModelIndex& parent, int first, int last);
    void sourceLayoutAboutToBeChanged(const QList<QPersistentModelIndex>& parents,
                                      QAbstractItemModel::LayoutChangeHint hint);
    void sourceLayoutChanged(const QList<QPersistentModelIndex>& parents, QAbstractItemModel::LayoutChangeHint hint);
    void pruneMissingProjects();

    QStringList mProjects;
    QList<QPersistentModelIndex> mSourceRoots;
    QString mCurrentProject;
    QList<QMetaObject::Connection> mSourceConnections;

    // Indexes captured while the source announces a layout change; see sourceLayoutAboutToBeChanged().
    QList<QModelIndex> mSavedProxyIndexes;
    QList<QPersistentModelIndex> mSavedSourceIndexes;

    bool mPruneScheduled;
};
