// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPlugin.h"
#include <QList>
#include <QIcon>
#include <QVariant>

QT_BEGIN_NAMESPACE

class QcwView;

/**
 * @class QcwViewCreator
 * @brief The interface a plugin implements to offer a type of view.
 *
 * @details One creator per view type. QcwWindow lists the creators registered under Category to
 * fill the "new view" menu, and looks a creator up by type() when it restores the views a session
 * saved — a plugin that changes or drops its type() strands every view saved under the old one.
 *
 * @par The template method
 * create() is the entry point and is not meant to be overridden: it calls the subclass's doCreate()
 * and stamps the result with this creator's identity — type(), the arguments it was handed and, on
 * request, a fresh tab pane id. A plugin therefore only implements doCreate().
 *
 * @see QcwView, QcwPlugin, QcwTabPaneManager
 */
class QCW_EXPORT QcwViewCreator : public QcwPlugin
{
public:
    /// The category these plugins are registered under, used to list the available ones.
    static constexpr QLatin1String Category { "View" };

    /// @brief The category this creator's plugins belong to.
    QString category() const override
    {
        return Category;
    }

    /**
     * @brief Build a view of this type.
     * @param parent The widget the view is created in, or @c nullptr for a detached one; ownership
     *               follows the usual QWidget rule.
     * @param assignId Give the view a fresh tab pane id from QcwTabPaneManager. Pass @c false when
     *                 the id comes from saved state, because the id is write-once and the restore
     *                 has to set it itself — see QcwView.
     * @param args Arguments for doCreate(); they are remembered on the view and saved with it, so
     *             restoring the session can repeat the call.
     * @return The new view, or @c nullptr when doCreate() produced none.
     *
     * @details The title and the icon are deliberately not stamped here: a window may hold several
     * views of one type and gives each its own name, so the caller decides — see
     * defaultWindowTitle() and defaultViewIcon() for what it starts from. What is stamped is
     * QcwView::defaultTitle(), the baseline a language change compares the tab's text against; a
     * caller that names a view something of its own clears it again, because then there is no
     * default left to re-resolve to.
     */
    QcwView* create(QWidget* parent, bool assignId, const QList<QVariant>& args = {});

    /// The title a view of this type starts with, before the window makes it unique.
    virtual QString defaultWindowTitle() const = 0;

    /// The icon a view of this type starts with, for its tab and its detached window.
    virtual QIcon defaultViewIcon() const = 0;

    /**
     * @brief Whether a window holds at most one view made by this creator.
     * @return False by default; override to make the view a per-window singleton.
     *
     * @details A singleton type is identified by its type alone: QcwPerspective::getOrCreateView()
     * ignores the requested title and reuses the window's existing view even when it belongs to
     * the perspective asking. The window's New View menu does not offer a second one, and
     * QcwWindow::createDetachedView() refuses to make one. Restoring a saved perspective reuses
     * the window's view for a tab of the type, even when the saved title differs. The view itself
     * can still be shown by several perspectives, which is what the reuse is for; a second
     * *instance* is what this forbids.
     *
     * @note The limit is per window, not per process: each window may have its own view of the
     *       type, because the state a singleton carries — a project tree, for example — belongs
     *       to one window.
     */
    virtual bool singleInstancePerWindow() const;

protected:
    /// Makes the view object itself; it has to be a child of @p parent.
    virtual QcwView* doCreate(QWidget* parent, const QList<QVariant>& args) = 0;
};

#define QcwViewCreator_iid "org.qcw.QcwViewCreator"

Q_DECLARE_INTERFACE(QcwViewCreator, QcwViewCreator_iid)

QT_END_NAMESPACE
