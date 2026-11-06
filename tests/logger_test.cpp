// Checks one QcwLogger line from all three sides: the file, a sink and the level tag. Covers
// the lines that predate a sink, repeated registration, removal, and the stamp a line has to
// keep however late in the run it is raised.

#include "QcwLogger.h"
#include "QcwPathUtils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <cstdio>
#include <vector>

namespace
{

class RecordingSink : public QcwLoggerSink
{
public:
    void logLine(const QString& line) override
    {
        lines.append(line);
    }

    QStringList lines;
};

bool check(bool condition, const char* what)
{
    if (condition)
    {
        return true;
    }

    std::fprintf(stderr, "%s: false\n", what);
    return false;
}

bool check(const QString& actual, const QString& expected, const char* what)
{
    if (actual == expected)
    {
        return true;
    }

    std::fprintf(stderr, "%s = \"%s\", expected \"%s\"\n", what, actual.toLocal8Bit().constData(),
                 expected.toLocal8Bit().constData());
    return false;
}

// The lines are looked up by the message at their end rather than by position, so that a
// message from Qt itself cannot make the test disagree with the logger.
qsizetype indexOf(const QStringList& lines, const QString& message)
{
    for (qsizetype i = 0; i < lines.size(); ++i)
    {
        if (lines.at(i).endsWith(message))
        {
            return i;
        }
    }
    return -1;
}

QString lineFor(const QStringList& lines, const QString& message)
{
    const qsizetype index = indexOf(lines, message);
    return index < 0 ? QString() : lines.at(index);
}

qsizetype occurrences(const QStringList& lines, const QString& message)
{
    qsizetype count = 0;
    for (qsizetype i = 0; i < lines.size(); ++i)
    {
        if (lines.at(i).endsWith(message))
        {
            ++count;
        }
    }
    return count;
}

/**
 * @brief Whether @p line is the logger's own composition of @p message at level @p level.
 * @details The stamp has to be recent as well as well shaped: a fallback to 1970 or a stamp
 * taken from a destroyed QDateTime would otherwise pass on shape alone.
 */
bool isWellFormed(const QString& line, const QString& message, const char* level)
{
    const QRegularExpression pattern {
        QStringLiteral("^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2} \\[%1\\] .*$").arg(QLatin1String(level))
    };
    if (!pattern.match(line).hasMatch() || !line.endsWith(message))
    {
        return false;
    }

    const QDateTime stamped = QDateTime::fromString(line.left(19), QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    return qAbs(stamped.secsTo(QDateTime::currentDateTime())) <= 60;
}

QStringList readFile(const QString& filePath)
{
    QFile file { filePath };
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }

    QStringList lines;
    while (!file.atEnd())
    {
        QString line = QString::fromUtf8(file.readLine());
        if (line.endsWith(QLatin1Char('\n')))
        {
            line.chop(1);
        }
        lines.append(line);
    }
    return lines;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // Test mode maps the standard locations under ~/.qttest, which keeps the log of this run
    // out of the developer's real data directory.
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("qcw-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("logger_test"));

    const QString logDir = QcwAppDirectories::logDirectory();
    if (!QDir().mkpath(logDir))
    {
        std::fprintf(stderr, "cannot create %s\n", logDir.toLocal8Bit().constData());
        return 1;
    }

    // Two runs started in the same second would share a file name and append to each other.
    QDir pruned { logDir };
    for (const QString& name : pruned.entryList({ QStringLiteral("*.log") }, QDir::Files))
    {
        pruned.remove(name);
    }

    QcwLogger::install();

    bool ok = true;

    RecordingSink early;
    QcwLogger::addSink(&early);
    ok &= check(QcwLogger::currentLogFile(), QString(), "no file is open before the first message");

    const QString firstMessage = QStringLiteral("logged before the second sink");
    qInfo().noquote() << firstMessage;

    const QString filePath = QcwLogger::currentLogFile();
    ok &= check(!filePath.isEmpty(), "the first message opens the file");

    RecordingSink second;
    QcwLogger::addSink(&second);
    QcwLogger::addSink(&second);

    const QString warningMessage = QStringLiteral("a warning");
    qWarning().noquote() << warningMessage;
    const QString debugMessage = QStringLiteral("a debug line");
    qDebug().noquote() << debugMessage;
    const QString criticalMessage = QStringLiteral("a critical line");
    qCritical().noquote() << criticalMessage;

    ok &= check(lineFor(early.lines, firstMessage) == lineFor(readFile(filePath), firstMessage),
                "the sink and the file hold the identical line");
    ok &= check(lineFor(second.lines, firstMessage).isEmpty(), "a sink added afterwards misses the earlier line");
    ok &= check(occurrences(second.lines, warningMessage) == 1, "the same sink twice still hears once");
    ok &= check(isWellFormed(lineFor(early.lines, firstMessage), firstMessage, "INFO"), "the INFO line is well formed");
    ok &= check(isWellFormed(lineFor(second.lines, warningMessage), warningMessage, "WARN"),
                "the WARN line is well formed");
    ok &= check(lineFor(early.lines, warningMessage) == lineFor(second.lines, warningMessage),
                "two sinks see the identical line");

    QcwLogger::removeSink(&early);
    QcwLogger::removeSink(&early); // Removing a sink that is gone is ignored.
    const QString afterRemovalMessage = QStringLiteral("logged after a sink went away");
    qInfo().noquote() << afterRemovalMessage;
    ok &= check(lineFor(early.lines, afterRemovalMessage).isEmpty(), "a removed sink hears nothing more");
    ok &= check(isWellFormed(lineFor(second.lines, afterRemovalMessage), afterRemovalMessage, "INFO"),
                "the remaining sink still hears");

    QcwLogger::addSink(nullptr); // Ignored, not a crash.

    const QStringList fileLines = readFile(filePath);
    const QStringList messages = { firstMessage, warningMessage, debugMessage, criticalMessage, afterRemovalMessage };
    const std::vector<const char*> levels = { "INFO", "WARN", "DBUG", "CRIT", "INFO" };
    qsizetype previousIndex = -1;
    for (qsizetype i = 0; i < messages.size(); ++i)
    {
        const QString line = lineFor(fileLines, messages.at(i));
        ok &= check(isWellFormed(line, messages.at(i), levels.at(static_cast<size_t>(i))),
                    "the file holds the message with its own level");
        ok &= check(occurrences(fileLines, messages.at(i)) == 1, "the file holds it exactly once");

        const qsizetype index = indexOf(fileLines, messages.at(i));
        ok &= check(index > previousIndex, "the file keeps the order of the run");
        previousIndex = index;
    }

    QcwLogger::removeSink(&second); // The contract every sink owes: gone before it is destroyed.
    QFile::remove(filePath);
    QDir().rmdir(logDir);
    std::fprintf(stderr, "logger_test=%d\n", ok);
    return ok ? 0 : 1;
}
