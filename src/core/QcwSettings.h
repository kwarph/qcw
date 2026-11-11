// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include "QcwSettingsProvider.h"

QT_BEGIN_NAMESPACE

/**
 * @class QcwSettings
 * @brief Process-wide access to the application's settings store.
 *
 * @details QcwSettings does not prescribe a store. get() delegates to a provider function;
 * until one is installed, the provider default-constructs QSettings, which selects
 * QSettings::defaultFormat() and derives the location from the organization and application
 * names. On Windows that is the registry, on macOS a property list and on Unix an INI file
 * under the per-user configuration directory.
 *
 * Installing a provider is optional: as long as nobody calls initialize(), get() returns
 * that default QSettings. Applications that need a specific store install their own provider;
 * QcwWorkbench-based applications do so to keep every setting in the single INI file
 * <tt>configDirectory()/QcwSettings.ini</tt>. QcwPreferencesWidget exports and imports the
 * store behind the provider it was built with — this one for the workbench's default dialog —
 * converting to and from INI as needed.
 *
 * @par Usage
 * @code
 * auto settings = QcwSettings::get();
 * settings.setValue(QLatin1String("appearance/themes/currentTheme"), name);
 * // no sync() needed: the destructor flushes, and the event loop does so periodically
 * @endcode
 *
 * @note get() deliberately returns by value. QSettings is reentrant — distinct objects are
 * safe in different threads even when they wrap the same store, while one shared instance
 * would not be. Qt documents constructing and destroying one as "very fast" because the
 * parsed settings are cached process-wide, so each fresh object re-reads nothing from disk
 * and still sees the changes made through any other object.
 *
 * @see QcwAppDirectories::configDirectory(), QcwWorkbench::initialize()
 */

struct QCW_EXPORT QcwSettings
{
    /**
     * @brief Installs @p provider as the source of all later get() calls.
     * @param provider Factory to use; nullptr keeps the built-in default and only freezes
     *        that choice.
     *
     * @details Only the first call has an effect; later calls are ignored, so a provider the
     * application installs before QcwWorkbench::initialize() takes precedence over the
     * workbench's INI provider. Until initialize() is called, get() keeps returning
     * default-constructed QSettings objects. Passing nullptr records the call without
     * changing the provider, which freezes the default.
     *
     * @note Call it during startup, before other threads use get(); the installed provider
     * is a plain global and this function does no synchronization.
     */
    static void initialize(QcwSettingsProvider provider);

    /**
     * @brief A settings object produced by the installed provider.
     * @details Build one per scope of use; changes reach the backing store when it goes out
     * of scope. Without initialize(), this is a default-constructed QSettings.
     */
    static QSettings get();
};

QT_END_NAMESPACE
