// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "NotesPlugin.h"

#include "DemoNotes.h"
#include "NotesPreferencesPage.h"
#include "NotesService.h"
#include "PluginInfo.h"
#include "QcwLambdaCommandHandler.h"
#include "QcwCommandRegistry.h"
#include "QcwPreferencesManager.h"
#include "QcwServiceRegistry.h"
#include "QcwSettings.h"
#include "QcwTopicRegistry.h"
#include "QcwWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QKeySequence>
#include <QTimer>

QString NotesPlugin::category() const
{
    return demo::NotesCategory;
}

void NotesPlugin::doRegisterContributions()
{
    // This is the whole of what a non-UI plugin does: publish what it adds. The loader calls
    // it once, before any window exists, and nothing here needs a window.
    auto* service = new NotesService;
    if (!serviceRegistry->registerService<demo::INotesService>(service))
    {
        delete service;
        return;
    }

    QcwTopicInfo topic;
    topic.id = QString(demo::TopicNotesChanged);
    topic.description = tr("The notes store changed; subscribers read the full list through INotesService.");
    topic.publisher = QStringLiteral("Notes");
    topic.isStateful = false;
    topic.payloadKeys = { { QString(demo::NoteCountKey), QStringLiteral("int") } };
    topicRegistry->registerTopic(topic);

    auto* page = new NotesPreferencesPage;
    if (preferencesManager->registerPage(page))
    {
        connect(page, &NotesPreferencesPage::logNewNotesChanged, service, &NotesService::setLogNewNotes);
    }
    else
    {
        delete page;
    }

    retranslateContributions();

    qInfo() << "Notes plugin: registered the store, the Add Note action and the preferences page";

    service->setLogNewNotes(QcwSettings::get().value(QString(demo::NotesLogKey), true).toBool());

    // A window that has no notes view is given one when a note arrives, which saves the user
    // from opening the view by hand first. The filter sees the window only once it is built,
    // so the check is deferred to the next turn.
    QCoreApplication::instance()->installEventFilter(this);
}

void NotesPlugin::retranslateContributions()
{
    QcwCommandInfo info;
    info.id = QString(demo::ActionAddNote);
    info.category = tr("Notes");
    info.label = tr("Add Note...");
    info.description = tr("Append a note to the notes view");
    info.defaultShortcut = QKeySequence(QStringLiteral("Ctrl+Alt+N"));

    commandRegistry->registerCommand(info);
}

bool NotesPlugin::eventFilter(QObject* watched, QEvent* event)
{
    // A QAction is no widget, so Qt's recursive language-change delivery never reaches the action
    // this plugin contributes; qApp announces the change before any top-level window sees it, which
    // is the order the palette needs: it reads the catalog a window republishes in its own pass.
    if (watched == qApp && event->type() == QEvent::LanguageChange)
    {
        retranslateContributions();
        return false;
    }

    auto* window = qobject_cast<QcwWindow*>(watched);
    if (!window || event->type() != QEvent::Show)
    {
        return QObject::eventFilter(watched, event);
    }

    if (!window->resolveHandler(QString(demo::ActionAddNote)))
    {
        auto* handler = new QcwLambdaCommandHandler(window);
        handler->onExecute([this](const QVariantMap&)
        {
            addNoteAndShow();
        });
        window->registerHandler(QString(demo::ActionAddNote), handler);
    }

    QTimer::singleShot(0, window, [window]()
    {
        if (auto* service = demo::notesService())
        {
            service->showNotes(window);
        }
    });

    return QObject::eventFilter(watched, event);
}

void NotesPlugin::addNoteAndShow()
{
    // The front window first: Ctrl+Alt+N reaches whichever window has focus.
    auto* target = qobject_cast<QcwWindow*>(QApplication::activeWindow());
    if (!target)
    {
        for (auto* widget : QApplication::topLevelWidgets())
        {
            if (auto* candidate = qobject_cast<QcwWindow*>(widget); candidate && candidate->isVisible())
            {
                target = candidate;
                break;
            }
        }
    }

    // A note with no window to show it in has nowhere to go; the view is what makes it
    // reachable, so the store is left alone.
    auto* service = demo::notesService();
    if (!target || !service)
    {
        return;
    }

    service->addNote(QString());
    service->showNotes(target);
}

QCW_PLUGIN_IMPLEMENT(NotesPlugin, "Notes")
