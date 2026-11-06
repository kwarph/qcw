// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ProjectFilterDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

ProjectFilterDialog::ProjectFilterDialog(const ProjectFilter& filter, QWidget* parent) :
    QDialog { parent },
    mPatterns { new QLineEdit { filter.patterns, this } },
    mMode { new QComboBox { this } },
    mCaseSensitive { new QCheckBox { tr("Case sensitive"), this } },
    mApplyToDirectories { new QCheckBox { tr("Apply the filter to folders too"), this } }
{
    setWindowTitle(tr("Filter Files"));

    mPatterns->setPlaceholderText(tr("*.cpp; *.h"));
    mPatterns->setClearButtonEnabled(true);

    mMode->addItem(tr("Show only matching files"), static_cast<int>(ProjectFilter::Mode::ShowOnly));
    mMode->addItem(tr("Hide matching files"), static_cast<int>(ProjectFilter::Mode::HideMatching));
    mMode->setCurrentIndex(mMode->findData(static_cast<int>(filter.mode)));

    mCaseSensitive->setChecked(filter.caseSensitive);
    mApplyToDirectories->setChecked(filter.applyToDirectories);

    auto* hint = new QLabel { tr("Separate wildcard patterns with semicolons. "
                                 "Leave the field empty to show every file."),
                              this };
    hint->setWordWrap(true);

    auto* form = new QFormLayout;
    form->addRow(tr("Patterns:"), mPatterns);
    form->addRow(tr("Mode:"), mMode);

    auto* buttons = new QDialogButtonBox { QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this };
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout { this };
    layout->addWidget(hint);
    layout->addLayout(form);
    layout->addWidget(mCaseSensitive);
    layout->addWidget(mApplyToDirectories);
    layout->addWidget(buttons);

    mPatterns->setFocus();
}

ProjectFilter ProjectFilterDialog::filter() const
{
    ProjectFilter result;
    result.patterns = mPatterns->text();
    result.mode = static_cast<ProjectFilter::Mode>(mMode->currentData().toInt());
    result.caseSensitive = mCaseSensitive->isChecked();
    result.applyToDirectories = mApplyToDirectories->isChecked();
    return result;
}
