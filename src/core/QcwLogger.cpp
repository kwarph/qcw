// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwLogger.h"
#include "QcwPathUtils.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <atomic>
#include <ctime>
#include <utility>

QT_BEGIN_NAMESPACE

namespace
{

// A global bool has no destructor, so it stays readable while the LoggerState below is being
// destroyed: Q_GLOBAL_STATIC sets its guard only after the object's own destructor has run, so
// isDestroyed() alone still answers false for the whole of that destructor.
Q_CONSTINIT std::atomic<bool> shuttingDown { false };

/**
 * @brief The logger's mutable state: the log file, the sinks and the mutex guarding them.
 * @details Q_GLOBAL_STATIC is the only way to ask a global whether it is still alive, and the
 * last messages of a run need that answer — QFile, QMutex and QList all lose their contents
 * during static destruction, and writing to or locking them then is what crashed the process.
 */
struct LoggerState
{
    QString directory;           ///< QcwAppDirectories::logDirectory(), once it can be had.
    QFile file;                  ///< The one file this run writes to.
    QList<QcwLoggerSink*> sinks; ///< The registered sinks.
    QMutex mutex;                ///< Guards the three fields above.

    ~LoggerState()
    {
        shuttingDown.store(true, std::memory_order_relaxed);

        QMutexLocker locker { &mutex };
        sinks.clear();
        file.close();
    }
};

Q_GLOBAL_STATIC(LoggerState, state)

/// Whether the state above can still be written to and read from.
bool usable()
{
    return !state.isDestroyed() && !shuttingDown.load(std::memory_order_relaxed);
}

/**
 * @brief The stamp of a line, taken from Qt while it answers and from libc afterwards.
 * @details A QDateTime that never was valid, or one that no longer formats, says Qt's time
 * services are gone. libc needs nothing but itself, and the lines raised then are the ones
 * worth keeping: they describe how the process ended.
 */
QString timestamp(const QDateTime& now, const QString& qtFormat, const char* libcFormat)
{
    const QString text = now.toString(qtFormat);
    if (!text.isEmpty())
    {
        return text;
    }

    const std::time_t seconds = std::time(nullptr);
    std::tm local {};

    // Windows' localtime_s takes the destination first and returns an error code; POSIX's
    // localtime_r takes the source first. Both write into the caller's buffer.
#if defined(Q_OS_WIN)
    if (localtime_s(&local, &seconds) != 0)
#else
    if (localtime_r(&seconds, &local) != 0)
#endif
    {
        return QString();
    }

    char buffer[32] = {};
    const size_t length = std::strftime(buffer, sizeof(buffer), libcFormat, &local);
    return QString::fromLatin1(buffer, static_cast<qsizetype>(length));
}

} // namespace

QtMessageHandler QcwLogger::originalHandler { nullptr };

void QcwLogger::install()
{
    // Kept apart from the state above, which comes into existence on first use and so cannot
    // say whether install() ran: chaining our own handler onto itself would make every message
    // recurse forever.
    static bool installed = false;
    if (installed)
    {
        return;
    }

    installed = true;
    originalHandler = qInstallMessageHandler(handleMessage);
}

void QcwLogger::addSink(QcwLoggerSink* sink)
{
    if (!sink || !usable())
    {
        return;
    }

    QMutexLocker locker { &state->mutex };
    if (!state->sinks.contains(sink))
    {
        state->sinks.append(sink);
    }
}

void QcwLogger::removeSink(QcwLoggerSink* sink)
{
    if (!usable())
    {
        return;
    }

    QMutexLocker locker { &state->mutex };
    state->sinks.removeAll(sink);
}

QString QcwLogger::currentLogFile()
{
    if (!usable())
    {
        return QString();
    }

    QMutexLocker locker { &state->mutex };
    return state->file.isOpen() ? state->file.fileName() : QString();
}

void QcwLogger::handleMessage(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    const char* level = "UNKN";
    switch (type)
    {
    case QtDebugMsg:
        level = "DBUG";
        break;
    case QtInfoMsg:
        level = "INFO";
        break;
    case QtWarningMsg:
        level = "WARN";
        break;
    case QtCriticalMsg:
        level = "CRIT";
        break;
    case QtFatalMsg:
        level = "FATL";
        break;
    }

    // The time is asked of Qt only while Qt lasts, which is also the only span in which the
    // file and the sinks exist at all.
    const bool alive = usable();
    const QDateTime now = alive ? QDateTime::currentDateTime() : QDateTime();
    const QString entry = QString("%1 [%2] %3")
                              .arg(timestamp(now, QStringLiteral("yyyy-MM-dd hh:mm:ss"), "%Y-%m-%d %H:%M:%S"),
                                   QString::fromLatin1(level), msg);

    if (alive)
    {
        QList<QcwLoggerSink*> currentSinks;
        {
            QMutexLocker locker { &state->mutex };

            // QStandardPaths resolves through globals that die before the last messages do, so
            // the answer is taken once and kept. Without a directory no file is attempted: the
            // name alone would point into the root of the filesystem.
            if (state->directory.isEmpty() && QCoreApplication::instance())
            {
                state->directory = QcwAppDirectories::logDirectory();
            }

            bool canWrite = state->file.isOpen();
            if (!canWrite && !state->directory.isEmpty())
            {
                state->file.setFileName(
                    QString("%1/%2.log")
                        .arg(state->directory, timestamp(now, QStringLiteral("yyyyMMddhhmmss"), "%Y%m%d%H%M%S")));
                // A file that will not open loses nothing but the file copy: the message still
                // reaches the console and the sinks below, and the next message retries under a
                // fresh name.
                canWrite = state->file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
            }

            if (canWrite)
            {
                state->file.write(entry.toUtf8() + '\n');
                state->file.flush();
            }

            // Notifying goes outside the lock, because a sink logging from another thread would
            // otherwise deadlock on this non-recursive mutex; the file and the sinks can
            // therefore disagree about the order of two near-simultaneous lines.
            currentSinks = state->sinks;
        }

        for (auto* sink : std::as_const(currentSinks))
        {
            sink->logLine(entry);
        }
    }

    // The console gets the same line as the file, without the newline the default handler
    // adds itself. Forwarding goes last, because the default handler aborts on a fatal
    // message.
    if (originalHandler)
    {
        originalHandler(type, context, entry);
    }
}

QT_END_NAMESPACE
