// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QWidget>

QT_BEGIN_NAMESPACE

class QLabel;
class QLineEdit;
class QSplitter;
class QTreeView;

class QcwNavigationContentWidgetPrivate;

/**
 * @class QcwNavigationContentWidget
 * @brief Two-pane frame: a filterable tree on the left and a content page on the right.
 *
 * @details The left pane holds the filter line and the tree; the right pane shows a breadcrumb
 * label above a separator and the widget installed with setContentWidget(). This class builds
 * only that chrome: it creates no model, does not connect the filter and does not fill the
 * breadcrumb. A subclass does those through the protected accessors, which is how
 * QcwPreferencesWidget reuses the layout for its page tree.
 *
 * A subclass normally calls setContentWidget() once, after creating its detail widget, and
 * sets the splitter's initial sizes in its first showEvent(). The content widget is reparented
 * into the right pane and destroyed with this widget, so do not delete it separately.
 *
 * @note The filter line has a default placeholder but no model wiring. A subclass that wants
 * its own translated labels sets them on filterEdit() and updates them on
 * QEvent::LanguageChange.
 *
 * @see QcwNavigationContentDialog, QcwPreferencesWidget
 */
class QCW_EXPORT QcwNavigationContentWidget : public QWidget
{
    Q_OBJECT
public:
    /// Builds the two panes; the tree and the content area start empty.
    explicit QcwNavigationContentWidget(QWidget* parent = nullptr);
    ~QcwNavigationContentWidget() override;

    /// The horizontal splitter between the tree and content panes.
    QSplitter* splitter() const;

    /// The filter line above the tree; the base class does not connect it to a model.
    QLineEdit* filterEdit() const;

    /// The tree view of the navigation pane; it has no model until a subclass sets one.
    QTreeView* treeView() const;

    /// The breadcrumb label above the content; it accepts rich text.
    QLabel* navigationLabel() const;

    /**
     * @brief Installs @p contentWidget as the right-hand detail widget.
     * @param contentWidget Widget to show under the breadcrumb; nullptr is ignored.
     *
     * @details Only the first call has an effect; later calls are ignored. The widget is
     * reparented into the right pane and gets an expanding size policy. Qt destroys it with
     * this widget, so the caller must not delete it while the frame is alive.
     */
    void setContentWidget(QWidget* contentWidget);

    /// The widget installed by setContentWidget(), or nullptr before that call.
    QWidget* contentWidget() const;

private:
    QcwNavigationContentWidgetPrivate* impl;
};

QT_END_NAMESPACE
