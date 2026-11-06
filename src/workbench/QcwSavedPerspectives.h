// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QByteArray>
#include <QMap>
#include <QString>

QT_BEGIN_NAMESPACE

class QcwPerspective;

/**
 * @class QcwSavedPerspectives
 * @brief Process-wide store of the perspectives the user saved by name.
 *
 * @details "Save Perspective As..." writes the current perspective through saveState() as
 * `<name>.xml` under `configDirectory()/perspectives`; the file holds the layout and one
 * entry per contained view (id, type, title, creating arguments and extra state), which is
 * what lets Open Perspective recreate those views later. initialize(), called by
 * QcwWorkbench::start(), scans that directory into the in-memory name-to-type map that the
 * Open Perspective dialog lists, and loadState() returns the raw XML that QcwWindow then
 * applies.
 *
 * @note The store is not thread-safe and belongs to the GUI thread.
 * @note This is the single global instance: instance() creates it on first use and always
 * returns the same object. Constructing a second one is pointless because nothing connects to
 * it.
 * @see QcwPerspective, QcwWindow
 */
class QCW_EXPORT QcwSavedPerspectives
{
public:
    /// Returns the process-wide store, creating it on first call.
    static QcwSavedPerspectives* instance();

    /**
     * @brief Reads the perspective files under @c configDirectory()/perspectives.
     * @details Creates the directory when it does not exist and fills the in-memory map from
     * the @c name and @c type attributes of every well-formed file. Called once by
     * QcwWorkbench::start(); calling it again re-reads the directory without clearing the map.
     */
    void initialize();

    /**
     * @brief Writes @p perspective to `configDirectory()/perspectives/<name>.xml`.
     * @return @c true when the file was written, @c false when it could not be opened.
     * @details The in-memory map is not updated; the caller calls addPerspective() as well.
     * @see loadState(), addPerspective()
     */
    bool saveState(QcwPerspective* perspective);

    /**
     * @brief The raw XML saved for @p name.
     * @return The file contents, or an empty QByteArray when no saved perspective has that name.
     */
    QByteArray loadState(const QString& name);

    /// Records @p name as a saved perspective of @p type, for the menus that list them.
    void addPerspective(const QString& name, const QString& type)
    {
        mPerspectives.insert(name, type);
    }

    /// The perspective type saved under @p name, or an empty string.
    QString type(const QString& name) const
    {
        return mPerspectives.value(name);
    }

    /// Whether a saved perspective named @p name was found or added.
    bool exists(const QString& name) const
    {
        return mPerspectives.contains(name);
    }

    /// The saved perspectives, keyed by name; the value is the perspective type.
    const auto& allPerspectives() const
    {
        return mPerspectives;
    }

    /// Number of saved perspectives.
    auto size() const
    {
        return mPerspectives.size();
    }

    /// Whether no saved perspective is known.
    bool empty() const
    {
        return mPerspectives.empty();
    }

private:
    QMap<QString, QString> mPerspectives; // key: name, value: type
};

#ifndef savedPerspectives
#define savedPerspectives QcwSavedPerspectives::instance()
#endif

QT_END_NAMESPACE
