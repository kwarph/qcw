// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTabPaneManager.h"
#include <QWidget>
#include <QGlobalStatic>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QcwTabPaneManager, tabPaneManagerInstance);

QcwTabPaneManager* QcwTabPaneManager::instance()
{
    return tabPaneManagerInstance();
}

void QcwTabPaneManager::addReference(QWidget* tabPane, QcwTabFolder* folder)
{
    if (!tabPane || !folder)
    {
        return;
    }

    auto it = mTabPanes.find(tabPane);
    if (it == mTabPanes.end())
    {
        it = mTabPanes.insert(tabPane, {});

        // A pane can also die without every holder calling removeReference,
        // e.g. destroyed together with its own parent widget. Drop the entry
        // then, so the map never keeps a dangling key whose address a later
        // widget could reuse together with this stale folder set. The
        // connection belongs to the pane, so it dies with it and can never
        // be registered twice for one widget.
        QObject::connect(tabPane, &QObject::destroyed, tabPane, [this, tabPane]()
        {
            mTabPanes.remove(tabPane);
        });
    }
    it.value().insert(folder);

#ifdef QCW_DEBUG
    qDebug() << "add tabPane:" << tabPane << "folder:" << folder;
#endif
}

void QcwTabPaneManager::removeReference(QWidget* tabPane, QcwTabFolder* folder)
{
    if (!tabPane || !folder)
    {
        return;
    }

    auto it = mTabPanes.find(tabPane);
    if (it != mTabPanes.end())
    {
        it.value().remove(folder);
        if (it.value().empty())
        {
#ifdef QCW_DEBUG
            qDebug() << "remove tabPane:" << tabPane << "folder:" << folder;
#endif
            mTabPanes.erase(it);
            // Nobody shows this pane anymore and the folders have detached
            // it, so the manager is the one that has to clean it up.
            tabPane->deleteLater();
        }
    }
}

QSet<QcwTabFolder*> QcwTabPaneManager::getReferences(QWidget* tabPane) const
{
    auto it = mTabPanes.constFind(tabPane);
    return it != mTabPanes.cend() ? it.value() : QSet<QcwTabFolder*> {};
}

int QcwTabPaneManager::nextPaneId()
{
    return mPaneId.fetch_add(1, std::memory_order_relaxed);
}

void QcwTabPaneManager::resetStartingPaneId(int id)
{
    // Raised with compare-and-swap rather than a plain store: a concurrent
    // nextPaneId() that lands between the load and the store would otherwise
    // be undone and its id handed out a second time.
    auto current = mPaneId.load(std::memory_order_relaxed);
    while (id > current && !mPaneId.compare_exchange_weak(current, id, std::memory_order_relaxed))
    {
    }
}

void QcwTabPaneManager::clear(bool deletePanes)
{
    if (deletePanes)
    {
        auto panes = mTabPanes.keys();
        for (auto pane : std::as_const(panes))
        {
            pane->deleteLater();
        }
    }

    mTabPanes.clear();
    mPaneId.store(0, std::memory_order_relaxed);
}

QT_END_NAMESPACE
