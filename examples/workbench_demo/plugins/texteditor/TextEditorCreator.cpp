// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "TextEditorCreator.h"

#include "EditorService.h"
#include "PluginInfo.h"
#include "QcwPreferencesManager.h"
#include "QcwServiceRegistry.h"
#include "QcwThemeIcons.h"
#include "QcwTopicRegistry.h"
#include "TextEditor.h"
#include "TextEditorAppearancePreferencesPage.h"

#include <QFileInfo>
#include <QList>

QString TextEditorCreator::defaultWindowTitle() const
{
    return tr("Untitled");
}

QIcon TextEditorCreator::defaultViewIcon() const
{
    return qcw::themeIcon(qcw::Icon::View);
}

void TextEditorCreator::doRegisterContributions()
{
    // The ids and payload keys are the contract; the catalog makes it discoverable.
    const auto registerTopic = [](QLatin1String id, const QString& description, bool stateful,
                                  const QList<QPair<QString, QString>>& payloadKeys)
    {
        QcwTopicInfo info;
        info.id = QString(id);
        info.description = description;
        info.publisher = QStringLiteral("TextEditor");
        info.isStateful = stateful;
        info.payloadKeys = payloadKeys;
        topicRegistry->registerTopic(info);
    };

    registerTopic(demo::TopicEditorSelection,
                  tr("The current editor's cursor and selection state; cleared when the editor hides."), true,
                  { { QString(demo::PathKey), QStringLiteral("QString") },
                    { QString(demo::PositionKey), QStringLiteral("int") },
                    { QString(demo::AnchorKey), QStringLiteral("int") },
                    { QString(demo::RevisionKey), QStringLiteral("int") },
                    { QString(demo::SelectedTextKey), QStringLiteral("QString") } });
    registerTopic(demo::TopicEditorOpened, tr("An editor opened a file."), false,
                  { { QString(demo::PathKey), QStringLiteral("QString") } });
    registerTopic(demo::TopicEditorClosed, tr("An editor closed its file."), false,
                  { { QString(demo::PathKey), QStringLiteral("QString") } });
    registerTopic(demo::TopicEditorSaved, tr("An editor wrote its file to disk."), false,
                  { { QString(demo::PathKey), QStringLiteral("QString") } });
    registerTopic(demo::TopicEditorPathChanged, tr("An editor's file gained a new path, for example after Save As."),
                  false,
                  { { QString(demo::PathKey), QStringLiteral("QString") },
                    { QString(demo::OldPathKey), QStringLiteral("QString") } });

    auto* service = new EditorService;
    if (!serviceRegistry->registerService<demo::IEditorService>(service))
    {
        delete service;
        service = nullptr;
    }

    auto* page = new TextEditorAppearancePreferencesPage;
    if (!preferencesManager->registerPage(page))
    {
        delete page;
        return;
    }

    if (service)
    {
        connect(page, &TextEditorAppearancePreferencesPage::settingsChanged, service,
                &EditorService::applyAppearanceSettings);
    }
}

QcwView* TextEditorCreator::doCreate(QWidget* parent, const QList<QVariant>& args)
{
    auto* editor = new TextEditor { parent };

    const QString filePath = args.isEmpty() ? QString() : args.first().toString();
    if (!filePath.isEmpty() && QFileInfo::exists(filePath))
    {
        editor->loadFile(filePath);
    }

    editor->applyAppearanceSettings();
    editor->setProperty("isEditor", true);

    return editor;
}

QCW_PLUGIN_IMPLEMENT(TextEditorCreator, "TextEditor")
