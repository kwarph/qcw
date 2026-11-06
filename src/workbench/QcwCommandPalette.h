// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QDialog>
#include <QPointer>

QT_BEGIN_NAMESPACE

class QcwWindow;
class QLineEdit;
class QTreeView;
class QStandardItemModel;
class QcwTreeProxyModel;

class QcwCommandPalettePrivate;

/**
 * @class QcwCommandPalette
 * @brief A searchable list of the catalogued commands, opened as a dialog over its window.
 *
 * @details The dialog lists every command catalogued in QcwCommandRegistry, grouped under its
 * category and sorted by label, whether or not a menu shows it — a command the palette is the only
 * way to reach is normal here. Each id resolves through QcwWindow::resolveCommand(), so a row is
 * the window's command QAction and follows the handler behind it: its label, icon, shortcut and
 * enabled state. A command nobody handles stays listed but greyed, and neither the arrow keys nor
 * Enter can select it. The list is built when the dialog opens and on a language change, so a
 * handler that appears or goes away while the palette is up is not re-read.
 *
 * Enter or a double-click closes the dialog and triggers the command on the next event-loop turn,
 * looking the action up again first. The command QAction a window resolves already carries that
 * window's id, so a process-wide command such as Remove This Window gets the window the user
 * meant without the palette knowing anything about it.
 *
 * The palette is owned by its QcwWindow, is window-modal and deletes itself on close;
 * QcwWindow::openCommandPalette() creates it and raises an already open one.
 *
 * @see QcwCommandRegistry, QcwCommandInfo, QcwWindow::openCommandPalette()
 */
class QCW_EXPORT QcwCommandPalette : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Builds the palette for @p window and fills it with the window's commands.
     * @param window The window whose current view decides which commands run; the dialog is
     *               parented to it. Must not be null.
     */
    explicit QcwCommandPalette(QcwWindow* window);
    ~QcwCommandPalette() override;

protected:
    /// Moves the selection with Up/Down/PageUp/PageDown while the filter has focus.
    bool eventFilter(QObject* watched, QEvent* event) override;

    /**
     * @brief Retitles the dialog and rebuilds the list when the UI language changes.
     * @details The category labels come from the command catalog, which the window
     *          republishes before this dialog is reached, so rebuilding the list here picks
     *          up the new language rather than the one the commands were registered in.
     */
    void changeEvent(QEvent* event) override;

private:
    QcwCommandPalettePrivate* impl;
};

QT_END_NAMESPACE
