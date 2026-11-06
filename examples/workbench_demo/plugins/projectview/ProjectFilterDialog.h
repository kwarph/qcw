// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ProjectFilterProxyModel.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

/// Edits one ProjectFilter; call exec() and read filter() after acceptance.
class ProjectFilterDialog : public QDialog
{
public:
    explicit ProjectFilterDialog(const ProjectFilter& filter, QWidget* parent = nullptr);

    ProjectFilter filter() const;

private:
    QLineEdit* mPatterns;
    QComboBox* mMode;
    QCheckBox* mCaseSensitive;
    QCheckBox* mApplyToDirectories;
};
