// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"

#include <QObject>
#include <QVariantMap>

QT_BEGIN_NAMESPACE

/**
 * @class QcwCommandHandler
 * @brief The implementation behind one command: what runs, and whether it can run.
 *
 * @details A handler is registered for a command id at one of four scopes — view,
 * perspective, window or workbench — and QcwWindow resolves the same id by asking the
 * scopes in that order. The first handler found wins, so the same command (Edit/Copy, for
 * example) can copy text in one view and copy a file path in another.
 *
 * @par What the handler owns
 * - execute() — the command body; may be empty for a command that only needs a shortcut or a
 *   checkbox.
 * - isEnabled() / isVisible() — drive the command QAction's state.
 * - isChecked() / setChecked() — drive and follow the QAction's checked state for checkable
 *   commands; QcwCommandInfo::checkable says whether the command is one.
 *
 * @par Execution order for checkable commands
 * The command QAction toggles first, then the window calls setChecked(action->isChecked()),
 * then execute(). setChecked() is a pre-sync and must not emit stateChanged(); a handler
 * that changes its business state elsewhere emits stateChanged() and the window re-reads
 * isChecked().
 *
 * @note Handlers are QObjects so stateChanged() can be connected, and so destroyed()
 * auto-unregisters a handler its owner forgot to remove.
 */
class QCW_EXPORT QcwCommandHandler : public QObject
{
    Q_OBJECT
public:
    using Params = QVariantMap;

    explicit QcwCommandHandler(QObject* parent = nullptr);

    /// Whether the command can execute in the current state. Drives the QAction's enabled
    /// flag, and the command palette greys out — but still lists — a command that answers false.
    virtual bool isEnabled() const
    {
        return true;
    }

    /// Whether the command has an entry in the menus and toolbars that contribute it. Hidden
    /// also means silent: Qt disables an invisible QAction, so its shortcut stops answering.
    /// The command palette does not consult this; it lists every catalogued command.
    /// A command no scope handles is not hidden for that reason — it stays where it is
    /// contributed and is greyed out, since which entries exist is the contribution's decision.
    virtual bool isVisible() const
    {
        return true;
    }

    /// Whether the command is checked; only read for checkable commands.
    virtual bool isChecked() const
    {
        return false;
    }

    /// Pre-syncs the checked state before execute(); must not emit stateChanged().
    virtual void setChecked(bool checked)
    {
        Q_UNUSED(checked);
    }

    /// The command body.
    virtual void execute(const Params& params) = 0;

    /// Whether this handler accepts @p params; used only by the caller, not by resolution.
    virtual bool accepts(const Params& params) const
    {
        Q_UNUSED(params);
        return true;
    }

    /// Manually trigger a stateChanged event to notify the window to update QAction when the state within the view
    /// changes.
    void notifyStateChanged()
    {
        emit stateChanged();
    }

signals:
    /// isEnabled(), isVisible() or isChecked() may have changed.
    void stateChanged();
};

QT_END_NAMESPACE
