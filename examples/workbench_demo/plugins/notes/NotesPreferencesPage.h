// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPreferencesPage.h"

class QCheckBox;

/**
 * @brief The notes preferences page, contributed by a plugin that has no view.
 *
 * The page edits the one setting the notes store has: whether every note added in this
 * process is also written to the log, which makes it visible in the LogView. The plugin reads
 * the value itself, so the page only writes — a page a plugin contributes from
 * doRegisterContributions() is the same kind of object as one contributed by a view creator.
 */
class NotesPreferencesPage : public QcwPreferencesPage
{
    Q_OBJECT
public:
    explicit NotesPreferencesPage(QWidget* parent = nullptr);

    QString settingsGroup() const override;
    QString name() const override;
    bool matchesFilter(const QString& filterText) const override;
    void restoreDefaults() override;
    void loadSettings() override;

signals:
    /// Emitted after the page wrote a changed value.
    void logNewNotesChanged(bool enabled);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void writeSettings();

    QCheckBox* mLogNewNotes;
};
