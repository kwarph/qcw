// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QMainWindow>
#include <QSet>

namespace Ui
{
class MainWindow;
}

class QcwTabFolder;
class QcwTabFolderContainer;

/// Main window of the folder container demo: a QcwTabFolderContainer holding three
/// folders, plain-text panes opened from disk, and a save/restore of the whole layout.
///
/// The layout is never written or read on its own: File > Save State produces state.xml next to
/// the executable and File > Restore State reads it back, so the two directions of the
/// serialization stay visible as an explicit action rather than as silent start-up behaviour.
///
/// File > Docking in a Dialog... opens the same container inside a QDialog, which is the plain
/// Qt host a container needs: it docks, splits and floats there as it does in the window, and
/// only folder minimize is switched off, because a minimized folder is handed to the host's
/// QToolBar area and a dialog has none.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    using TabPaneCreator = QWidget* (*)(const QList<QVariant>& creatingArgs);

    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void createInitialLayout();
    void prepareActions();

    /// Creates a pane through the creator registered for @p paneType, or nullptr if there
    /// is no creator or the creator could not build the widget.
    QWidget* createPane(const QString& paneType, const QList<QVariant>& creatingArgs);
    /// Opens @p filePath as a tab in the central folder. False when the file could not be read.
    bool openFile(const QString& filePath);

    /// Shows a modal dialog hosting a container of its own, and closes it when @p dialogDone
    /// comes back false.
    void openDockingDialog();

    /// Writes the pane at @p index of @p folder back to its file. False on any failure, whose
    /// message is appended to @p error when given.
    bool saveFile(QcwTabFolder* folder, int index, QStringList* error = nullptr);
    /// Saves every dirty pane of every folder, then reports what could not be written.
    void saveAll();

    /// Installs @p folder's tab close handler. Called once per folder the container announces.
    void connectFolder(QcwTabFolder* folder);

    void saveState(const QString& xmlFile);
    /// False when the file is missing, the document does not describe a window, or a folder
    /// refused its element; the layout then stays as it was or is rebuilt from scratch.
    bool restoreState(const QString& xmlFile);

private:
    Ui::MainWindow* ui;
    QMap<QString, TabPaneCreator> creators;
    QSet<QString> openedFiles;
};
