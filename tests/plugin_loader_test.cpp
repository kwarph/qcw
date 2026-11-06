// Checks the plugin directory lookup: the app-local directory always comes first, the
// platform's installed layout follows it, and a list of candidates tolerates the directories
// an application does not use.

#include "QcwPluginLoader.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <cstdio>

namespace
{
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("plugin_loader_test_app"));

    const QDir applicationDir(QCoreApplication::applicationDirPath());
    const QString appLocalPlugins = applicationDir.absoluteFilePath(QStringLiteral("plugins"));
    const QString executableName = QFileInfo(QCoreApplication::applicationFilePath()).completeBaseName();

    const QStringList directories = QcwPluginLoader::defaultPluginDirectories();

    check(!directories.isEmpty(), "the list is not empty");
    check(directories.first() == appLocalPlugins, "the app-local plugins directory comes first");
    check(!directories.contains(QString()), "the list has no empty entries");

    auto sorted = directories;
    sorted.sort();
    sorted.removeDuplicates();
    check(sorted.size() == directories.size(), "the list has no duplicates");

#if defined(Q_OS_UNIX)
    // The test executable sits in PREFIX/bin, so its prefix is the build directory.
    const QString prefix = QDir::cleanPath(applicationDir.absolutePath() + QStringLiteral("/.."));
    const QString libPlugins = QDir(prefix).absoluteFilePath(QStringLiteral("lib/%1/plugins").arg(executableName));
    check(directories.contains(libPlugins), "the Unix private-library layout is a candidate");

#ifdef Q_OS_LINUX
    const QString lib64Plugins = QDir(prefix).absoluteFilePath(QStringLiteral("lib64/%1/plugins").arg(executableName));
    check(directories.contains(lib64Plugins), "the lib64 layout is a candidate");

    // A distribution picks the multiarch triplet, so only an existing one can be discovered.
    const QString tripletRoot = QDir(prefix).absoluteFilePath(QStringLiteral("lib/qcw-test-triplet"));
    const QString tripletPlugins = QDir(tripletRoot).absoluteFilePath(QStringLiteral("%1/plugins").arg(executableName));
    check(QDir().mkpath(tripletPlugins), "the multiarch fixture can be created");

    check(QcwPluginLoader::defaultPluginDirectories().contains(tripletPlugins),
          "an existing multiarch layout is found");

    QDir(tripletRoot).removeRecursively();
#endif
#endif

    // The default list names directories most applications do not have, so the list overload
    // must pass over missing ones instead of reporting each as a failed load.
    const QTemporaryDir emptyDir;
    check(emptyDir.isValid(), "the temporary plugins directory exists");
    check(QcwPluginLoader::loadPlugins(
              QStringList { applicationDir.absoluteFilePath(QStringLiteral("qcw-missing-plugins")), emptyDir.path() }),
          "missing candidates are skipped and an empty directory loads cleanly");

    if (failures != 0)
    {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }

    std::printf("all checks passed\n");
    return 0;
}
