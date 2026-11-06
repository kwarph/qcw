// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ProjectViewCreator.h"

#include "DemoServices.h"
#include "PluginInfo.h"
#include "ProjectView.h"
#include "QcwThemeIcons.h"
#include "QcwTopicRegistry.h"

QString ProjectViewCreator::defaultWindowTitle() const
{
    return tr("Projects");
}

QIcon ProjectViewCreator::defaultViewIcon() const
{
    return qcw::themeIcon(qcw::Icon::View);
}

bool ProjectViewCreator::singleInstancePerWindow() const
{
    // The project view is the demo's provider of the File commands, and only the current view can
    // answer a command at all, so a second one would show the same project list twice and leave the
    // first one idle.
    return true;
}

void ProjectViewCreator::doRegisterContributions()
{
    QcwTopicInfo info;
    info.id = QString(demo::TopicProjectSelection);
    info.description = tr("The file or folder currently selected in the project tree; cleared when the tree hides.");
    info.publisher = QStringLiteral("ProjectView");
    info.isStateful = true;
    info.payloadKeys = { { QString(demo::PathKey), QStringLiteral("QString") },
                         { QString(demo::IsDirKey), QStringLiteral("bool") } };

    topicRegistry->registerTopic(info);
}

QcwView* ProjectViewCreator::doCreate(QWidget* parent, const QList<QVariant>&)
{
    // The project list lives in QSettings, so the view reopens the projects the last session
    // had rather than taking a folder as a creating argument.
    return new ProjectView { parent };
}

QCW_PLUGIN_IMPLEMENT(ProjectViewCreator, "ProjectView")
