// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "DemoNotes.h"

#include <QList>
#include <QObject>

class QcwWindow;

/**
 * @brief The process-wide notes store; implements demo::INotesService.
 *
 * @details The plugin creates this object once and publishes it in QcwServiceRegistry. It
 * holds the notes, so every window sees the same store, and it knows how to open a notes view
 * in a window that has none. After every change it publishes demo::TopicNotesChanged on the
 * application-scope topic service; the views subscribe instead of being told one by one. It
 * never touches a menu or a command: the plugin does the wiring around it.
 */
class NotesService : public QObject, public demo::INotesService
{
    Q_OBJECT
    Q_INTERFACES(demo::INotesService)

public:
    explicit NotesService(QObject* parent = nullptr);

    QList<demo::Note> notes() const override;
    demo::Note addNote(const QString& text) override;
    bool updateNote(const demo::Note& note, const QString& text) override;
    bool removeNote(const demo::Note& note) override;
    bool logNewNotes() const override;
    void showNotes(QcwWindow* window) override;

    /// Sets whether new notes are also appended to the log; the preferences page writes this.
    void setLogNewNotes(bool enabled);

private:
    bool hasNotesView(QcwWindow* window) const;
    void openNotesView(QcwWindow* window);
    void announceChange();

    QList<demo::Note> mNotes;
    bool mLogNewNotes { true };
};
