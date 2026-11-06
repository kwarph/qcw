// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwWindow.h"

class QToolBar;

class MainWindow : public QcwWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void postSetupUi() override;
    void retranslateUi() override;
    QcwTabFolder* createSharedFolder() override;

private:
    void addViewMenu();
    void registerFileCommands();
    void registerViewCommands();

    /// Registers the command metadata and its File-menu contribution in one call; the
    /// command's handler is supplied by ProjectView.
    void registerAppFileCommand(const QString& id,
                                const QString& contributionId,
                                const QString& label,
                                QKeySequence shortcut);

    /// Registers the command metadata and its View-menu contribution in one call.
    void registerAppViewCommand(const QString& id,
                                const QString& contributionId,
                                const QString& label,
                                const QString& description);

    /// Builds the demo's tool bars and registers the contributions that fill them. A tool bar
    /// is inserted into registeredToolBars() under the target it answers to, so the window
    /// places the entries its own.
    void addToolBars();

    QToolBar* mMainToolBar { nullptr };
    QToolBar* mEditToolBar { nullptr };
};
