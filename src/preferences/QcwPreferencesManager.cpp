// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwPreferencesManager.h"
#include "QcwPreferencesPage.h"
#include <QDebug>
#include <QGlobalStatic>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QcwPreferencesManager, preferencesManagerInstance);

QcwPreferencesManager* QcwPreferencesManager::instance()
{
    return preferencesManagerInstance;
}

bool QcwPreferencesManager::registerPage(QcwPreferencesPage* page)
{
    if (!page)
    {
        return false;
    }

    const auto settingsGroup = page->settingsGroup();
    if (settingsGroup.isEmpty())
    {
        qWarning() << "Preferences page" << page << "has an empty settings group, ignoring it";
        return false;
    }

    if (mPages.contains(settingsGroup))
    {
        // Replacing the page would leave an open preferences window showing the old one,
        // while lookups already resolve to the new one. The first registration wins.
        qWarning() << "A preferences page for group" << settingsGroup << "is already registered, ignoring" << page;
        return false;
    }

    page->setParent(nullptr);
    mPages.insert(settingsGroup, page);

    return true;
}

QcwPreferencesPage* QcwPreferencesManager::getPage(const QString& settingsGroup) const
{
    return mPages.value(settingsGroup);
}

const QMap<QString, QcwPreferencesPage*>& QcwPreferencesManager::allPages() const
{
    return mPages;
}

QT_END_NAMESPACE
