// Which languages exist, where their catalogs come from, and what a switch announces.
//
// Three rules decide this subsystem, and each is easy to break from the other side. The list of
// languages has to come from what the application owns: Qt's installation carries a catalog for
// thirty-odd locales, so listing whatever <base>_<locale>.qm files are reachable advertised
// languages whose QCW strings could never follow. The search order has to put a deployed or
// installed catalog ahead of Qt's own copy, because the vendored one silently disagrees with the
// Qt the application links against. And a switch has to be announced rather than only stored:
// the key is the framework's, so a consumer that read it directly would be coupled to it.
//
// The catalogs this test writes are not catalogs — a name is all the listing rule reads, and a
// load that fails on one must leave the switch working. That doubles as the check that a broken
// file costs nothing but its own translation. Only the part that asks whether a switch really
// installs something reaches for the repository's compiled catalogs, and it is handed the
// directory rather than reading it from where the demo deployment stages it.

#include "QcwSettings.h"
#include "QcwTopicService.h"
#include "QcwTranslations.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariantMap>
#include <cstdio>

namespace {

constexpr QLatin1StringView QtBase("qtbase");
constexpr QLatin1StringView LibraryBase("qcw");
constexpr QLatin1StringView RegisteredBase("capsapp");
constexpr QLatin1StringView RegisteredLocale("de");
constexpr QLatin1StringView QtOnlyLocale("ko");
constexpr QLatin1StringView LibraryLocale("zz");

int gFailures = 0;

void check(bool ok, const char* what, const QString& detail = {})
{
    std::fprintf(stderr, "%s %s%s%s\n", ok ? "ok  " : "FAIL", what, detail.isEmpty() ? "" : " - ",
                 detail.isEmpty() ? "" : qUtf8Printable(detail));
    if (!ok)
    {
        ++gFailures;
    }
}

/// Creates a file named @p fileName under @p directory. The contents are deliberately not a
/// catalog: the listing rule reads names, and loading must survive failing on one.
void writeCatalogFile(const QString& directory, const QString& fileName)
{
    QFile file(QDir(directory).filePath(fileName));
    if (file.open(QIODevice::WriteOnly))
    {
        file.write("not a catalog");
    }
}

/// The catalog name QcwTranslations looks for: <base>_<locale>.qm.
QString catalogName(QLatin1StringView base, QLatin1StringView locale)
{
    return QString(base) + QLatin1Char('_') + QString(locale) + QStringLiteral(".qm");
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryDir store;
    if (!store.isValid())
    {
        std::fprintf(stderr, "FAIL no temporary store for the settings file\n");
        return 1;
    }

    const QString settingsPath = QDir(store.path()).filePath(QStringLiteral("QcwSettings.ini"));
    QcwSettings::initialize([settingsPath]()
    {
        return QSettings(settingsPath, QSettings::IniFormat);
    });

    QTemporaryDir catalogDir;
    if (!catalogDir.isValid())
    {
        std::fprintf(stderr, "FAIL no temporary directory for the catalogs\n");
        return 1;
    }

    const QString catalogs = catalogDir.path();
    writeCatalogFile(catalogs, catalogName(RegisteredBase, RegisteredLocale));
    writeCatalogFile(catalogs, catalogName(LibraryBase, LibraryLocale));
    writeCatalogFile(catalogs, catalogName(QtBase, QtOnlyLocale));
    writeCatalogFile(catalogs, QStringLiteral("capsapp_fr.ts"));

    // --------------------------------------------------------------------- listing

    check(!QcwTranslations::availableLocales().contains(QString(RegisteredLocale)),
          "a directory nobody registered advertises nothing");

    QcwTranslations::registerCatalogBase(QString(RegisteredBase));
    QcwTranslations::addCatalogDirectory(catalogs);

    const QStringList locales = QcwTranslations::availableLocales();
    check(locales.contains(QString(RegisteredLocale)), "a registered base in a registered directory advertises its locale");
    check(locales.contains(QString(LibraryLocale)), "the library's own base still counts");
    check(!locales.contains(QString(QtOnlyLocale)), "Qt's catalogs never advertise a language");
    check(!locales.contains(QStringLiteral("fr")), "a .ts source is not a catalog");
    check(locales.contains(QStringLiteral("en")), "the sources' own language needs no catalog");

    QStringList sorted = locales;
    sorted.sort();
    check(locales == sorted, "the list comes sorted by locale name", locales.join(QStringLiteral(",")));

    // ----------------------------------------------------------------- bases and paths

    const QStringList bases = QcwTranslations::catalogBases();
    check(bases == QStringList { QString(QtBase), QString(LibraryBase), QString(RegisteredBase) },
          "the built-in bases come first, then the registered ones in order", bases.join(QStringLiteral(",")));

    QcwTranslations::registerCatalogBase(QString(RegisteredBase));
    QcwTranslations::registerCatalogBase(QString());
    QcwTranslations::registerCatalogBase(QString(LibraryBase));
    QcwTranslations::addCatalogDirectory(catalogs);
    QcwTranslations::addCatalogDirectory(QString());
    check(QcwTranslations::catalogBases() == bases, "a base re-registered, an empty one and a built-in change nothing");

    const QStringList directories = QcwTranslations::catalogDirectories();
    const QString qtDirectory = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    check(!qtDirectory.isEmpty() && directories.contains(qtDirectory),
          "Qt's own translations directory closes the search", qtDirectory);
    check(directories.indexOf(catalogs) >= 0 && directories.indexOf(catalogs) < directories.indexOf(qtDirectory),
          "a registered directory is searched before Qt's");
    check(QcwTranslations::catalogDirectories() == directories, "adding a directory twice changes nothing");

    // ---------------------------------------------------------------- the announcement

    auto* topics = QcwTopicService::forApplication();
    int announcements = 0;
    QString announced;
    topics->subscribe(
        QString(QcwLanguageTopic), &app, [&announcements, &announced](const QVariantMap& payload)
        {
            ++announcements;
            announced = payload.value(QStringLiteral("locale")).toString();
        });

    check(announcements == 0, "the topic has not been published yet");

    QcwTranslations::setLanguage(QString(RegisteredLocale));
    check(QcwTranslations::currentLanguage() == QString(RegisteredLocale), "the applied locale is the chosen one");
    check(QcwTranslations::storedLanguage() == QString(RegisteredLocale), "the choice is kept as it was made");
    check(QcwSettings::get().value(QStringLiteral("appearance/language")).toString() == QString(RegisteredLocale),
          "the choice is stored under the framework's own key");
    check(announcements == 1 && announced == QString(RegisteredLocale),
          "a switch is announced, with the catalogs of a broken file loaded and dropped");

    // The replay a late consumer relies on: subscribing now reports the language that is already
    // installed, without a separate current() call.
    int lateAnnouncements = 0;
    QString lateLocale;
    topics->subscribe(
        QString(QcwLanguageTopic), &app, [&lateAnnouncements, &lateLocale](const QVariantMap& payload)
        {
            ++lateAnnouncements;
            lateLocale = payload.value(QStringLiteral("locale")).toString();
        });
    check(lateAnnouncements == 1 && lateLocale == QString(RegisteredLocale),
          "a consumer that arrives after the switch still gets the cached payload");

    QcwTranslations::setLanguage(QString());
    check(announcements == 2, "following the system is announced too");
    check(QcwTranslations::storedLanguage().isEmpty(), "the empty choice is stored as no choice");
    check(!QcwSettings::get().contains(QStringLiteral("appearance/language")),
          "the key is removed, so a moved system locale is followed on the next start");
    check(announced == QLocale::system().name(), "the announcement carries the resolved locale, not the empty choice",
          announced);

    // ------------------------------------------------------------------ what gets loaded

    // The repository's own catalog directory, registered the way an application registers its
    // own: this is the only place a *real* compiled catalog comes from, so what follows proves
    // that a switch installs one rather than only advertising a name. It is passed in as a
    // definition rather than read from the directory the demo deployment stages beside the
    // executables, because building this test alone does not stage anything.
    const QString repositoryCatalogs = QStringLiteral(QCW_REPO_TRANSLATIONS_DIR);
    QcwTranslations::addCatalogDirectory(repositoryCatalogs);

    QcwTranslations::setLanguage(QStringLiteral("zh_CN"));

    const QString systemDefault =
        QCoreApplication::translate("QcwAppearancePreferencesPage", "System Default");
    check(systemDefault != QStringLiteral("System Default"), "a registered directory installs a real catalog",
          systemDefault);

    // What dropping Qt's vendored catalogs relies on: nothing in this repository shadows them, so
    // the qtbase base is loaded from the Qt installation the application links against — which
    // catalogDirectories() above shows closes the search. Whether that installation carries the
    // files is the platform's business, so it is not asserted here.
    const QStringList vendored = QDir(repositoryCatalogs).entryList({ QStringLiteral("qtbase_*.qm") }, QDir::Files);
    check(vendored.isEmpty(), "no copy of Qt's catalogs is vendored", vendored.join(QStringLiteral(",")));

    QcwTranslations::setLanguage(QStringLiteral("en"));
    check(QCoreApplication::translate("QcwAppearancePreferencesPage", "System Default")
              == QStringLiteral("System Default"),
          "switching back to the source language uninstalls the catalog");

    // ------------------------------------------------------------------ the stored choice

    const int announcedBeforeApplying = announcements;
    {
        auto settings = QcwSettings::get();
        settings.setValue(QStringLiteral("appearance/language"), QStringLiteral("de_DE"));
    }

    QcwTranslations::applyStoredLanguage();
    check(QcwTranslations::currentLanguage() == QStringLiteral("de_DE"),
          "applyStoredLanguage() follows what is stored, without writing anything");
    check(announcements == announcedBeforeApplying + 1 && announced == QStringLiteral("de_DE"),
          "and it announces the resolved locale as well");
    check(QcwSettings::get().value(QStringLiteral("appearance/language")).toString() == QStringLiteral("de_DE"),
          "the stored choice survives an apply");

    std::fprintf(stderr, "%s\n", gFailures == 0 ? "all checks passed" : "checks failed");
    return gFailures == 0 ? 0 : 1;
}
