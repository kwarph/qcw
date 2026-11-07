// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "DemoGlobal.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QtPlugin>

class QcwWindow;

/**
 * @file DemoNotes.h
 * @brief The contract of the notes plugin, the demo's one plugin that adds no widget.
 *
 * @details The notes plugin subclasses QcwPlugin directly instead of one of the creator
 * interfaces: it builds no view and no perspective, so a creator would only add an unused
 * create() method. What it contributes is the other half of the plugin system — a service in
 * QcwServiceRegistry, an application-scope topic that announces changes to the store, a
 * command that the command palette lists, and a preferences page. The interface below is
 * declared with Q_DECLARE_INTERFACE so consumers reach the plugin by interface rather than by
 * its type() string.
 */
namespace demo
{

/// The category the notes plugin reports, kept apart from "View" and "Perspective".
inline constexpr QLatin1String NotesCategory { "Tool" };

/// The creator type of the notes plugin, as QcwPluginManager lists it.
inline constexpr QLatin1String NotesPluginType { "Notes" };

/// The type of the notes view, as its creator reports it and saved state refers to it.
inline constexpr QLatin1String NotesViewType { "NotesView" };

/// Id of the command that adds a note; the notes plugin registers the handler.
inline constexpr QLatin1String ActionAddNote { "Notes/AddNote" };

/// QSettings group the notes preferences page edits.
inline constexpr QLatin1String NotesSettingsGroup { "notes" };

/// QSettings key of the flag that appends every new note to the log.
inline constexpr QLatin1String NotesLogKey { "logNewNotes" };

/// The application-scope topic announcing that the notes store changed. NotesService publishes
/// it after every add, edit and removal; subscribers read the full list through INotesService.
inline constexpr QLatin1String TopicNotesChanged { "app/notes/changed" };

/// The number of notes after the change, as an int.
inline constexpr QLatin1String NoteCountKey { "count" };

/// One note as the notes view shows it.
struct Note
{
    QString text;
    QDateTime createdAt;

    bool operator==(const Note& other) const { return text == other.text && createdAt == other.createdAt; }

    bool operator!=(const Note& other) const { return !(*this == other); }
};

/**
 * @brief The notes store, implemented by the notes plugin's NotesService object.
 *
 * @details The service keeps the notes in the process and never touches a window, which is why
 * this interface names neither QcwWindow nor a widget. Every window shows the same store, so
 * the notes view of each window adds a note through the service rather than through its own
 * action; a view learns about changes by subscribing to demo::TopicNotesChanged.
 */
class INotesService
{
public:
    virtual ~INotesService() = default;

    /// Every note, oldest first.
    virtual QList<Note> notes() const = 0;

    /// Adds @p text and returns the note as stored.
    virtual Note addNote(const QString& text) = 0;

    /// Replaces the text of @p note. False when the note is no longer there.
    virtual bool updateNote(const Note& note, const QString& text) = 0;

    /// Removes @p note. False when it is no longer there.
    virtual bool removeNote(const Note& note) = 0;

    /// True while the notes made in this process are also appended to the log. The notes
    /// preferences page edits this; the plugin applies the logs itself.
    virtual bool logNewNotes() const = 0;

    /// Opens the notes view of @p window, creating it when that window has none yet.
    virtual void showNotes(QcwWindow* window) = 0;
};

QCW_DEMO_EXPORT INotesService* notesService();

} // namespace demo

Q_DECLARE_INTERFACE(demo::INotesService, "org.qcw.demo.INotesService")
