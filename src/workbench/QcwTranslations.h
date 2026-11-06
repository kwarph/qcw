// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"

#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE

/// The application-scope topic that carries the applied language; see QcwTranslations.
inline constexpr QLatin1StringView QcwLanguageTopic { "app/language" };

/**
 * @class QcwTranslations
 * @brief The catalogs a language is made of, and the subsystem that applies one.
 *
 * @details A language is not one file: it is the set of catalogs named
 * <tt><i>base</i>_<i>locale</i>.qm</tt> that QcwTranslations loads together. Two bases are
 * built in — @c qtbase for Qt's own strings and @c qcw for this library's and the demos' —
 * and an application adds its own with registerCatalogBase(), so switching the language in the
 * preferences page installs its catalogs the same way it installs QCW's. An application that
 * keeps its catalogs somewhere else points at them with addCatalogDirectory().
 *
 * Directories are searched in this order, and a base is loaded from the first one that has it:
 * the installed @c qcw/translations data directory, @c translations beside the executable, the
 * registered directories, and Qt's own translations directory from
 * QLibraryInfo::path(QLibraryInfo::TranslationsPath). The last of those is why QCW ships no
 * copy of Qt's catalogs: Qt already installs them, and a vendored copy would silently disagree
 * with the Qt the application links against.
 *
 * @par Which languages are offered
 * availableLocales() lists the locales that have a catalog under an application-owned base —
 * @c qcw and the registered ones — plus @c en, the language the sources are written in, which
 * needs no catalog at all. Qt's base is deliberately left out of the listing: the Qt
 * installation carries a catalog for every language Qt supports, and offering them would
 * advertise a UI that QCW's own strings cannot follow.
 *
 * @par Usage
 * @code
 * // Once at startup, before the language is applied:
 * QcwTranslations::registerCatalogBase(QStringLiteral("notesapp"));
 * QcwTranslations::addCatalogDirectory(QStringLiteral(":/notesapp/translations"));
 *
 * // Anywhere that needs to follow the language:
 * auto* topics = QcwTopicService::forApplication();
 * topics->subscribe(QcwLanguageTopic, this, [this](const QVariantMap& payload) {
 *     retranslateForLocale(payload.value(QStringLiteral("locale")).toString());
 * });
 * @endcode
 *
 * @note Loading an application's catalogs is only half of a language switch: strings taken in
 * a constructor do not change because a translator appeared. Re-apply them in
 * QEvent::LanguageChange, as QcwView::retranslateUi() and QcwWindow::retranslateUi() do for the
 * framework's own.
 * @note @c appearance/language holds the choice; an empty value follows the system. The key
 * belongs to this subsystem — a host application should read currentLanguage() or subscribe to
 * QcwLanguageTopic instead.
 * @note The catalogs and the registered names are process-wide state, read and written on the
 * GUI thread. Not thread-safe; register during startup, before the first apply.
 *
 * @see QcwAppearancePreferencesPage, QcwTopicService
 */
struct QCW_EXPORT QcwTranslations
{
    /**
     * @brief Adds @p base to the set of catalogs a language consists of.
     * @param base The file-name base of the application's catalogs, without the locale suffix.
     * @details Ignored when empty or already known, so a plugin that loads twice registers once.
     * A base has to be known before the language is applied; QcwWorkbench::initialize() applies
     * the stored language once for its own chrome and again when a plugin has registered a base
     * while loading, so doRegisterContributions() is a valid place for one.
     */
    static void registerCatalogBase(const QString& base);

    /**
     * @brief Adds @p directory to the search path for every catalog base.
     * @param directory A directory holding <tt><i>base</i>_<i>locale</i>.qm</tt> files.
     * @details Searched after the installed data directory and after @c translations beside the
     * executable, so a deployed or installed catalog still wins, and before Qt's own directory.
     * Ignored when empty or already known.
     */
    static void addCatalogDirectory(const QString& directory);

    /// Every base that is loaded for a language: the built-ins and the registered ones.
    static QStringList catalogBases();

    /// Every directory that is searched, in search order.
    static QStringList catalogDirectories();

    /// The locales worth offering in a language selector, sorted by locale name.
    static QStringList availableLocales();

    /**
     * @brief The locale currently in effect, resolved and never empty.
     * @details The value setLanguage() chose, or the system locale when the choice is empty.
     * Before the first apply it answers from the stored choice, or from the system locale when
     * nothing is stored, so it is meaningful before QcwWorkbench::initialize() has run.
     */
    static QString currentLanguage();

    /**
     * @brief The choice as it is stored; empty means the application follows the system.
     * @details What a language selector preselects. currentLanguage() is the resolved locale and
     * cannot tell "System Default" apart from the locale that happened to resolve to.
     */
    static QString storedLanguage();

    /**
     * @brief Persists @p localeName, applies it and publishes QcwLanguageTopic.
     * @param localeName A locale name such as @c zh_CN; empty follows the system.
     * @details This is the entry point for a user's choice. The value is stored as chosen, so
     * following the system is restored as following the system rather than as a frozen locale.
     */
    static void setLanguage(const QString& localeName);

    /**
     * @brief Applies the stored language without writing anything back.
     * @details What QcwWorkbench::initialize() calls through
     * QcwAppearancePreferencesPage::applyStoredSettings() before windows exist, and again after
     * the plugins have loaded when one of them registered a catalog base. A program without a
     * workbench calls it itself, once its catalogs are registered.
     */
    static void applyStoredLanguage();
};

QT_END_NAMESPACE
