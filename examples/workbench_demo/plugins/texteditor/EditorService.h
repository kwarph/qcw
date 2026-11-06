// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "DemoServices.h"

#include <QObject>
#include <QSet>

class TextEditor;

/**
 * @brief The process-wide owner of the open TextEditors; implements demo::IEditorService.
 *
 * @details Every editor registers itself through trackEditor() once it is built; from then on
 * this object answers ProjectView's lookups, saves and renames no matter which folder or
 * perspective shows an editor. The service registry owns it for the whole run. The
 * opened/closed announcements are published by TextEditor itself through its window's
 * QcwTopicService; this service only tracks the live editors.
 */
class EditorService : public QObject, public demo::IEditorService
{
    Q_OBJECT
    Q_INTERFACES(demo::IEditorService)

public:
    explicit EditorService(QObject* parent = nullptr);

    void trackEditor(QWidget* editor) override;

    /// Re-reads the appearance settings for every open editor.
    void applyAppearanceSettings();

    QWidget* editorForFile(const QString& filePath) const override;
    QString editorFilePath(QWidget* editor) const override;
    bool saveEditor(QWidget* editor, QString* error) override;
    bool saveEditorTo(QWidget* editor, const QString& targetPath, QString* error) override;
    void remapEditorPaths(const QString& oldPrefix, const QString& newPrefix) override;
    void closeEditorsUnder(const QString& path) override;
    int saveEditorsUnder(const QString& path, QStringList* errors) override;
    QString textForFile(const QString& filePath) const override;
    int revisionForFile(const QString& filePath) const override;
    bool revealAt(const QString& filePath, int position) override;

private:
    QSet<TextEditor*> mEditors;
};
