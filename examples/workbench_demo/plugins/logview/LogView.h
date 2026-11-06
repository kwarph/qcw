// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwLogger.h"
#include "QcwView.h"
#include "QcwCommandHandler.h"

#include <QHash>
#include <QMap>
#include <QMetaObject>
#include <QPair>
#include <QString>

class QAction;
class QMenu;
class QPlainTextEdit;
class QToolBar;

/**
 * @brief A read-only pane that shows the process log.
 *
 * The view seeds itself with the current QcwLogger file and then appends every new line, so
 * what it shows matches the log file: timestamp, level and message. Each line is drawn in the
 * color the current theme's LogView.theme gives its level, and the pane recolors its lines when
 * the theme changes. Its top-right tool bar clears the pane and writes its content to a .log
 * file. The context menu is the pane's own: setupHandlers() wraps its Copy and Select All
 * actions as command handlers, and those are what the window's Edit menu and tool bar run while
 * the log is the current view. The pane's font is
 * the one its appearance page stores, or the system's fixed font when none is stored.
 */
class LogView : public QcwView, public QcwLoggerSink
{
    Q_OBJECT
public:
    explicit LogView(QWidget* parent = nullptr);
    ~LogView() override;

    /// Re-reads the log view settings group and applies it to this pane.
    void applyAppearanceSettings();

    QToolBar* topRightToolBar() const override;
    QHash<QString, QcwCommandHandler*> commandHandlers() const override;
    void logLine(const QString& line) override;

protected:
    void retranslateUi() override;

private:
    void translateUi();
    void appendLine(const QString& line);
    void applyThemeColors();
    void scrollToEnd();
    void loadExistingLines();
    void updateEditActions();
    void clearLog();
    void saveLog();
    void setupHandlers();

    QPlainTextEdit* mEditor;
    QMenu* mContextMenu;
    QToolBar* mToolBar;
    QAction* mClearAction;
    QAction* mSaveAction;
    QString mLastSavePath;
    QMetaObject::Connection mThemeConnection;
    QHash<QString, QcwCommandHandler*> mHandlers;
};
