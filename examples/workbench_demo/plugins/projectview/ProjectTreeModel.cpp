// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ProjectTreeModel.h"

#include "DemoServices.h"
#include "DemoUtils.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFileSystemModel>
#include <QFont>
#include <QMetaObject>

#include <algorithm>

namespace
{
QString pathOverlap(const QString& path, const QString& other)
{
    if (demo::pathIsUnder(path, other) || demo::pathIsUnder(other, path))
    {
        return other;
    }

    return {};
}

QString commonAncestor(const QStringList& roots)
{
    if (roots.isEmpty())
    {
        return QDir::rootPath();
    }

    QString common = roots.first();
    while (common != QDir::rootPath())
    {
        const bool containsAll = std::all_of(roots.cbegin(), roots.cend(), [&common](const QString& root)
        {
            return demo::pathIsUnder(root, common);
        });
        if (containsAll)
        {
            break;
        }

        const QString parent = QFileInfo { common }.absolutePath();
        if (parent == common)
        {
            break;
        }

        common = parent;
    }

    return common;
}
} // namespace

ProjectTreeModel::ProjectTreeModel(QObject* parent) :
    QAbstractProxyModel { parent }, mPruneScheduled { false }
{
}

void ProjectTreeModel::setSourceModel(QAbstractItemModel* newSourceModel)
{
    if (newSourceModel == sourceModel())
    {
        return;
    }

    beginResetModel();

    for (const auto& connection : std::as_const(mSourceConnections))
    {
        disconnect(connection);
    }
    mSourceConnections.clear();
    mSavedProxyIndexes.clear();
    mSavedSourceIndexes.clear();

    QAbstractProxyModel::setSourceModel(newSourceModel);
    connectSourceSignals();
    rebuildSourceRoots();

    endResetModel();
    setWatchRoot(mProjects);
}

void ProjectTreeModel::setProjects(const QStringList& roots)
{
    QStringList accepted;
    for (const QString& rawRoot : roots)
    {
        const QString root = demo::normalizedPath(rawRoot);
        if (root.isEmpty() || !QFileInfo { root }.isDir())
        {
            continue;
        }

        bool overlaps = false;
        for (const QString& existing : std::as_const(accepted))
        {
            if (!pathOverlap(root, existing).isEmpty())
            {
                overlaps = true;
                break;
            }
        }

        if (!overlaps)
        {
            accepted.append(root);
        }
    }

    beginResetModel();
    mProjects = accepted;
    mCurrentProject = mProjects.value(0);
    rebuildSourceRoots();
    endResetModel();

    setWatchRoot(mProjects);

    emit projectsChanged();
    emit currentProjectChanged(mCurrentProject);
}

bool ProjectTreeModel::addProject(const QString& rawRoot)
{
    auto* model = fileModel();
    const QString root = demo::normalizedPath(rawRoot);
    if (!model || root.isEmpty() || mProjects.contains(root) || !QFileInfo { root }.isDir())
    {
        return false;
    }

    // The file model has to watch the widened common ancestor before the row appears,
    // otherwise the project's children fall outside the subtree it sorts.
    QStringList expanded = mProjects;
    expanded.append(root);
    setWatchRoot(expanded);

    const int row = mProjects.size();
    beginInsertRows({}, row, row);
    mProjects.append(root);
    mSourceRoots.append(QPersistentModelIndex { model->index(root) });
    endInsertRows();

    setCurrentProject(root);
    emit projectsChanged();
    return true;
}

void ProjectTreeModel::closeProject(const QString& root)
{
    const int row = mProjects.indexOf(root);
    if (row < 0)
    {
        return;
    }

    const bool wasCurrent = mProjects.at(row) == mCurrentProject;

    beginRemoveRows({}, row, row);
    mProjects.removeAt(row);
    mSourceRoots.removeAt(row);
    if (wasCurrent)
    {
        mCurrentProject.clear();
    }
    endRemoveRows();

    if (wasCurrent)
    {
        const QString next = mProjects.value(0);
        if (next.isEmpty())
        {
            emit currentProjectChanged(QString());
        }
        else
        {
            setCurrentProject(next);
        }
    }

    emit projectsChanged();
    setWatchRoot(mProjects);
}

QStringList ProjectTreeModel::projects() const
{
    return mProjects;
}

bool ProjectTreeModel::containsProject(const QString& root) const
{
    return mProjects.contains(root);
}

QString ProjectTreeModel::currentProject() const
{
    return mCurrentProject;
}

