// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QMainWindow>
#include <QSet>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class PreferencesDialog;
class QcwTabFolder;

/// Main window of the renderer demo: one tab folder, a few plain-text panes and a
/// preferences dialog that swaps the folder's renderer and theme at run time.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    /// Installs the theme-matching renderer on @p folder and gives it the dialog's theme data.
    void applyTheme(QcwTabFolder* folder);
    /// Installs @p folder's tab close and transfer handlers; folders already known are skipped.
    void connectFolder(QcwTabFolder* folder);
    /// Opens @p filePath as a tab unless it is open already.
    void openFile(const QString& filePath);
    /// Writes the pane at @p index of @p folder back to its file. False on any failure.
    bool saveFile(QcwTabFolder* folder, int index);
    void prepareActions();

private:
    Ui::MainWindow* ui;
    PreferencesDialog* preferences;
    QSet<QString> openedFiles;
    QSet<QcwTabFolder*> folders;
    bool isDefaultStyle { false };
};
