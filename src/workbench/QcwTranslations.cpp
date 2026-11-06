// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTranslations.h"

#include "QcwSettings.h"
#include "QcwTopicRegistry.h"
#include "QcwTopicService.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>
#include <QVariantMap>

#include <map>
#include <memory>

QT_BEGIN_NAMESPACE
namespace {

constexpr QLatin1StringView kQtCatalogBase("qtbase");
constexpr QLatin1StringView kLibraryCatalogBase("qcw");
constexpr QLatin1StringView kSourceLanguage("en");
constexpr QLatin1StringView kTranslationSuffix(".qm");
constexpr QLatin1StringView kLanguageKey("appearance/language");
constexpr QLatin1StringView kLocalePayloadKey("locale");

/// The bases and directories an application registered, in registration order. Plain
/// process-wide data, like the settings provider: nothing owns them, and since no plugin ever
/// unloads there is nothing to unregister either.
QStringList registeredBases;
QStringList registeredDirectories;

/// The locale the last apply installed, which is what currentLanguage() reports.
QString appliedLanguage;

/// One translator per catalog base, kept across languages so a switch reloads them instead of
/// reallocating. Keyed by base and owned here: parenting them to the application would leave
/// the table holding pointers that a destroyed QCoreApplication already freed, and a test
/// process creates more than one application. std::map rather than QMap because QMap::insert()
/// takes its value by reference, which a move-only type cannot satisfy.
std::map<QString, std::unique_ptr<QTranslator>>& catalogTranslators()
{
    static std::map<QString, std::unique_ptr<QTranslator>> translators;
    return translators;
}

/// The bases whose catalogs advertise a language: everything that is loaded except Qt's own.
QStringList listedCatalogBases()
{
    QStringList bases = QcwTranslations::catalogBases();
    bases.removeOne(QString(kQtCatalogBase));
    return bases;
}

void ensureTopicRegistered()
{
    static bool registered = false;
    if (registered)
    {
        return;
    }

    QcwTopicInfo info;
    info.id = QString(QcwLanguageTopic);
    info.description = QStringLiteral(
        "Announces the language that is now installed. Published on every apply: at startup, "
        "from the preferences page and from QcwTranslations::setLanguage(). Carries the resolved "
        "locale, so following the system appears as that system locale rather than as an empty "
        "choice. Never cleared.");
    info.publisher = QStringLiteral("QcwTranslations");
    info.isStateful = true;
    info.payloadKeys = { { QString(kLocalePayloadKey), QStringLiteral("QString") } };
    topicRegistry->registerTopic(info);

    registered = true;
}

/// Installs the catalogs for @p choice and publishes the result; empty follows the system.
void applyChosenLanguage(const QString& choice)
{
    const QLocale locale = choice.isEmpty() ? QLocale::system() : QLocale(choice);
    const QStringList bases = QcwTranslations::catalogBases();
    const QStringList directories = QcwTranslations::catalogDirectories();

    auto& translators = catalogTranslators();
    for (const QString& base : bases)
    {
        auto it = translators.find(base);
        if (it == translators.end())
        {
            it = translators.emplace(base, std::make_unique<QTranslator>()).first;
        }

        auto* translator = it->second.get();

        // Removed before it is loaded, not loaded while installed: QTranslator::load() clears
        // first and clear() posts a LanguageChange for an installed translator, so probing the
        // directories in order would post one event per failed directory per base. A base whose
        // catalog is in no directory simply stays uninstalled, which costs nothing but its own
        // translation.
        QCoreApplication::removeTranslator(translator);
        for (const QString& directory : directories)
        {
            if (translator->load(locale, base, QStringLiteral("_"), directory))
            {
                QCoreApplication::installTranslator(translator);
                break;
            }
        }
    }

    // The name the catalogs are keyed by, not QLocale::name(): a QLocale built from "de"
    // resolves a default territory and would report "de_DE", which no catalog is named after
    // and the user never chose. Only following the system resolves anything.
    appliedLanguage = choice.isEmpty() ? QLocale::system().name() : choice;

    ensureTopicRegistered();
    QVariantMap payload;
    payload.insert(QString(kLocalePayloadKey), appliedLanguage);
    QcwTopicService::forApplication()->publish(QString(QcwLanguageTopic), payload);
}

} // namespace

