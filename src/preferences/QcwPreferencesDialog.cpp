// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwPreferencesDialog.h"
#include "QcwPreferencesWidget.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

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
    QDialog { parent }, impl { new QcwPreferencesDialogPrivate { this } }
{
    auto verticalLayout = new QVBoxLayout(this);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    verticalLayout->setSpacing(0);
    verticalLayout->setObjectName("verticalLayout");

    auto widget = new QcwPreferencesWidget(pages, settingsProvider, this);
    verticalLayout->addWidget(widget);

    auto line = new QFrame(this);
    line->setObjectName("line");
    line->setFrameShape(QFrame::Shape::HLine);
    line->setFrameShadow(QFrame::Shadow::Sunken);
    verticalLayout->addWidget(line);

    auto buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(9, 9, 9, 9);
    buttonLayout->setSpacing(9);
    buttonLayout->setObjectName("horizontalLayout");

    impl->restoreButton = new QPushButton(this);
    buttonLayout->addWidget(impl->restoreButton);

    impl->exportButton = new QPushButton(this);
    buttonLayout->addWidget(impl->exportButton);

    impl->importButton = new QPushButton(this);
    buttonLayout->addWidget(impl->importButton);

    auto buttonBox = new QDialogButtonBox(this);
    buttonBox->setObjectName("buttonBox");
    buttonBox->setOrientation(Qt::Orientation::Horizontal);
    buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Close);
    buttonLayout->addWidget(buttonBox);

    verticalLayout->addLayout(buttonLayout);

    // Preferences pages apply changes live; pressing Return in a field must not close the
    // dialog through a default button.
    for (auto* button : findChildren<QPushButton*>())
    {
        button->setDefault(false);
        button->setAutoDefault(false);
    }

    connect(buttonBox, &QDialogButtonBox::rejected, this, qOverload<>(&QDialog::reject));
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
