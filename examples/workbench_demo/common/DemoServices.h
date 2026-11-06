// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "DemoGlobal.h"
#include "QcwSettings.h"

#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QtPlugin>

class QObject;
class QWidget;

/**
 * @file DemoServices.h
 * @brief The small contract the demo plugins use to talk to one another.
 *
 * @details TextEditor and ProjectView live in separate plugin libraries, so they cannot link
 * to each other. Each service is implemented by a dedicated object inside the providing plugin
 * and published in the process-wide QcwServiceRegistry under its interface id; the other
 * plugins look it up by the interface and cast with qobject_cast(). The interfaces hold no
 * state; the implementing QObject does. A creator only builds its views and never answers a
 * service itself.
 *
 * Activity lines do not need a service: they are written with qInfo(), so they land in the
 * process log and the LogView, which follows QcwLogger, sees them with their level.
 *
 * Topics follow the same rule: their ids and payload keys are declared here — or, for the notes
 * store, in DemoNotes.h — while the payloads travel through a QcwTopicService. Every plugin
 * includes those headers, so both sides of a topic contract compile against the same names.
 */
namespace demo
{

inline constexpr QLatin1String ToolBarEdit { "demo/ToolBar/Edit" };

/// The id of the View menu the demo window inserts after Edit.
inline constexpr QLatin1String MenuView { "View" };

// File commands ProjectView contributes and MainWindow registers in the window's File
// skeleton. Projects are a demo concept, so the ids live here rather than in
// QcwWindowActionIds.
inline constexpr QLatin1String ActionNewProject { "File/NewProject" };
inline constexpr QLatin1String ActionNewFile { "File/NewFile" };
inline constexpr QLatin1String ActionOpenProject { "File/OpenProject" };
inline constexpr QLatin1String ActionCloseProject { "File/CloseProject" };

/// QSettings key holding the project folders ProjectView reopens.
inline constexpr QLatin1String ProjectRootsKey { "demo/projectRoots" };

/// QSettings key holding the project folder most actions act on.
inline constexpr QLatin1String CurrentProjectKey { "demo/currentProject" };

/// The single project folder earlier versions remembered. Read only, to migrate it into
/// ProjectRootsKey; rememberProjectRoots() drops it.
inline constexpr QLatin1String ProjectRootKey { "demo/projectRoot" };

/// QSettings group the TextEditor appearance page edits.
inline constexpr QLatin1String TextEditorSettingsGroup { "appearance/textEditor" };

/// QSettings group the LogView appearance page edits.
inline constexpr QLatin1String LogViewSettingsGroup { "appearance/logView" };

/// The creator type ProjectView asks for when it opens a file.
inline constexpr QLatin1String TextEditorType { "TextEditor" };

/// The creator type of the log view.
inline constexpr QLatin1String LogViewType { "LogView" };

/// The creator type of the project file manager.
inline constexpr QLatin1String ProjectViewType { "ProjectView" };

/// The creator type of the symbol outline.
inline constexpr QLatin1String OutlineViewType { "OutlineView" };

// --- Topic IDs ---
//
// The topics here are window-scoped: they are published and consumed through
// owningWindow()->topicService(). The "win/" prefix follows the framework convention; the
// service itself does not validate it. The editor publishes the selection and announcement
// topics, the project view subscribes to the announcements, and the outline subscribes to the
// editor selection topic. The one application-scope topic is demo::TopicNotesChanged, declared
// in DemoNotes.h.

// The current editor's cursor and selection state. Cached and replayed to a late subscriber;
// cleared when the editor hides.
inline constexpr QLatin1String TopicEditorSelection { "win/selection/editor" };

// The file currently selected in the project tree. Cached and replayed; cleared when the tree
// hides.
inline constexpr QLatin1String TopicProjectSelection { "win/selection/project" };

// Editor announcements. Subscribers use them to refresh derived state such as save actions;
// they are not a history. The current service caches every topic, so a late subscriber may
// replay the last announcement. The editor's path travels under PathKey.
inline constexpr QLatin1String TopicEditorOpened { "win/editor/opened" };
inline constexpr QLatin1String TopicEditorClosed { "win/editor/closed" };
inline constexpr QLatin1String TopicEditorSaved { "win/editor/saved" };

// TopicEditorPathChanged adds OldPathKey with the path before the change.
inline constexpr QLatin1String TopicEditorPathChanged { "win/editor/pathChanged" };

// --- Payload Keys ---
inline constexpr QLatin1String PathKey { "path" };
inline constexpr QLatin1String OldPathKey { "oldPath" };
inline constexpr QLatin1String IsDirKey { "isDir" };
inline constexpr QLatin1String PositionKey { "position" };
inline constexpr QLatin1String AnchorKey { "anchor" };
// The document revision the positions belong to; a change invalidates a cached parse.
inline constexpr QLatin1String RevisionKey { "revision" };
inline constexpr QLatin1String SelectedTextKey { "selectedText" };

/**
 * @brief The bridge ProjectView uses to find, save and follow the open TextEditor views.
 *
 * @details The editor widget is returned as a plain QWidget because ProjectView must not know
 * the editor plugin's classes. Every method tolerates a widget that is not a TextEditor.
 */
class IEditorService
{
public:
    virtual ~IEditorService() = default;

    /// The open editor showing @p filePath, or nullptr.
    virtual QWidget* editorForFile(const QString& filePath) const = 0;

    /// Starts following @p editor so lookups and saves reach it. The editor calls this on
    /// itself once it is built; a widget that is not a TextEditor is ignored.
    virtual void trackEditor(QWidget* editor) = 0;

    /// The file @p editor is showing, or an empty string.
    virtual QString editorFilePath(QWidget* editor) const = 0;

    /// Writes @p editor to its own file. @p error receives a message on failure.
    virtual bool saveEditor(QWidget* editor, QString* error) = 0;

    /// Saves @p editor under @p targetPath and makes that its new file.
    virtual bool saveEditorTo(QWidget* editor, const QString& targetPath, QString* error) = 0;

    /// Updates editors whose file is @p oldPrefix or below it after a rename or move.
    virtual void remapEditorPaths(const QString& oldPrefix, const QString& newPrefix) = 0;

    /// Closes every editor showing a file at or below @p path.
    virtual void closeEditorsUnder(const QString& path) = 0;

    /// Saves every modified editor showing a file at or below @p path. Returns how many were
    /// written; failures are appended to @p errors.
    virtual int saveEditorsUnder(const QString& path, QStringList* errors) = 0;

    /// The document text of the editor showing @p filePath, or an empty string when none does.
    virtual QString textForFile(const QString& filePath) const = 0;

    /// The document revision of the editor showing @p filePath, or -1 when none does.
    virtual int revisionForFile(const QString& filePath) const = 0;

    /**
     * @brief Brings the editor showing @p filePath to the front and moves its cursor.
     * @param filePath The file to reveal.
     * @param position A character position in the document; negative leaves the cursor alone.
     * @return @c true when an open editor shows the file; @c false otherwise.
     *
     * @details A navigation request, not a published selection: the editor publishes its own
     * selection topic once the cursor moves, and a view that follows that topic must not
     * publish one back or the two would chase each other.
     */
    virtual bool revealAt(const QString& filePath, int position) = 0;
};

QCW_DEMO_EXPORT IEditorService* editorService();

} // namespace demo

Q_DECLARE_INTERFACE(demo::IEditorService, "org.qcw.demo.IEditorService")
