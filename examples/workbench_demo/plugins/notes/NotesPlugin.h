// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPlugin.h"
#include "QcwPluginTemplate.h"

/**
 * @brief The notes plugin: the demo's plugin that adds no view of its own.
 *
 * Unlike every other plugin in this demo, this one does not derive from QcwViewCreator or
 * QcwPerspectiveCreator and creates nothing for a window to show in a tab. It is a plain
 * QcwPlugin whose category() is "Tool", and what it contributes is the rest of the plugin
 * system: a notes store as a service, an Add Note command that the command palette lists under
 * "Notes" and that no menu shows, and a preferences page. The plugin is fully useful before any
 * QcwWindow exists.
 *
 * The store itself is a separate NotesService object, not the plugin: the plugin only publishes
 * it and wires the command and the views around it.
 *
 * The demo also ships a NotesView, because a store needs somewhere to be seen, but that is a
 * separate plugin. The notes outlive the view, the view can be closed and reopened, and it
 * reaches the store through demo::INotesService rather than through a creator interface.
 */
class NotesPlugin : public QcwPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qcw.QcwPlugin" FILE "notes.json")
    QCW_PLUGIN_INTERFACE

public:
    QString category() const override;

protected:
    void doRegisterContributions() override;

    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void addNoteAndShow();
    void retranslateContributions();
};
