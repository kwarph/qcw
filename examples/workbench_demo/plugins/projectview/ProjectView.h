// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ProjectFilterProxyModel.h"
#include "QcwView.h"

#include <QFileInfo>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVariantMap>

class QAction;
class QFileSystemModel;
class QLabel;
class QModelIndex;
class QPersistentModelIndex;
class QPushButton;
class QStackedLayout;
class QToolBar;
class QWidget;
class QcwTabFolder;
class ProjectTreeModel;
class ProjectTreeView;

/**
 * @brief A file manager for several project folders, one of which is current.
 *
 * @details The tree's top-level rows are the open projects (ProjectTreeModel), and selecting
 * anything inside a project makes it the current one. Actions scoped to a project — New File,
 * Save All, Close Project — use the current project; Save and Save As follow the selected
 * file's editor, as before. Files are opened in the window's shared folder through the
 * IEditorService, so an already open file is focused wherever its tab currently lives. Rename,
 * delete and drag-and-drop moves keep the open editors in step.
 *
 * The project list and the current project are remembered in QSettings, so a recreated view
 * reopens them. The filter and the expanded folders are part of the saved view state, as
 * saveExtraState() shows; expansion is remembered by path because the file indexes are rebuilt
 * and only exist once QFileSystemModel has loaded the directory.
 */
class ProjectView : public QcwView
{
    Q_OBJECT
public:
    explicit ProjectView(QWidget* parent = nullptr);

    QHash<QString, QcwCommandHandler*> commandHandlers() const override;
    QToolBar* topRightToolBar() const override;

    void saveExtraState(QVariantMap& state) const override;
    void restoreExtraState(const QVariantMap& state) override;

protected:
    bool event(QEvent* event) override;
    void retranslateUi() override;

private:
    void setupUi();
    void setupActions();
    void connectSignals();
    void loadProjects();

    void newProject();
    void openProject();
    void closeCurrentProject();
    void addProject(const QString& path);
    QString projectConflict(const QString& root) const;
    void selectProject(const QString& root);
    void currentSelectionChanged();
    void updateProjectsPage();

    void refresh();
    void showFilterDialog();
    void applyFilter(const ProjectFilter& filter, bool logChange);
    void updateActions();

    QString expansionPath(const QModelIndex& proxyIndex) const;
    void expandPendingPaths();

    void openFile(const QString& filePath);
    void saveSelected();
    void saveAsSelected();
    void saveAll();
    void newFile();
    void newFolder();
    void deleteSelected();
    void renameSelected();

    QString createName(const QString& title);
    QModelIndex selectedProxyIndex() const;
    QFileInfo selectedFileInfo() const;
    QString targetDirectory() const;
    QString directoryForTarget(const QPersistentModelIndex& proxyTarget) const;
    QcwTabFolder* editorFolder() const;
    void logActivity(const QString& message);
    void setupHandlers();

    QFileSystemModel* mModel;
    ProjectTreeModel* mProjects;
    ProjectFilterProxyModel* mProxy;
    ProjectTreeView* mTree;
    QWidget* mWelcomePage;
    QLabel* mWelcomeTitle;
    QLabel* mWelcomeHint;
    QPushButton* mWelcomeButton;
    QStackedLayout* mStack;
    QToolBar* mToolBar;

    QAction* mNewProjectAction;
    QAction* mOpenProjectAction;
    QAction* mCloseProjectAction;
    QAction* mNewFileAction;
    QAction* mNewFolderAction;
    QAction* mSaveAction;
    QAction* mSaveAsAction;
    QAction* mSaveAllAction;
    QAction* mDeleteAction;
    QAction* mRenameAction;
    QAction* mFilterAction;
    QAction* mRefreshAction;

    ProjectFilter mFilter;

    QSet<QString> mExpandedPaths;
    QSet<QString> mPendingExpansion;
    QHash<QString, QcwCommandHandler*> mHandlers;
};
