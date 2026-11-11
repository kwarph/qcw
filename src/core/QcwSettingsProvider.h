// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QSettings>
#include <functional>

QT_BEGIN_NAMESPACE

/**
 * @brief Factory for settings objects: one way to describe a settings store.
 *
 * @details The same type is used at two scopes:
 * - QcwSettings::initialize() stores the process-wide provider behind QcwSettings::get().
 * - QcwPreferencesWidget hands a provider to the pages it displays, so those pages can edit a
 *   project's store instead of the global one. QcwPreferencesPage::settings() is how a page
 *   asks for it.
 *
 * The type is a std::function, not a function pointer, because a provider may capture the
 * description of the store it opens — for example the path of a project's INI file. It is
 * copied into each owner, so a caller may pass a temporary lambda. The state the lambda
 * captures must outlive every object that holds the provider: a page owned by
 * QcwPreferencesManager lives until the process exits.
 *
 * Each call must return a fresh QSettings by value, and the provider must be safe to call
 * from every thread that may reach it. An empty provider is not a factory; each owner decides
 * what it means — QcwSettings::initialize() treats it as "keep the default", while
 * QcwPreferencesPage::settings() falls back to QcwSettings::get().
 */
using QcwSettingsProvider = std::function<QSettings()>;

QT_END_NAMESPACE