// -----------------------------------------------------------------------------

void QcwTranslations::registerCatalogBase(const QString& base)
{
    if (!base.isEmpty() && !catalogBases().contains(base))
    {
        registeredBases.append(base);
    }
}

void QcwTranslations::addCatalogDirectory(const QString& directory)
{
    if (!directory.isEmpty() && !registeredDirectories.contains(directory))
    {
        registeredDirectories.append(directory);
    }
}

QStringList QcwTranslations::catalogBases()
{
    QStringList bases { QString(kQtCatalogBase), QString(kLibraryCatalogBase) };
    bases.append(registeredBases);
    return bases;
}

QStringList QcwTranslations::catalogDirectories()
{
    QStringList directories = QStandardPaths::locateAll(
        QStandardPaths::GenericDataLocation, QStringLiteral("qcw/translations"), QStandardPaths::LocateDirectory);

    // Guarded because applicationDirPath() is empty without an application instance, which would
    // leave a relative "translations" in the list and search it against the working directory.
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    if (!applicationDirectory.isEmpty())
    {
        const QString applicationTranslations =
            QDir(applicationDirectory).filePath(QStringLiteral("translations"));
        if (!directories.contains(applicationTranslations))
        {
            directories.append(applicationTranslations);
        }
    }

    for (const QString& directory : registeredDirectories)
    {
        if (!directories.contains(directory))
        {
            directories.append(directory);
        }
    }

    // Qt's own catalogs are the reason QCW ships none: copying them into the repository would
    // freeze them at whichever Qt they were taken from, while this path is always the one the
    // application actually links against.
    const QString qtTranslations = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (!qtTranslations.isEmpty() && !directories.contains(qtTranslations))
    {
        directories.append(qtTranslations);
    }

    return directories;
}

QStringList QcwTranslations::availableLocales()
{
    const QStringList bases = listedCatalogBases();
    QStringList locales;

    for (const QString& directory : catalogDirectories())
    {
        QStringList patterns;
        for (const QString& base : bases)
        {
            patterns << base + QStringLiteral("_*") + QString(kTranslationSuffix);
        }

        const QStringList files = QDir(directory).entryList(patterns, QDir::Files);
        for (const QString& file : files)
        {
            for (const QString& base : bases)
            {
                const QString prefix = base + QLatin1Char('_');
                if (!file.startsWith(prefix) || !file.endsWith(kTranslationSuffix))
                {
                    continue;
                }

                const auto localeLength = file.size() - prefix.size() - kTranslationSuffix.size();
                const QString localeName = file.mid(prefix.size(), localeLength);
                if (!localeName.isEmpty())
                {
                    locales << localeName;
                }
                break;
            }
        }
    }

    // The language the sources are written in has no catalog to discover it from.
    locales << QString(kSourceLanguage);
    locales.removeDuplicates();
    locales.sort();
    return locales;
}

QString QcwTranslations::currentLanguage()
{
    if (!appliedLanguage.isEmpty())
    {
        return appliedLanguage;
    }

    // Before the first apply the stored choice is all there is to go by.
    const QString stored = storedLanguage();
    return stored.isEmpty() ? QLocale::system().name() : stored;
}

QString QcwTranslations::storedLanguage()
{
    return QcwSettings::get().value(kLanguageKey).toString();
}

void QcwTranslations::setLanguage(const QString& localeName)
{
    auto settings = QcwSettings::get();
    if (localeName.isEmpty())
    {
        settings.remove(kLanguageKey);
    }
    else
    {
        settings.setValue(kLanguageKey, localeName);
    }

    applyChosenLanguage(localeName);
}

void QcwTranslations::applyStoredLanguage()
{
    applyChosenLanguage(storedLanguage());
}

QT_END_NAMESPACE
