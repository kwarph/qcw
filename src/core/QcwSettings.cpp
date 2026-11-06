// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwSettings.h"

QT_BEGIN_NAMESPACE

namespace
{

/// Built-in provider: the platform's default store, location and format.
QSettings getSettings()
{
    return QSettings();
}

QcwSettingsProvider settingsProvider = getSettings;

} // namespace

void QcwSettings::initialize(QcwSettingsProvider provider)
{
    // First call wins: a later install is ignored, and nullptr freezes the default.
    static bool initialized = false;
    if (!initialized)
    {
        if (provider)
        {
            settingsProvider = provider;
        }
        initialized = true;
    }
}

QSettings QcwSettings::get()
{
    return settingsProvider();
}

QT_END_NAMESPACE