void ProjectTreeModel::setCurrentProject(const QString& root)
{
    if (root == mCurrentProject)
    {
        return;
    }

    const int oldRow = mProjects.indexOf(mCurrentProject);
    const int newRow = mProjects.indexOf(root);
    if (!root.isEmpty() && newRow < 0)
    {
        return;
    }

    mCurrentProject = root;
    const int lastColumn = qMax(0, columnCount() - 1);

    if (oldRow >= 0)
    {
        emit dataChanged(index(oldRow, 0), index(oldRow, lastColumn), { Qt::FontRole });
    }
    if (newRow >= 0)
    {
        emit dataChanged(index(newRow, 0), index(newRow, lastColumn), { Qt::FontRole });
    }

    emit currentProjectChanged(mCurrentProject);
}

QString ProjectTreeModel::projectForPath(const QString& path) const
{
    QString best;
    for (const QString& root : mProjects)
    {
        if (demo::pathIsUnder(path, root) && root.size() > best.size())
        {
            best = root;
        }
    }

    return best;
}

QModelIndex ProjectTreeModel::projectIndex(const QString& root) const
{
    return index(mProjects.indexOf(root), 0);
}

bool ProjectTreeModel::isProjectIndex(const QModelIndex& index) const
{
    return index.isValid() && !index.parent().isValid();
}

QString ProjectTreeModel::filePath(const QModelIndex& index) const
{
    auto* model = fileModel();
    const QModelIndex sourceIndex = mapToSource(index);
    return (model && sourceIndex.isValid()) ? model->filePath(sourceIndex) : QString();
}

QString ProjectTreeModel::fileName(const QModelIndex& index) const
{
    auto* model = fileModel();
    const QModelIndex sourceIndex = mapToSource(index);
    return (model && sourceIndex.isValid()) ? model->fileName(sourceIndex) : QString();
}

QFileInfo ProjectTreeModel::fileInfo(const QModelIndex& index) const
{
    auto* model = fileModel();
    const QModelIndex sourceIndex = mapToSource(index);
    return (model && sourceIndex.isValid()) ? model->fileInfo(sourceIndex) : QFileInfo {};
}

bool ProjectTreeModel::isDir(const QModelIndex& index) const
{
    auto* model = fileModel();
    const QModelIndex sourceIndex = mapToSource(index);
    return model && sourceIndex.isValid() && model->isDir(sourceIndex);
}

void ProjectTreeModel::refresh()
{
    auto* model = fileModel();
    if (!model || mProjects.isEmpty())
    {
        return;
    }

    // setRootPath() marks the previous root's children as unwatched, so hopping through the
    // empty path and back makes the model list the projects' common ancestor again.
    const QString watchRoot = model->rootPath();
    if (watchRoot.isEmpty())
    {
        return;
    }

    model->setRootPath(QString());
    model->setRootPath(watchRoot);
}

QModelIndex ProjectTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
    {
        return {};
    }

    if (!parent.isValid())
    {
        const QPersistentModelIndex root = mSourceRoots.value(row);
        return root.isValid() ? createIndex(row, column, root.internalPointer()) : QModelIndex {};
    }

    const QModelIndex sourceParent = mapToSource(parent);
    if (!sourceParent.isValid() || !sourceModel())
    {
        return {};
    }

    const QModelIndex sourceIndex = sourceModel()->index(row, column, sourceParent);
    if (!sourceIndex.isValid())
    {
        return {};
    }

    return createIndex(sourceIndex.row(), sourceIndex.column(), sourceIndex.internalPointer());
}

QModelIndex ProjectTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid() || !sourceModel())
    {
        return {};
    }

    return mapFromSource(sourceModel()->parent(mapToSource(child)));
}

int ProjectTreeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
    {
        return 0;
    }

    if (!parent.isValid())
    {
        return mProjects.size();
    }

    const QModelIndex sourceParent = mapToSource(parent);
    return (sourceModel() && sourceParent.isValid()) ? sourceModel()->rowCount(sourceParent) : 0;
}

int ProjectTreeModel::columnCount(const QModelIndex&) const
{
    return sourceModel() ? sourceModel()->columnCount() : 0;
}

bool ProjectTreeModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return !mProjects.isEmpty();
    }

    return sourceModel() && QAbstractProxyModel::hasChildren(parent);
}

bool ProjectTreeModel::canFetchMore(const QModelIndex& parent) const
{
    if (!parent.isValid() || !sourceModel())
    {
        return false;
    }

    return QAbstractProxyModel::canFetchMore(parent);
}

