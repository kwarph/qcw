// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwView.h"
#include "QcwCommandHandler.h"

#include <QMap>
#include <QPair>
#include <QString>
#include <QVariantMap>
#include <QHash>

class QAction;
class QLabel;
class QMenu;
class QWidget;
class QcwTabFolder;
class CodeEditor;

/**
 * @brief A plain-text editor view with line numbers and a cursor status widget.
 *
 * The editor keeps the line endings of the file it loaded: the text is normalized to `\n`
 * while it is edited and converted back when it is written. A tab is marked dirty through its
 * folder whenever the document changes, and closing a dirty tab asks whether to save first.
 *
 * The editor publishes the window-scoped demo::TopicEditorSelection while it is the window's
 * current view, and the editor announcement topics when it opens, saves, changes path or is
 * destroyed. Both travel through owningWindow()->topicService().
 */
class TextEditor : public QcwView
{
    Q_OBJECT
public:
    explicit TextEditor(QWidget* parent = nullptr);
    ~TextEditor() override;

    /// Reads @p filePath, detects its line endings and makes it the editor's file.
    bool loadFile(const QString& filePath);

    /// Writes the document to its own file, asking for a path when it has none.
    bool save(QString* error = nullptr);

    /// Writes the document to @p filePath and makes that its file.
    bool saveTo(const QString& filePath, QString* error = nullptr);

    /// The file on disk, empty for a document that was never saved.
    QString filePath() const;
    void setFilePath(const QString& filePath);

    /// Whether the document has unsaved changes.
    bool isModified() const;

    /**
     * @brief Publishes an editor announcement on @p topic through the owning window's service.
     * @param topic A "win/..." topic id from DemoServices.h.
     * @param data Payload keys to add; the editor's file path is added under demo::PathKey.
     * @details Does nothing while the editor is not in a window.
     */
    void publishEvent(QLatin1String topic, const QVariantMap& data);

    /// Re-reads the text editor settings group and applies it to this editor.
    void applyAppearanceSettings();

    /// Closes the tab that shows this editor; the editor is deleted with it.
    void closeTab();

    /// The document text, normalized to `\n`.
    QString text() const;

    /// The document revision; it changes on every edit and lets readers skip re-parsing.
    int documentRevision() const;

    /// Puts the cursor at @p position and gives the editor focus.
    void revealPosition(int position);

    QWidget* statusBarWidget() const override;
    QHash<QString, QcwCommandHandler*> commandHandlers() const override;
    void canClose(const std::function<void(bool)>& callback) override;

    void saveExtraState(QVariantMap& state) const override;
    void restoreExtraState(const QVariantMap& state) override;

protected:
    bool event(QEvent* event) override;
    void retranslateUi() override;

private:
    enum class LineEnding
    {
        LF,
        CRLF,
        CR
    };

    /// The folder showing this editor, or nullptr while it is not in a tab.
    QcwTabFolder* currentFolder();
    int currentTabIndex();

    void updateCursorStatus();
    void updateEditActions();
    void updateTabInfo();
    void updateDirtyState();
    void connectToCurrentFolder();
    bool saveAs(QString* error);
    bool confirmClose();
    void setupHandlers();
    /// QString overload for the internal call sites; same contract as the public one.
    void publishEvent(const QString& topic, const QVariantMap& data = {});
    /// Publishes the cursor state on demo::TopicEditorSelection while this editor is current.
    void publishSelection();

    CodeEditor* mEditor;
    QWidget* mStatusWidget;
    QLabel* mPositionLabel;
    QLabel* mEndingLabel;
    QMenu* mContextMenu;
    QString mFilePath;
    LineEnding mLineEnding { LineEnding::LF };
    QHash<QString, QcwCommandHandler*> mHandlers;
    bool mOpenedPublished { false }; // win/editor/opened is published once, on first show.
};
