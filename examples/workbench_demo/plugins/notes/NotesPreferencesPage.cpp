// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "NotesPreferencesPage.h"

#include "DemoNotes.h"

#include <QCheckBox>
#include <QEvent>
#include <QFormLayout>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>

NotesPreferencesPage::NotesPreferencesPage(QWidget* parent) :
    QcwPreferencesPage { parent }, mLogNewNotes { new QCheckBox { this } }
{
    auto* layout = new QVBoxLayout { this };
    layout->addWidget(mLogNewNotes);
    layout->addStretch();

    connect(mLogNewNotes, &QCheckBox::toggled, this, [this](bool checked)
    {
        writeSettings();
        emit logNewNotesChanged(checked);
    });

    retranslateUi();
}

void NotesPreferencesPage::retranslateUi()
{
    mLogNewNotes->setText(tr("Append every new note to the log"));
}

void NotesPreferencesPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }

    QcwPreferencesPage::changeEvent(event);
}

QString NotesPreferencesPage::settingsGroup() const
{
    return QString(demo::NotesSettingsGroup);
}

QString NotesPreferencesPage::name() const
{
    return tr("Notes");
}

bool NotesPreferencesPage::matchesFilter(const QString& filterText) const
{
    const QString searchable = QStringList { name(), mLogNewNotes->text() }.join(QLatin1Char(' '));
    return searchable.contains(filterText, Qt::CaseInsensitive);
}

void NotesPreferencesPage::restoreDefaults()
{
    {
        const QSignalBlocker blocker { mLogNewNotes };
        mLogNewNotes->setChecked(true);
    }

    writeSettings();
    emit logNewNotesChanged(true);
}

void NotesPreferencesPage::loadSettings()
{
    const bool enabled = settings().value(QString(demo::NotesLogKey), true).toBool();
    const QSignalBlocker blocker { mLogNewNotes };
    mLogNewNotes->setChecked(enabled);
}

void NotesPreferencesPage::writeSettings()
{
    settings().setValue(QString(demo::NotesLogKey), mLogNewNotes->isChecked());
}
