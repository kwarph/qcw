// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "DemoNotes.h"
#include "QcwView.h"

#include <QList>

class QAction;
class QListWidget;
class QToolBar;

/**
 * @brief Shows the notes the notes plugin stores, in the window it is laid out in.
 *
 * The view owns no data: it asks demo::INotesService for the notes and subscribes to
 * demo::TopicNotesChanged on the application-scope topic service, so every notes view in the
 * process shows the same store and a note added from one window appears in all. Adding,
 * removing and editing go back through the service.
 *
 * Subscribing in the constructor rather than when first shown is possible because the
 * application-scope service outlives the view and subscribe() replays the last announcement,
 * so a view created late still catches up.
 *
 * The view is a plugin of its own, separate from the notes plugin. That mirrors the roles: the
 * store is a non-UI capability, the view is one way — the demo's only at the moment — to see
 * it, and closing the view leaves the notes untouched.
 */
class NotesView : public QcwView
{
    Q_OBJECT
public:
    explicit NotesView(QWidget* parent = nullptr);

    QToolBar* topRightToolBar() const override;

protected:
    void retranslateUi() override;

private:
    void connectToStore();
    void reloadNotes();
    void addNote();
    void removeCurrentNote();
    void editCurrentNote();

    QListWidget* mList;
    QToolBar* mToolBar;
    QAction* mAddAction;
    QAction* mRemoveAction;
    QAction* mEditAction;
    QList<demo::Note> mNotes;
};
