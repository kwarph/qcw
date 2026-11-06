// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "LogViewCreator.h"

#include "LogView.h"
#include "LogViewAppearancePreferencesPage.h"
#include "PluginInfo.h"
#include "QcwPreferencesManager.h"
#include "QcwThemeIcons.h"

QString LogViewCreator::defaultWindowTitle() const
{
    return tr("Log");
}

QIcon LogViewCreator::defaultViewIcon() const
{
    return qcw::themeIcon(qcw::Icon::View);
}

void LogViewCreator::doRegisterContributions()
{
    auto* page = new LogViewAppearancePreferencesPage;
    if (preferencesManager->registerPage(page))
    {
        connect(page, &LogViewAppearancePreferencesPage::settingsChanged, this, [this]()
        {
            for (auto* view : std::as_const(mViews))
            {
                view->applyAppearanceSettings();
            }
        });
    }
    else
    {
        delete page;
    }
}

QcwView* LogViewCreator::doCreate(QWidget* parent, const QList<QVariant>& args)
{
    Q_UNUSED(args);

    auto* view = new LogView { parent };
    mViews.insert(view);
    connect(view, &QObject::destroyed, this, [this, view]()
    {
        mViews.remove(view);
    });

    return view;
}

QCW_PLUGIN_IMPLEMENT(LogViewCreator, "LogView")