void ProjectTreeModel::fetchMore(const QModelIndex& parent)
{
    if (!parent.isValid() || !sourceModel())
    {
        return;
    }

    QAbstractProxyModel::fetchMore(parent);
}

QVariant ProjectTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
    {
        return {};
    }

    if (isProjectIndex(index))
    {
        if (role == Qt::ToolTipRole)
        {
            return mProjects.value(index.row());
        }

        if (role == Qt::FontRole && mProjects.value(index.row()) == mCurrentProject)
        {
            QFont font;
            font.setBold(true);
            return font;
        }
    }

    return QAbstractProxyModel::data(index, role);
}

Qt::ItemFlags ProjectTreeModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags result = QAbstractProxyModel::flags(index);
    if (isProjectIndex(index))
    {
        result &= ~Qt::ItemIsDragEnabled;
    }

    return result;
}

QModelIndex ProjectTreeModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid() || !sourceModel())
    {
        return {};
    }

    // The internal pointer names the file-system node; the row only says where it sat when the
    // index was made, so a source re-sort cannot invalidate the node.
    return createSourceIndex(proxyIndex.row(), proxyIndex.column(), proxyIndex.internalPointer());
}

QModelIndex ProjectTreeModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid() || !sourceModel() || !sourceIndex.internalPointer())
    {
        return {};
    }

    for (int row = 0; row < mSourceRoots.size(); ++row)
    {
        if (mSourceRoots.at(row).internalPointer() == sourceIndex.internalPointer())
        {
            return createIndex(row, sourceIndex.column(), sourceIndex.internalPointer());
        }
    }

    const QModelIndex sourceParent = sourceIndex.parent();
    if (!sourceParent.isValid())
    {
        return {};
    }

    const QModelIndex proxyParent = mapFromSource(sourceParent);
    if (!proxyParent.isValid())
    {
        return {};
    }

    return createIndex(sourceIndex.row(), sourceIndex.column(), sourceIndex.internalPointer());
}

QFileSystemModel* ProjectTreeModel::fileModel() const
{
    return qobject_cast<QFileSystemModel*>(sourceModel());
}

void ProjectTreeModel::connectSourceSignals()
{
    QAbstractItemModel* source = sourceModel();
    if (!source)
    {
        return;
    }

    mSourceConnections = {
        // The source model can be destroyed before this proxy (Qt deletes a QObject's children in
        // creation order, and ProjectView creates the file model first). Drop the persistent
        // indexes while the source's private data still exists, or their destructors would touch
        // a destroyed model.
        connect(source, &QObject::destroyed, this, [this]()
    {
        mSourceRoots.clear();
        mSavedProxyIndexes.clear();
        mSavedSourceIndexes.clear();
    }),
        connect(source, &QAbstractItemModel::dataChanged, this, &ProjectTreeModel::sourceDataChanged),
        connect(source, &QAbstractItemModel::rowsAboutToBeInserted, this,
                &ProjectTreeModel::sourceRowsAboutToBeInserted),
        connect(source, &QAbstractItemModel::rowsInserted, this, &ProjectTreeModel::sourceRowsInserted),
        connect(source, &QAbstractItemModel::rowsAboutToBeRemoved, this, &ProjectTreeModel::sourceRowsAboutToBeRemoved),
        connect(source, &QAbstractItemModel::rowsRemoved, this, &ProjectTreeModel::sourceRowsRemoved),
        connect(source, &QAbstractItemModel::modelAboutToBeReset, this,
                [this]()
    {
        beginResetModel();
    }),
        connect(source, &QAbstractItemModel::modelReset, this,
                [this]()
    {
        rebuildSourceRoots();
        endResetModel();
    }),
        connect(source, &QAbstractItemModel::layoutAboutToBeChanged, this,
                &ProjectTreeModel::sourceLayoutAboutToBeChanged),
        connect(source, &QAbstractItemModel::layoutChanged, this, &ProjectTreeModel::sourceLayoutChanged),
    };
}

void ProjectTreeModel::rebuildSourceRoots()
{
    mSourceRoots.clear();

    auto* model = fileModel();
    if (!model)
    {
        return;
    }

    mSourceRoots.reserve(mProjects.size());
    for (const QString& root : std::as_const(mProjects))
    {
        mSourceRoots.append(QPersistentModelIndex { model->index(root) });
    }
}

void ProjectTreeModel::setWatchRoot(const QStringList& roots)
{
    auto* model = fileModel();
    if (!model || roots.isEmpty())
    {
        return;
    }

    const QString common = commonAncestor(roots);
    if (model->rootPath() != common)
    {
        model->setRootPath(common);
    }
}

