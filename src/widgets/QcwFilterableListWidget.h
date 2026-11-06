// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QFrame>
#include <QIcon>
#include <QString>

QT_BEGIN_NAMESPACE

class QFont;
class QModelIndex;
class QEvent;
class QShowEvent;
class QStandardItem;
class QcwFilterableListWidgetPrivate;

/**
 * @class QcwFilterableListWidget
 * @brief A read-only list of icon/text items with a built-in text filter.
 *
 * @details The widget combines a QLineEdit (the filter bar) and a QListView sharing one
 * QStandardItemModel: whatever is typed into the bar hides the rows that do not contain
 * it, case-insensitively. The typed text is matched as a fixed string, so regular
 * expression metacharacters are searched for literally instead of being interpreted.
 *
 * It is the popup behind a "list all tabs" button: the user types a few characters, then
 * commits the choice by clicking an item or by pressing Return.
 *
 * @par Structure
 * @li filterEdit — the QLineEdit holding the filter text, hidden by setFilterBarVisible().
 * @li separator  — the horizontal line under (or above) the filter bar.
 * @li view       — the QListView, showing a filtered view of the model and using
 *                  QcwFixedRowHeightItemDelegate so all rows are the same height.
 * Those three are also the QSS object names of the widgets.
 *
 * @par Owning items
 * addItem() returns the item it created so the caller can attach data to it
 * (QStandardItem::setData()), but the internal model keeps ownership: the caller must not
 * delete the item. The itemSelected signal reports that same pointer.
 *
 * @see QcwFixedRowHeightItemDelegate
 */
class QCW_EXPORT QcwFilterableListWidget : public QFrame
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the widget.
     * @param filterBarOnTop True to stack the filter bar above the list, false to put it
     *                       below. The list always takes the remaining space.
     * @param parent The parent widget.
     */
    explicit QcwFilterableListWidget(bool filterBarOnTop, QWidget* parent = nullptr);

    /** @brief Destructs the widget, releasing the model and its items. */
    ~QcwFilterableListWidget() override;

    /**
     * @brief Appends an item to the list.
     * @param text The label shown in the row, and the text the filter matches against.
     * @param icon The row icon, sized by QcwFixedRowHeightItemDelegate::IconSize.
     * @param font The row font, stored through Qt::FontRole. Pass the view font to leave a
     *             row looking normal and a bolder font to single it out.
     * @return The new item, owned by the internal model.
     */
    QStandardItem* addItem(const QString& text, const QIcon& icon, const QFont& font);

    /**
     * @brief Shows or hides the filter bar and its separator.
     * @param visible False turns the widget into a plain list: the rows keep their size and
     *                sizeHint() stops counting the bar.
     * @details Hiding the bar also stops it from taking focus when the widget is shown.
     */
    void setFilterBarVisible(bool visible);

    /**
     * @brief Estimates the size needed to show the whole list.
     * @details The width fits the widest item plus room for the vertical scroll bar; the
     * height fits every row, capped so a long list scrolls instead of growing without
     * bounds, plus the filter bar when it is visible.
     * @return The suggested size, or a square placeholder while the list is empty.
     */
    QSize sizeHint() const override;

signals:
    /**
     * @brief Emitted when the user commits an item.
     * @param item The committed item, owned by the internal model.
     * @details A commit is a click on a row, Return while the list has focus, or Return
     *          while the filter bar has focus — the latter also resets the filter. A
     *          double click fires both QAbstractItemView::clicked() and activated() for the
     *          same row, but the item is reported only once; committing the same item twice
     *          in a row takes a new filter keystroke in between to be reported again.
     * @note Nothing is emitted when the filter matches no item.
     */
    void itemSelected(QStandardItem* item);

protected:
    /**
     * @brief Moves the keyboard focus to the filter bar whenever the widget is shown.
     * @details Skipped when the bar is hidden, so focus stays where the caller put it.
     */
    void showEvent(QShowEvent* event) override;

    /**
     * @brief Re-applies the filter bar's placeholder text in the new language.
     * @details The placeholder is a string captured at construction, so a language change
     *          has to write it again; Qt delivers QEvent::LanguageChange to this widget
     *          because it sits in a top-level window's child tree.
     */
    void changeEvent(QEvent* event) override;

private:
    Q_DISABLE_COPY(QcwFilterableListWidget)

    QcwFilterableListWidgetPrivate* impl;
};

QT_END_NAMESPACE
