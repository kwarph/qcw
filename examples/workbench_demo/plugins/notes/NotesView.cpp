// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "NotesView.h"

#include "QcwOverlayScrollBarUtils.h"
#include "QcwTopicService.h"

#include <QAction>
#include <QInputDialog>
#include <QListWidget>
#include <QToolBar>
#include <QVBoxLayout>

namespace
{
QString noteLabel(const demo::Note& note)
{
    const QString text = note.text.trimmed().isEmpty() ? NotesView::tr("(empty note)") : note.text.trimmed();
    return QStringLiteral("%1  —  %2")
        .arg(text, note.createdAt.toString(QLocale::system().dateTimeFormat(QLocale::ShortFormat)));
}

QString noteSummary(const demo::Note& note)
{
    const QString firstLine = note.text.trimmed().section(QLatin1Char('\n'), 0, 0);
    return firstLine.isEmpty() ? NotesView::tr("(empty note)") : firstLine;
}
} // namespace

NotesView::NotesView(QWidget* parent) :
    QcwView { parent },
    mList { new QListWidget { this } },
    mToolBar { new QToolBar { this } },
    mAddAction { new QAction { tr("Add Note..."), this } },
    mRemoveAction { new QAction { tr("Remove Note"), this } },
    mEditAction { new QAction { tr("Edit Note..."), this } }
{
    setWindowTitle(tr("Notes"));

    auto* layout = new QVBoxLayout { this };
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(mList);

    mList->setFrameShape(QFrame::NoFrame);

    attachOverlayScrollBar(mList, this);

    mAddAction->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    mRemoveAction->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    mEditAction->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    for (auto* action : { mAddAction, mRemoveAction, mEditAction })
    {
        mToolBar->addAction(action);
    }

    connect(mAddAction, &QAction::triggered, this, &NotesView::addNote);
    connect(mRemoveAction, &QAction::triggered, this, &NotesView::removeCurrentNote);
    connect(mEditAction, &QAction::triggered, this, &NotesView::editCurrentNote);
    connect(mList, &QListWidget::itemDoubleClicked, this, &NotesView::editCurrentNote);
    connect(mList, &QListWidget::currentRowChanged, this, [this](int row)
    {
        mRemoveAction->setEnabled(row >= 0);
        mEditAction->setEnabled(row >= 0);
    });

    mRemoveAction->setEnabled(false);
    mEditAction->setEnabled(false);

    connectToStore();
}

void NotesView::retranslateUi()
{
    mAddAction->setText(tr("Add Note..."));
    mRemoveAction->setText(tr("Remove Note"));
    mEditAction->setText(tr("Edit Note..."));
}

QToolBar* NotesView::topRightToolBar() const
{
    return mToolBar;
}

void NotesView::connectToStore()
{
    auto* store = demo::notesService();
    if (!store)
    {
        return;
    }

    // The service and the view share the application scope, so one subscription sees the
    // changes every window makes. subscribe() replays the last announcement, if there was one;
    // the reload after it covers a view created before the first change.
    QcwTopicService::forApplication()->subscribe(QString(demo::TopicNotesChanged), this, [this](const QVariantMap&)
    {
        reloadNotes();
    });
    reloadNotes();
}

void NotesView::reloadNotes()
{
    auto* store = demo::notesService();
    if (!store)
    {
        return;
    }

    const int current = mList->currentRow();

    mNotes = store->notes();
    mList->clear();
    for (const auto& note : std::as_const(mNotes))
    {
        auto* item = new QListWidgetItem { noteSummary(note), mList };
        item->setToolTip(note.text);
    }

    if (current >= 0 && current < mList->count())
    {
        mList->setCurrentRow(current);
    }
}

void NotesView::addNote()
{
    auto* store = demo::notesService();
    if (!store)
    {
        return;
    }

    bool accepted = false;
    const QString text = QInputDialog::getMultiLineText(this, tr("Add Note"), tr("Note:"), QString(), &accepted);
    if (accepted && !text.trimmed().isEmpty())
    {
        store->addNote(text);
    }
}

void NotesView::removeCurrentNote()
{
    auto* store = demo::notesService();
    const int row = mList->currentRow();
    if (!store || row < 0 || row >= mNotes.size())
    {
        return;
    }

    store->removeNote(mNotes.at(row));
}

void NotesView::editCurrentNote()
{
    auto* store = demo::notesService();
    const int row = mList->currentRow();
    if (!store || row < 0 || row >= mNotes.size())
    {
        return;
    }

    const demo::Note note = mNotes.at(row);
    bool accepted = false;
    const QString text = QInputDialog::getMultiLineText(this, tr("Edit Note"), tr("Note:"), note.text, &accepted);
    if (accepted && text != note.text)
    {
        store->updateNote(note, text);
    }
}