void ProjectTreeModel::schedulePrune()
{
    if (mPruneScheduled)
    {
        return;
    }

    mPruneScheduled = true;
    QMetaObject::invokeMethod(this, &ProjectTreeModel::pruneMissingProjects, Qt::QueuedConnection);
}

void ProjectTreeModel::pruneMissingProjects()
{
    mPruneScheduled = false;

    // A project folder that vanished is closed; the file system model cannot keep a node for it.
    const QStringList candidates = mProjects;
    for (const QString& root : candidates)
    {
        if (!QFileInfo { root }.isDir())
        {
            closeProject(root);
        }
    }
}

void ProjectTreeModel::sourceDataChanged(const QModelIndex& topLeft,
                                         const QModelIndex& bottomRight,
                                         const QList<int>& roles)
{
    const QModelIndex proxyTopLeft = mapFromSource(topLeft);
    const QModelIndex proxyBottomRight = mapFromSource(bottomRight);
    if (proxyTopLeft.isValid() && proxyBottomRight.isValid())
    {
        emit dataChanged(proxyTopLeft, proxyBottomRight, roles);
    }
}

void ProjectTreeModel::sourceRowsAboutToBeInserted(const QModelIndex& parent, int first, int last)
{
    const QModelIndex proxyParent = mapFromSource(parent);
    if (proxyParent.isValid())
    {
        beginInsertRows(proxyParent, first, last);
    }
}

void ProjectTreeModel::sourceRowsInserted(const QModelIndex& parent, int, int)
{
    if (mapFromSource(parent).isValid())
    {
        endInsertRows();
    }
}

void ProjectTreeModel::sourceRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last)
{
    const QModelIndex proxyParent = mapFromSource(parent);
    if (proxyParent.isValid())
    {
        beginRemoveRows(proxyParent, first, last);
    }
}

void ProjectTreeModel::sourceRowsRemoved(const QModelIndex& parent, int, int)
{
    if (mapFromSource(parent).isValid())
    {
        endRemoveRows();
    }

    schedulePrune();
}

void ProjectTreeModel::sourceLayoutAboutToBeChanged(const QList<QPersistentModelIndex>& sourceParents,
                                                    QAbstractItemModel::LayoutChangeHint hint)
{
    QList<QPersistentModelIndex> parents;
    for (const QPersistentModelIndex& sourceParent : sourceParents)
    {
        if (!sourceParent.isValid())
        {
            parents.append(QPersistentModelIndex {});
            continue;
        }

        const QModelIndex mapped = mapFromSource(sourceParent);
        if (mapped.isValid())
        {
            parents.append(mapped);
        }
    }

    emit layoutAboutToBeChanged(parents, hint);

    // The source re-sorts its nodes between the two signals and updates the persistent indexes
    // registered in it. Keeping one per proxy index is what lets sourceLayoutChanged() find the
    // node's new row for the filter proxy above.
    mSavedProxyIndexes.clear();
    mSavedSourceIndexes.clear();

    const auto proxyIndexes = persistentIndexList();
    for (const QModelIndex& proxyIndex : proxyIndexes)
    {
        mSavedProxyIndexes.append(proxyIndex);
        mSavedSourceIndexes.append(QPersistentModelIndex { mapToSource(proxyIndex) });
    }
}

void ProjectTreeModel::sourceLayoutChanged(const QList<QPersistentModelIndex>& sourceParents,
                                           QAbstractItemModel::LayoutChangeHint hint)
{
    QModelIndexList from;
    QModelIndexList to;
    from.reserve(mSavedProxyIndexes.size());
    to.reserve(mSavedProxyIndexes.size());

    for (int i = 0; i < mSavedProxyIndexes.size(); ++i)
    {
        from.append(mSavedProxyIndexes.at(i));
        to.append(mapFromSource(mSavedSourceIndexes.at(i)));
    }

    mSavedProxyIndexes.clear();
    mSavedSourceIndexes.clear();
    changePersistentIndexList(from, to);

    QList<QPersistentModelIndex> parents;
    for (const QPersistentModelIndex& sourceParent : sourceParents)
    {
        if (!sourceParent.isValid())
        {
            parents.append(QPersistentModelIndex {});
            continue;
        }

        const QModelIndex mapped = mapFromSource(sourceParent);
        if (mapped.isValid())
        {
            parents.append(mapped);
        }
    }

    emit layoutChanged(parents, hint);
}
