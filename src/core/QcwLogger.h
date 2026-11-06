// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QString>
#include <QtGlobal>

QT_BEGIN_NAMESPACE

/**
 * @class QcwLoggerSink
 * @brief Receives the lines QcwLogger composes for the log file.
 *
 * @details A sink sees each line as the logger composed it — timestamp, level tag and message,
 * without the trailing newline — which is the text the file holds whenever the file is open.
 * addSink() only affects messages logged afterwards; read QcwLogger::currentLogFile() for what
 * was written before the sink was added.
 *
 * @note logLine() runs on the thread that raised the message, which is the GUI thread for QCW's
 * own logging. A sink that touches GUI widgets has to marshal the line itself. A sink that logs
 * is not heard by this class again: Qt answers that nested message with its own default handler
 * instead of re-entering the one installed here. The sink must be removed before it is
 * destroyed.
 *
 * @see QcwLogger
 */
class QCW_EXPORT QcwLoggerSink
{
public:
    virtual ~QcwLoggerSink() = default;

    /// Shows one line; it is identical to the line written to the log file.
    virtual void logLine(const QString& line) = 0;
};

/**
 * @class QcwLogger
 * @brief Writes every qDebug()/qWarning()/qCritical() message of the process to a log file,
 *        without swallowing the console output.
 *
 * @details Qt allows one message handler per process to replace the built-in one, so
 * installing QcwLogger takes the logging over entirely. install() therefore remembers the
 * handler it replaces and hands each message to it after writing the file, so the console
 * keeps its output and the abort() that a fatal message performs still runs. The console
 * receives the same composed line as the file, not the bare message: a terminal and the
 * log file never disagree about a line's timestamp or level.
 *
 * @par What a line looks like
 * @code
 * 2026-10-17 09:41:07 [INFO] Current theme: Dark
 * @endcode
 * The timestamp is local time to the second, then a four-letter level tag, then the message
 * exactly as the caller wrote it; the console and every sink print that line unchanged. The
 * @c context of the message (source file, line, category) is deliberately left out: it
 * clutters the lines and goes stale whenever the code is edited.
 *
 * @par One file per run
 * The file is named after the moment logging started, @c yyyyMMddhhmmss.log inside
 * QcwAppDirectories::logDirectory(), and stays open for the life of the process; every line
 * is flushed immediately, so a crash does not cost the tail of the log. Deleting the
 * accumulated files is the workbench's job, not the logger's.
 *
 * @par The last messages of a run
 * Qt hands a message handler to code that outlives the application object: the destructors of
 * the library's and Qt's own globals still log, and by then @c QStandardPaths and @c QDateTime
 * answer nothing or crash. The log directory is therefore resolved once, on the first message
 * raised while the application object exists, and the timestamp of a line whose time Qt would
 * not format comes from libc instead. A message raised after this class' own state began to be
 * destroyed reaches the console alone, rather than locking a destroyed mutex.
 *
 * @note The class is all static state and is entered from whichever thread raised the
 * message; QCW logs from the GUI thread.
 *
 * @see QcwWorkbench, QcwAppDirectories::logDirectory(), qInstallMessageHandler()
 */
class QCW_EXPORT QcwLogger
{
public:
    /**
     * @brief Routes the process's log messages to a file, keeping the console output.
     * @details Safe to call more than once: the second call does nothing instead of chaining
     *          the handler onto itself and writing every line twice.
     * @note The first message opens the file, and to name it the logger needs
     *       QcwAppDirectories::logDirectory() — which needs the application object, its
     *       organization and application names, and a directory it created. QcwWorkbench's
     *       initialize() sets all three up and then installs this handler, which is the
     *       ordering that works; a message raised before them reaches the console and the sinks
     *       only.
     */
    static void install();

    /**
     * @brief Registers @p sink for the lines logged from now on.
     * @details Adding the same sink twice has no further effect. The logger borrows the
     *          pointer; the caller removes it again before destroying the sink.
     */
    static void addSink(QcwLoggerSink* sink);

    /// Removes @p sink; a sink that was never added is ignored.
    static void removeSink(QcwLoggerSink* sink);

    /**
     * @brief The log file the logger is writing, or an empty string while none is open.
     * @details The file is opened by the first message and kept open for the rest of the
     *          run. Reading it and then calling addSink() gives a sink the complete output,
     *          including the lines that predate it.
     */
    static QString currentLogFile();

private:
    /**
     * @brief The message handler itself; see install().
     * @param type Kind of message; becomes the level tag.
     * @param context Source location of the message. Unused, as the class note explains.
     * @param msg The message the caller passed to qDebug() and friends, already formatted.
     */
    static void handleMessage(QtMsgType type, const QMessageLogContext& context, const QString& msg);

private:
    static QtMessageHandler originalHandler; ///< The handler install() replaced, and forwards to.
};

QT_END_NAMESPACE
