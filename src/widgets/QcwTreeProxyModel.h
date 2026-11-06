// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QSortFilterProxyModel>
#include <QString>

QT_BEGIN_NAMESPACE

/**
 * @class QcwTreeProxyModel
 * @brief Filters a tree by a literal search text while keeping the matches reachable.
 *
 * @details A row is accepted when the search text matches the row itself, when it matches
 * one of the row's ancestors (a matching node brings its whole subtree), or when it matches
 * one of the row's descendants (a match deep in the tree keeps its ancestors visible).
 * Without a search text every non-excluded row is accepted.
 *
 * The compared value is the filterRole() data of the column selected by filterKeyColumn()
 * (column 0 by default; -1 compares every column). Matching is a case-insensitive
 * substring test by default and never a regular expression, so text such as "(" is
 * searched literally. The inherited setFilterCaseSensitivity(), filterRole() and
 * filterKeyColumn() are honored.
 *
 * Subclasses that keep searchable data outside the filter role — a preferences page, for
 * example — override matchesAdditionalConditions(), which can only widen the match set.
 * Rows that must never appear regardless of the search text — dot files, for example —
 * are hidden by isRowExcluded(), which only narrows it. The inherited ways of installing a
 * filter (filterRegularExpression(), setFilterWildcard()) are not used, because this model
 * performs the comparison itself; setFilterText() is the only filter input.
 *
 * @see QSortFilterProxyModel
 */
class QCW_EXPORT QcwTreeProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

    /// The literal search text; an empty text accepts every non-excluded row. @see setFilterText()
    Q_PROPERTY(QString filterText READ filterText RESET clearFilterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    /**
     * @brief Builds a proxy with no filter text.
     * @details The text comparison starts out case-insensitive; the inherited
     *          setFilterCaseSensitivity() can change that.
     */
    explicit QcwTreeProxyModel(QObject* parent = nullptr);

    /// The current search text; empty while the whole tree is shown.
    QString filterText() const;

    /**
     * @brief Clears the search text and shows the whole tree.
     * @note Emits filterTextChanged() only when the text actually changes.
     * @see setFilterText()
     */
    Q_SLOT void clearFilterText();

    /**
     * @brief Sets the literal search text and refilters the tree immediately.
     * @param filterText Substring to look for; an empty text accepts every non-excluded row.
     * @note Setting the text that is already set does nothing.
     * @see filterText(), clearFilterText(), matchesAdditionalConditions()
     */
    Q_SLOT void setFilterText(const QString& filterText);

    /// Emitted after the search text changed, with the new text.
    Q_SIGNAL void filterTextChanged(const QString& filterText);

protected:
    /**
     * @brief Decides whether a source row is part of the filtered tree.
     * @param sourceRow Row to test in its parent.
     * @param sourceParent Parent of the row; invalid for top-level rows.
     * @return True when the row, one of its ancestors or one of its descendants matches.
     * @details QSortFilterProxyModel drives the filtering through this override. The
     *          hierarchy check lives here; the per-row comparison is rowMatchesFilter().
     */
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

    /**
     * @brief Hard visibility gate evaluated before any search-text comparison.
     * @param sourceIndex Source index of the row to test, in the first column.
     * @return True when the row never appears in the filtered tree.
     *
     * @details filterAcceptsRow() calls it for the row itself, for every candidate
     * ancestor and for every candidate descendant, so an excluded row is never shown
     * and never makes another row reachable. Unlike matchesAdditionalConditions(),
     * which can only widen the match set, this hook only narrows it, and it ignores
     * the search text. Override it for rows that must be hidden whatever the filter
     * says, such as dot files. When a subclass's answer changes at runtime, refilter
     * by calling beginFilterChange(), then endFilterChange(Direction::Rows). The
     * default returns false.
     */
    virtual bool isRowExcluded(const QModelIndex& sourceIndex) const;

    /**
     * @brief Extends the per-row match beyond the filterRole() text.
     * @param sourceIndex Source index of the row to test, in the first column.
     * @param filterText The current search text; never empty when this is called.
     * @return True when the row matches for a reason the base class cannot see.
     *
     * @details Called only after the filterRole() comparison failed, and called for every
     * row examined as a candidate, ancestor or descendant. Override it to search data held
     * outside the item model, such as the page behind a preferences node. Keep the
     * implementation cheap and free of side effects, because one filter pass calls it
     * repeatedly. The default returns false.
     */
    virtual bool matchesAdditionalConditions(const QModelIndex& sourceIndex, const QString& filterText) const;

private:
    /// True when the row's filterRole() text or matchesAdditionalConditions() matches.
    bool rowMatchesFilter(const QModelIndex& sourceIndex) const;

    /// True when any row below @p sourceIndex matches the search text.
    bool hasMatchingDescendant(const QModelIndex& sourceIndex) const;

    QString mFilterText;
};

QT_END_NAMESPACE
