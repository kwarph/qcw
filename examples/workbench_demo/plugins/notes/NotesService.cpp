// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "NotesService.h"

#include "QcwPluginManager.h"
#include "QcwTabFolder.h"
#include "QcwTopicService.h"
#include "QcwView.h"
#include "QcwViewCreator.h"
#include "QcwWindow.h"

#include <QDebug>

NotesService::NotesService(QObject* parent) :
    QObject { parent }
{
}

QList<demo::Note> NotesService::notes() const
{
    return mNotes;
}

demo::Note NotesService::addNote(const QString& text)
{
    demo::Note note { text, QDateTime::currentDateTime() };
    mNotes.append(note);
    announceChange();

    if (mLogNewNotes)
    {
        qInfo().noquote() << QStringLiteral("Note added: %1").arg(text);
    }

    return note;
}

bool NotesService::updateNote(const demo::Note& note, const QString& text)
{
    for (auto& stored : mNotes)
    {
        if (stored == note)
        {
            stored.text = text;
            announceChange();
            return true;
        }
    }

    return false;
}

bool NotesService::removeNote(const demo::Note& note)
{
    if (mNotes.removeAll(note) == 0)
    {
        return false;
    }

    announceChange();
    return true;
}

bool NotesService::logNewNotes() const
{
    return mLogNewNotes;
}

void NotesService::setLogNewNotes(bool enabled)
{
    mLogNewNotes = enabled;
}

void NotesService::showNotes(QcwWindow* window)
{
    if (window)
    {
        openNotesView(window);
    }
}

bool NotesService::hasNotesView(QcwWindow* window) const
{
    const auto views = window->allViews();
    for (auto* view : views)
    {
        if (view->type() == demo::NotesViewType)
        {
            return true;
        }
    }

    return false;
}

void NotesService::openNotesView(QcwWindow* window)
{
    if (!window || hasNotesView(window))
    {
        return;
    }

    auto* creator = pluginManager->getPluginT<QcwViewCreator>(QString(demo::NotesViewType));
    auto* folder = window->sharedFolder();
    if (!creator || !folder)
    {
        return;
    }

    // The shared folder belongs to the window and survives perspective switches, so a note
    // stays where the user can see it no matter which perspective is open.
    auto* view = creator->create(folder, true);
    if (!view)
    {
        return;
    }

    // The store opened this view, so it says so: the line is how the log shows that a
    // window that had no notes pane gained one.
    const QString title = window->makeNewViewName(creator->defaultWindowTitle());
    folder->setCurrentIndex(folder->addTab(view, title, creator->defaultViewIcon()));
    qInfo().noquote() << QStringLiteral("Notes service: opened \"%1\" in window %2").arg(title, window->id());
}

void NotesService::announceChange()
{
    // The topic carries no list: subscribers read it through INotesService, so the store stays
    // the single source of truth. The count lets a subscriber skip the read when it only cares
    // that something changed.
    QVariantMap payload;
    payload[QString(demo::NoteCountKey)] = mNotes.size();
    QcwTopicService::forApplication()->publish(QString(demo::TopicNotesChanged), payload);
}
