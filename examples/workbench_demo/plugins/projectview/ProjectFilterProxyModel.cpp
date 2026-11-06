// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ProjectFilterProxyModel.h"

#include "ProjectTreeModel.h"

QList<QRegularExpression> ProjectFilter::compile(const QString& patterns, bool caseSensitive)
{
    QList<QRegularExpression> expressions;
    const auto parts = patterns.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const auto& part : parts)
    {
        const QString pattern = part.trimmed();
        if (pattern.isEmpty())
        {
            continue;
        }

        QRegularExpression::PatternOptions options;
        if (!caseSensitive)
        {
            options |= QRegularExpression::CaseInsensitiveOption;
        }

        const QString expression = QRegularExpression::wildcardToRegularExpression(pattern);
        expressions.append(QRegularExpression { expression, options });
    }

    return expressions;
}

ProjectFilterProxyModel::ProjectFilterProxyModel(QObject* parent) :
    QSortFilterProxyModel { parent }
{
}

void ProjectFilterProxyModel::setFilter(const ProjectFilter& filter)
{
    beginFilterChange();
    mFilter = filter;
    mExpressions = ProjectFilter::compile(mFilter.patterns, mFilter.caseSensitive);
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

const ProjectFilter& ProjectFilterProxyModel::filter() const
{
    return mFilter;
}

bool ProjectFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    // Project rows are the tree's entry points, so no filter may hide them.
    if (!sourceParent.isValid())
    {
        return true;
    }

    if (mExpressions.isEmpty())
    {
        return true;
    }

    auto* model = qobject_cast<ProjectTreeModel*>(sourceModel());
    if (!model)
    {
        return true;
    }

    const QModelIndex index = model->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
    {
        return true;
    }

    if (model->isDir(index) && !mFilter.applyToDirectories)
    {
        return true;
    }

    const bool match = matchesName(model->fileName(index));
    return mFilter.mode == ProjectFilter::Mode::ShowOnly ? match : !match;
}

bool ProjectFilterProxyModel::matchesName(const QString& name) const
{
    for (const auto& expression : mExpressions)
    {
        if (expression.match(name).hasMatch())
        {
            return true;
        }
    }

    return false;
}
