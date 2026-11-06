// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPluginTemplate.h"
#include "QcwViewCreator.h"

/**
 * @brief Creates NotesView views, the window in which the notes store is visible.
 *
 * A separate plugin from the notes store, which has no view of its own. The creator is a thin
 * shell around NotesView: the view finds the store through demo::notesService() when it is
 * first shown, so the two plugins never include each other's classes.
 */
class NotesViewCreator : public QcwViewCreator
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qcw.QcwViewCreator" FILE "notesview.json")
    Q_INTERFACES(QcwViewCreator)
    QCW_PLUGIN_INTERFACE

public:
    QString defaultWindowTitle() const override;
    QIcon defaultViewIcon() const override;

private:
    QcwView* doCreate(QWidget* parent, const QList<QVariant>& args) override;
};
