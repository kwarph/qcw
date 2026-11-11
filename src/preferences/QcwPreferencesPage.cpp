// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwPreferencesPage.h"
#include "QcwSettings.h"

QT_BEGIN_NAMESPACE

QcwPreferencesPage::QcwPreferencesPage(QWidget* parent) :
    QWidget { parent }
{
}

void QcwPreferencesPage::setSettingsProvider(QcwSettingsProvider provider)
{
    mSettingsProvider = provider;
}

QSettings QcwPreferencesPage::settings() const
{
    // A page can be used outside a preferences widget: a test may read it directly, and some
    // pages are constructed long before a widget shows them. Falling back to the process-wide
    // store keeps the pre-scope behaviour for those pages instead of calling an empty
    // std::function.
    return mSettingsProvider ? mSettingsProvider() : QcwSettings::get();
}

QT_END_NAMESPACE
