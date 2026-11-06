// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QList>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QString>

/**
 * @brief The filter settings ProjectView keeps and persists.
 *
 * @details Patterns are semicolon-separated wildcards such as @c "*.cpp; *.h". Directories
 * are always kept for navigation unless applyToDirectories is set, in which case the same
 * rule hides or shows them by name too.
 */
struct ProjectFilter
{
    enum class Mode
    {
        ShowOnly,    ///< Keep rows whose name matches a pattern.
        HideMatching ///< Drop rows whose name matches a pattern.
    };

    QString patterns;
    Mode mode { Mode::ShowOnly };
    bool caseSensitive { false };
    bool applyToDirectories { false };

    bool isEmpty() const
    {
        return patterns.trimmed().isEmpty();
    }

    static QList<QRegularExpression> compile(const QString& patterns, bool caseSensitive);
};

/**
 * @brief Filters the rows of a ProjectTreeModel by file-name wildcards.
 *
 * @details The top-level project rows are never filtered: they are how the tree is navigated,
 * so the patterns only apply to the files and folders inside a project.
 */
class ProjectFilterProxyModel : public QSortFilterProxyModel
{
public:
    explicit ProjectFilterProxyModel(QObject* parent = nullptr);

    void setFilter(const ProjectFilter& filter);
    const ProjectFilter& filter() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    bool matchesName(const QString& name) const;

    ProjectFilter mFilter;
    QList<QRegularExpression> mExpressions;
};
