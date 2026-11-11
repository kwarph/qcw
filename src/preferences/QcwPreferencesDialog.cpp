// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwPreferencesDialog.h"
#include "QcwPreferencesWidget.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QPushButton>

QT_BEGIN_NAMESPACE

class QcwPreferencesDialogPrivate
{
public:
    explicit QcwPreferencesDialogPrivate(QcwPreferencesDialog* interface) :
        owner { interface }
    {
    }

    /// Re-applies the texts the constructor captured, for the language now installed.
    void retranslateUi();

public:
    QcwPreferencesDialog* owner;
    QPushButton* restoreButton { nullptr };
    QPushButton* exportButton { nullptr };
    QPushButton* importButton { nullptr };
};

void QcwPreferencesDialogPrivate::retranslateUi()
{
    restoreButton->setText(QCoreApplication::translate("QcwPreferencesDialog", "&Restore Defaults"));
    restoreButton->setToolTip(QCoreApplication::translate("QcwPreferencesDialog",
                                                          "Restore the settings on this page to their default values"));

    exportButton->setText(QCoreApplication::translate("QcwPreferencesDialog", "&Export"));
    exportButton->setToolTip(QCoreApplication::translate("QcwPreferencesDialog", "Export all settings to a file"));

    importButton->setText(QCoreApplication::translate("QcwPreferencesDialog", "&Import"));
    importButton->setToolTip(QCoreApplication::translate("QcwPreferencesDialog", "Import all settings from a file"));

    owner->setWindowTitle(QCoreApplication::translate("QcwPreferencesDialog", "Preferences"));
}

QcwPreferencesDialog::QcwPreferencesDialog(const QMap<QString, QcwPreferencesPage*>& pages,
                                           QcwSettingsProvider settingsProvider,
                                           QWidget* parent) :
    QcwNavigationContentDialog { parent }, impl { new QcwPreferencesDialogPrivate { this } }
{
    auto widget = new QcwPreferencesWidget { pages, settingsProvider, this };
    setContentWidget(widget);

    auto* btnBox = buttonBox();
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Close);

    // The base keeps the action buttons on the left and the button box on the right, with a
    // stretch in between. Add them in visual order so Restore, Export, Import stay left of
    // Close, as they were in the old hand-built row.
    impl->restoreButton = new QPushButton(this);
    addActionButton(impl->restoreButton);

    impl->exportButton = new QPushButton(this);
    addActionButton(impl->exportButton);

    impl->importButton = new QPushButton(this);
    addActionButton(impl->importButton);

    // Preferences pages apply changes live; pressing Return in a field must not close the
    // dialog through a default button.
    for (auto* button : findChildren<QPushButton*>())
    {
        button->setDefault(false);
        button->setAutoDefault(false);
    }

    connect(btnBox, &QDialogButtonBox::rejected, this, qOverload<>(&QDialog::reject));
    connect(impl->restoreButton, &QPushButton::clicked, widget, &QcwPreferencesWidget::restoreDefaults);
    connect(impl->exportButton, &QPushButton::clicked, widget, &QcwPreferencesWidget::exportSettings);
    connect(impl->importButton, &QPushButton::clicked, widget, &QcwPreferencesWidget::importSettings);

    impl->retranslateUi();
}

QcwPreferencesDialog::~QcwPreferencesDialog()
{
    delete impl;
}

void QcwPreferencesDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        impl->retranslateUi();
    }

    QDialog::changeEvent(event);
}

QT_END_NAMESPACE
