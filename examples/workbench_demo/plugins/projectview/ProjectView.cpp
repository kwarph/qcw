// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ProjectView.h"

#include "DemoServices.h"
#include "DemoUtils.h"
#include "ProjectFilterDialog.h"
#include "ProjectTreeModel.h"
#include "ProjectTreeView.h"
#include "QcwLambdaCommandHandler.h"
#include "QcwTopicService.h"
#include "QcwPluginManager.h"
#include "QcwTabFolder.h"
#include "QcwViewCreator.h"
#include "QcwWindow.h"
#include "QcwWindowActionIds.h"
#include "QcwFixedRowHeightItemDelegate.h"
#include "QcwOverlayScrollBarUtils.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QStackedLayout>
#include <QStandardPaths>
#include <QStyle>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QTimer>

#include <algorithm>

namespace
{

// The announcements only refresh the save actions; they carry no state this view keeps.
constexpr QLatin1String kEditorAnnouncementTopics[] {
    demo::TopicEditorOpened,
    demo::TopicEditorClosed,
    demo::TopicEditorSaved,
    demo::TopicEditorPathChanged,
};

QcwTabFolder* folderOf(QWidget* pane)
{
    for (QWidget* widget = pane; widget; widget = widget->parentWidget())
    {
        if (auto* folder = qobject_cast<QcwTabFolder*>(widget))
        {
            return folder;
        }
    }

    return nullptr;
}
} // namespace

ProjectView::ProjectView(QWidget* parent) :
    QcwView { parent },
    mModel { new QFileSystemModel { this } },
    mProjects { new ProjectTreeModel { this } },
    mProxy { new ProjectFilterProxyModel { this } },
    mTree { nullptr },
    mWelcomePage { nullptr },
    mStack { nullptr },
    mToolBar { nullptr },
    mNewProjectAction { nullptr },
    mOpenProjectAction { nullptr },
    mCloseProjectAction { nullptr },
    mNewFileAction { nullptr },
    mNewFolderAction { nullptr },
    mSaveAction { nullptr },
    mSaveAsAction { nullptr },
    mSaveAllAction { nullptr },
    mDeleteAction { nullptr },
    mRenameAction { nullptr },
    mFilterAction { nullptr },
    mRefreshAction { nullptr }
{
    setupUi();
    setupActions();
    setupHandlers();
    connectSignals();
    loadProjects();
    updateActions();
}

QToolBar* ProjectView::topRightToolBar() const
{
    return mToolBar;
}

void ProjectView::saveExtraState(QVariantMap& state) const
{
    state.insert(QStringLiteral("filterPatterns"), mFilter.patterns);
    state.insert(QStringLiteral("filterMode"), static_cast<int>(mFilter.mode));
    state.insert(QStringLiteral("filterCaseSensitive"), mFilter.caseSensitive);
    state.insert(QStringLiteral("filterApplyToDirectories"), mFilter.applyToDirectories);

    QStringList expanded(mExpandedPaths.cbegin(), mExpandedPaths.cend());
    expanded.sort();
    state.insert(QStringLiteral("expandedPaths"), expanded);
}

void ProjectView::restoreExtraState(const QVariantMap& state)
{
    ProjectFilter filter;
    filter.patterns = state.value(QStringLiteral("filterPatterns")).toString();
    filter.mode = static_cast<ProjectFilter::Mode>(
        state.value(QStringLiteral("filterMode"), static_cast<int>(ProjectFilter::Mode::ShowOnly)).toInt());
    filter.caseSensitive = state.value(QStringLiteral("filterCaseSensitive")).toBool();
    filter.applyToDirectories = state.value(QStringLiteral("filterApplyToDirectories")).toBool();

    applyFilter(filter, false);

    const QStringList expanded = state.value(QStringLiteral("expandedPaths")).toStringList();
    mExpandedPaths = QSet<QString>(expanded.cbegin(), expanded.cend());
    mPendingExpansion = mExpandedPaths;
    expandPendingPaths();
}

void ProjectView::setupUi()
{
    mModel->setReadOnly(false);
    mModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);

    mProjects->setSourceModel(mModel);
    mProxy->setSourceModel(mProjects);
    mProxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    mTree = new ProjectTreeView { this };
    mTree->setModel(mProxy);
    mTree->setUniformRowHeights(true);
    mTree->setSortingEnabled(true);
    mTree->sortByColumn(0, Qt::AscendingOrder);
    mTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTree->setSelectionMode(QAbstractItemView::SingleSelection);
    mTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTree->setDragEnabled(true);
    mTree->setAcceptDrops(true);
    mTree->setDropIndicatorShown(true);
    mTree->setDragDropMode(QAbstractItemView::DragDrop);
    mTree->setDefaultDropAction(Qt::MoveAction);
    mTree->setContextMenuPolicy(Qt::CustomContextMenu);
    mTree->setItemDelegate(new QcwFixedRowHeightItemDelegate { mTree });

    attachOverlayScrollBar(mTree, this);

    for (int column = 1; column < mModel->columnCount(); ++column)
    {
        mTree->setColumnHidden(column, true);
    }

    mTree->header()->setStretchLastSection(false);
    mTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mTree->setHeaderHidden(true);

    mWelcomePage = new QWidget { this };
    auto* welcomeLayout = new QVBoxLayout { mWelcomePage };
    welcomeLayout->addStretch();

    mWelcomeTitle = new QLabel { tr("No project is open"), mWelcomePage };
    mWelcomeTitle->setAlignment(Qt::AlignCenter);
    mWelcomeTitle->setStyleSheet(QStringLiteral("font-weight: bold;"));

    mWelcomeHint = new QLabel { tr("Add a project folder to browse and edit its files. "
                                    "The folder dialog can also create one."),
                                 mWelcomePage };
    mWelcomeHint->setAlignment(Qt::AlignCenter);
    mWelcomeHint->setWordWrap(true);

    mWelcomeButton = new QPushButton { tr("Open Project..."), mWelcomePage };
    connect(mWelcomeButton, &QPushButton::clicked, this, &ProjectView::openProject);

    welcomeLayout->addWidget(mWelcomeTitle);
    welcomeLayout->addWidget(mWelcomeHint);
    welcomeLayout->addSpacing(12);
    welcomeLayout->addWidget(mWelcomeButton, 0, Qt::AlignCenter);
    welcomeLayout->addStretch();

    mStack = new QStackedLayout { this };
    mStack->addWidget(mWelcomePage);
    mStack->addWidget(mTree);
    mStack->setCurrentWidget(mWelcomePage);

    mToolBar = new QToolBar { this };
    mToolBar->setIconSize(QSize { 16, 16 });
    mToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
}

void ProjectView::setupActions()
{
    mNewProjectAction =
        new QAction { style()->standardIcon(QStyle::SP_FileDialogNewFolder), tr("New Project..."), this };
    mNewProjectAction->setToolTip(tr("Create a folder and open it as a project"));
    connect(mNewProjectAction, &QAction::triggered, this, &ProjectView::newProject);

    mOpenProjectAction =
        new QAction { style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Open Project..."), this };
    mOpenProjectAction->setToolTip(tr("Browse an existing folder as a project"));
    connect(mOpenProjectAction, &QAction::triggered, this, &ProjectView::openProject);

    mCloseProjectAction =
        new QAction { style()->standardIcon(QStyle::SP_DialogCloseButton), tr("Close Project"), this };
    mCloseProjectAction->setToolTip(tr("Remove the current project from the tree"));
    connect(mCloseProjectAction, &QAction::triggered, this, &ProjectView::closeCurrentProject);

    mNewFileAction = new QAction { style()->standardIcon(QStyle::SP_FileIcon), tr("New File..."), this };
    connect(mNewFileAction, &QAction::triggered, this, &ProjectView::newFile);

    mNewFolderAction =
        new QAction { style()->standardIcon(QStyle::SP_FileDialogNewFolder), tr("New Folder..."), this };
    connect(mNewFolderAction, &QAction::triggered, this, &ProjectView::newFolder);

    mSaveAction = new QAction { style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save"), this };
    connect(mSaveAction, &QAction::triggered, this, &ProjectView::saveSelected);

    mSaveAsAction = new QAction { style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save As..."), this };
    connect(mSaveAsAction, &QAction::triggered, this, &ProjectView::saveAsSelected);

    // No platform icon names a "save all" for every theme: take the theme's own when it has
    // one and reuse the save button's icon otherwise, so the two never disagree in meaning.
    mSaveAllAction = new QAction { QIcon::fromTheme(QStringLiteral("document-save-all"),
                                                     style()->standardIcon(QStyle::SP_DialogSaveButton)),
                                    tr("Save All"), this };
    mSaveAllAction->setToolTip(tr("Save every modified file in the current project"));
    connect(mSaveAllAction, &QAction::triggered, this, &ProjectView::saveAll);

    mDeleteAction = new QAction { style()->standardIcon(QStyle::SP_TrashIcon), tr("Delete"), this };
    mDeleteAction->setShortcut(QKeySequence::Delete);
    connect(mDeleteAction, &QAction::triggered, this, &ProjectView::deleteSelected);

    // Some themes ship no rename icon at all; the action then keeps its text only.
    mRenameAction = new QAction { QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Rename..."), this };
    mRenameAction->setShortcut(QKeySequence { Qt::Key_F2 });
    connect(mRenameAction, &QAction::triggered, this, &ProjectView::renameSelected);

    mFilterAction = new QAction { style()->standardIcon(QStyle::SP_FileDialogContentsView), tr("Filter..."), this };
    mFilterAction->setToolTip(tr("Filter the project files"));
    connect(mFilterAction, &QAction::triggered, this, &ProjectView::showFilterDialog);

    mRefreshAction = new QAction { style()->standardIcon(QStyle::SP_BrowserReload), tr("Refresh"), this };
    mRefreshAction->setToolTip(tr("Reload the project folders"));
    connect(mRefreshAction, &QAction::triggered, this, [this]()
    {
        refresh();
        logActivity(tr("Refreshed the project tree"));
    });

    mToolBar->addAction(mFilterAction);
    mToolBar->addAction(mRefreshAction);
}

bool ProjectView::event(QEvent* event)
{
    if (event->type() == QEvent::Show)
    {
        // The view is shown before the window finishes making it current; publish on the next
        // event-loop turn so currentSelectionChanged() sees this view as currentView().
        QTimer::singleShot(0, this, &ProjectView::currentSelectionChanged);
    }
    else if (event->type() == QEvent::Hide)
    {
        // A tab or perspective switch hides the tree; drop the subject so consumers do not keep
        // showing a file that is off screen.
        if (auto* window = owningWindow())
        {
            window->topicService()->clear(QString(demo::TopicProjectSelection));
        }
    }

    return QcwView::event(event);
}

void ProjectView::retranslateUi()
{
    mWelcomeTitle->setText(tr("No project is open"));
    mWelcomeHint->setText(tr("Add a project folder to browse and edit its files. "
                              "The folder dialog can also create one."));
    mWelcomeButton->setText(tr("Open Project..."));

    mNewProjectAction->setText(tr("New Project..."));
    mNewProjectAction->setToolTip(tr("Create a folder and open it as a project"));
    mOpenProjectAction->setText(tr("Open Project..."));
    mOpenProjectAction->setToolTip(tr("Browse an existing folder as a project"));
    mCloseProjectAction->setText(tr("Close Project"));
    mCloseProjectAction->setToolTip(tr("Remove the current project from the tree"));
    mNewFileAction->setText(tr("New File..."));
    mNewFolderAction->setText(tr("New Folder..."));
    mSaveAction->setText(tr("Save"));
    mSaveAsAction->setText(tr("Save As..."));
    mSaveAllAction->setText(tr("Save All"));
    mSaveAllAction->setToolTip(tr("Save every modified file in the current project"));
    mDeleteAction->setText(tr("Delete"));
    mRenameAction->setText(tr("Rename..."));
    mFilterAction->setText(tr("Filter..."));
    mFilterAction->setToolTip(tr("Filter the project files"));
    mRefreshAction->setText(tr("Refresh"));
    mRefreshAction->setToolTip(tr("Reload the project folders"));
}

void ProjectView::connectSignals()
{
    // Expansion is remembered by path: QFileSystemModel rebuilds its indexes and loads
    // directories asynchronously, so a saved index would be stale or not resolvable yet.
    connect(mTree, &QTreeView::expanded, this, [this](const QModelIndex& index)
    {
        const QString path = expansionPath(index);
        if (!path.isEmpty())
        {
            mExpandedPaths.insert(path);
        }
    });
    connect(mTree, &QTreeView::collapsed, this, [this](const QModelIndex& index)
    {
        const QString path = expansionPath(index);
        if (!path.isEmpty())
        {
            mExpandedPaths.remove(path);
        }
    });

    // A path becomes resolvable only after its directory has been fetched, so retry the saved
    // expansions whenever the file model finishes loading another directory.
    connect(mModel, &QFileSystemModel::directoryLoaded, this, &ProjectView::expandPendingPaths);

    connect(mTree, &QTreeView::doubleClicked, this, [this](const QModelIndex& proxyIndex)
    {
        const QModelIndex treeIndex = mProxy->mapToSource(proxyIndex);
        if (!treeIndex.isValid() || mProjects->isDir(treeIndex))
        {
            return;
        }

        openFile(mProjects->filePath(treeIndex));
    });

    connect(mTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &ProjectView::currentSelectionChanged);

    connect(mProjects, &ProjectTreeModel::projectsChanged, this, [this]()
    {
        demo::rememberProjectRoots(mProjects->projects());
        updateProjectsPage();
        updateActions();
    });

    connect(mProjects, &ProjectTreeModel::currentProjectChanged, this, [this](const QString& root)
    {
        demo::rememberCurrentProject(root);
        updateActions();
    });

    // Save and Save As follow whether the selected file is open in an editor, so they have to be
    // re-evaluated when an editor appears, goes away, is saved or is renamed. The framework keeps
    // these subscriptions on the window the view actually belongs to, and a view recreated into a
    // window hears each announcement again.
    const auto refreshAfterEditorEvent = [this](const QVariantMap&) { updateActions(); };

    for (const QLatin1String& topic : kEditorAnnouncementTopics)
    {
        subscribeWindowTopic(QString(topic), refreshAfterEditorEvent);
    }

    connect(mTree, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
    {
        const QModelIndex index = mTree->indexAt(pos);
        if (index.isValid())
        {
            mTree->setCurrentIndex(index);
        }
        else
        {
            mTree->clearSelection();
        }

        updateActions();

        QMenu menu { this };
        menu.addAction(mNewProjectAction);
        menu.addAction(mOpenProjectAction);
        menu.addAction(mCloseProjectAction);
        menu.addSeparator();
        menu.addAction(mNewFileAction);
        menu.addAction(mNewFolderAction);
        menu.addAction(mSaveAction);
        menu.addAction(mSaveAsAction);
        menu.addAction(mSaveAllAction);
        menu.addSeparator();
        menu.addAction(mDeleteAction);
        menu.addAction(mRenameAction);
        menu.exec(mTree->viewport()->mapToGlobal(pos));
    });

    mTree->dropHandler = [this](const QPersistentModelIndex& target, const QStringList& paths)
    {
        const QString targetDir = directoryForTarget(target);
        if (targetDir.isEmpty())
        {
            return;
        }

        auto* service = demo::editorService();
        for (const auto& path : paths)
        {
            const QString movedTo = QDir { targetDir }.filePath(QFileInfo { path }.fileName());
            if (service)
            {
                service->remapEditorPaths(path, movedTo);
            }

            logActivity(tr("Moved %1 to %2").arg(path, targetDir));
        }

        updateActions();
    };
}

void ProjectView::loadProjects()
{
    // Read before the model announces anything: setProjects() makes its first project current
    // and the slot persists that choice, which would overwrite the remembered one.
    const QString remembered = demo::normalizedPath(demo::rememberedCurrentProject());

    mProjects->setProjects(demo::rememberedProjectRoots());
    if (mProjects->containsProject(remembered))
    {
        mProjects->setCurrentProject(remembered);
    }

    updateProjectsPage();
}

void ProjectView::newProject()
{
    const QString current = mProjects->currentProject();
    const QString start = current.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation) :
                                              QFileInfo { current }.absolutePath();

    const QString parentDir =
        QFileDialog::getExistingDirectory(this, tr("New Project"), start, QFileDialog::ShowDirsOnly);
    if (parentDir.isEmpty())
    {
        return;
    }

    const QString name = createName(tr("New Project"));
    if (name.isEmpty())
    {
        return;
    }

    const QString path = QDir { parentDir }.filePath(name);
    if (QFileInfo::exists(path))
    {
        QMessageBox::warning(this, tr("New Project"), tr("\"%1\" already exists.").arg(name));
        return;
    }

    if (!QDir { parentDir }.mkdir(name))
    {
        QMessageBox::warning(this, tr("New Project"), tr("Cannot create the folder \"%1\".").arg(path));
        return;
    }

    logActivity(tr("Created project folder %1").arg(path));
    addProject(path);
}

void ProjectView::openProject()
{
    const QString current = mProjects->currentProject();
    const QString start = current.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation) :
                                              QFileInfo { current }.absolutePath();

    const QString path = QFileDialog::getExistingDirectory(this, tr("Open Project"), start, QFileDialog::ShowDirsOnly);
    if (!path.isEmpty())
    {
        addProject(path);
    }
}

void ProjectView::closeCurrentProject()
{
    const QString root = mProjects->currentProject();
    if (root.isEmpty())
    {
        return;
    }

    mProjects->closeProject(root);
    logActivity(tr("Closed project %1").arg(root));
    updateActions();
}

void ProjectView::addProject(const QString& path)
{
    const QString root = demo::normalizedPath(path);
    if (root.isEmpty() || !QFileInfo { root }.isDir())
    {
        QMessageBox::warning(this, tr("Open Project"), tr("\"%1\" is not a folder.").arg(path));
        return;
    }

    if (mProjects->containsProject(root))
    {
        QMessageBox::warning(this, tr("Open Project"), tr("\"%1\" is already open.").arg(root));
        return;
    }

    if (const QString conflict = projectConflict(root); !conflict.isEmpty())
    {
        QMessageBox::warning(this, tr("Open Project"),
                             tr("\"%1\" overlaps the open project \"%2\".").arg(root, conflict));
        return;
    }

    if (!mProjects->addProject(root))
    {
        return;
    }

    logActivity(tr("Opened project %1").arg(root));
    selectProject(root);
    updateActions();
}

QString ProjectView::projectConflict(const QString& root) const
{
    for (const QString& existing : mProjects->projects())
    {
        if (existing == root)
        {
            continue;
        }

        if (demo::pathIsUnder(root, existing) || demo::pathIsUnder(existing, root))
        {
            return existing;
        }
    }

    return {};
}

void ProjectView::selectProject(const QString& root)
{
    const QModelIndex index = mProxy->mapFromSource(mProjects->projectIndex(root));
    if (!index.isValid())
    {
        return;
    }

    mTree->setCurrentIndex(index);
    mTree->scrollTo(index);
}

void ProjectView::currentSelectionChanged()
{
    const QFileInfo info = selectedFileInfo();
    if (!info.filePath().isEmpty())
    {
        const QString root = mProjects->projectForPath(info.absoluteFilePath());
        if (!root.isEmpty())
        {
            mProjects->setCurrentProject(root);
        }
    }

    updateActions();

    // Only the current view owns the window's subject; background tabs stay silent.
    if (auto* window = owningWindow(); window && window->currentView() == this)
    {
        QVariantMap payload;
        payload[QString(demo::PathKey)] = info.absoluteFilePath();
        payload[QString(demo::IsDirKey)] = info.isDir();
        window->topicService()->publish(QString(demo::TopicProjectSelection), payload);
    }
}

void ProjectView::updateProjectsPage()
{
    const bool hasProjects = !mProjects->projects().isEmpty();
    mStack->setCurrentWidget(hasProjects ? static_cast<QWidget*>(mTree) : mWelcomePage);
}

void ProjectView::refresh()
{
    mProjects->refresh();
    updateActions();
}

void ProjectView::showFilterDialog()
{
    ProjectFilterDialog dialog { mFilter, this };
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    applyFilter(dialog.filter(), true);
}

void ProjectView::applyFilter(const ProjectFilter& filter, bool logChange)
{
    mFilter = filter;
    mProxy->setFilter(mFilter);

    if (logChange)
    {
        if (mFilter.isEmpty())
        {
            logActivity(tr("Cleared the file filter"));
        }
        else
        {
            logActivity(tr("File filter: %1 (%2)")
                            .arg(mFilter.patterns, mFilter.mode == ProjectFilter::Mode::ShowOnly ?
                                                        tr("show matching") :
                                                        tr("hide matching")));
        }
    }

    updateActions();

    // Paths the previous filter hid may now have a proxy index.
    expandPendingPaths();
}

QString ProjectView::expansionPath(const QModelIndex& proxyIndex) const
{
    return mProjects->filePath(mProxy->mapToSource(proxyIndex));
}

void ProjectView::expandPendingPaths()
{
    if (mPendingExpansion.isEmpty())
    {
        return;
    }

    // Ancestors have to be expanded before a deep path resolves; the path length orders them.
    QStringList paths(mPendingExpansion.cbegin(), mPendingExpansion.cend());
    std::sort(paths.begin(), paths.end(), [](const QString& left, const QString& right)
    {
        return left.size() < right.size();
    });

    for (const QString& path : paths)
    {
        const QModelIndex sourceIndex = mModel->index(path);
        if (!sourceIndex.isValid())
        {
            continue;
        }

        // Filtered-out nodes have no proxy index; they stay pending until the filter changes.
        const QModelIndex proxyIndex = mProxy->mapFromSource(mProjects->mapFromSource(sourceIndex));
        if (!proxyIndex.isValid())
        {
            continue;
        }

        mTree->expand(proxyIndex);
        mPendingExpansion.remove(path);
    }
}

void ProjectView::updateActions()
{
    const QString current = mProjects->currentProject();
    const bool hasCurrent = !current.isEmpty();

    const QModelIndex treeIndex = mProxy->mapToSource(selectedProxyIndex());
    const bool hasSelection = treeIndex.isValid();
    const bool isProjectRow = mProjects->isProjectIndex(treeIndex);
    const QFileInfo info = mProjects->fileInfo(treeIndex);

    QWidget* editor = nullptr;
    if (hasSelection && info.isFile())
    {
        if (auto* service = demo::editorService())
        {
            editor = service->editorForFile(info.absoluteFilePath());
        }
    }

    mCloseProjectAction->setEnabled(hasCurrent);
    mNewFileAction->setEnabled(hasCurrent);
    mNewFolderAction->setEnabled(hasCurrent);
    mSaveAllAction->setEnabled(hasCurrent);
    mSaveAction->setEnabled(editor != nullptr);
    mSaveAsAction->setEnabled(editor != nullptr);
    mDeleteAction->setEnabled(hasSelection && !isProjectRow);
    mRenameAction->setEnabled(hasSelection && !isProjectRow);
    mFilterAction->setEnabled(hasCurrent);
    mRefreshAction->setEnabled(hasCurrent);

    for (auto* handler : mHandlers)
    {
        handler->notifyStateChanged();
    }
}

void ProjectView::openFile(const QString& filePath)
{
    auto* service = demo::editorService();
    if (!service)
    {
        QMessageBox::warning(this, tr("Open File"), tr("The TextEditor plugin is not available."));
        return;
    }

    if (service->editorForFile(filePath))
    {
        service->revealAt(filePath, -1);
        logActivity(tr("Activated %1").arg(filePath));
        return;
    }

    auto* creator = pluginManager->getPluginT<QcwViewCreator>(QString(demo::TextEditorType));
    if (!creator)
    {
        QMessageBox::warning(this, tr("Open File"), tr("The TextEditor plugin is not loaded."));
        return;
    }

    auto* folder = editorFolder();
    if (!folder)
    {
        QMessageBox::warning(this, tr("Open File"), tr("The window has no folder to open files in."));
        return;
    }

    auto* view = creator->create(folder, true, { filePath });
    if (!view)
    {
        QMessageBox::warning(this, tr("Open File"), tr("The editor could not be created."));
        return;
    }

    const int index = folder->addTab(view, QFileInfo { filePath }.fileName(), creator->defaultViewIcon());

    if (index < 0)
    {
        delete view;
        return;
    }

    folder->setTabToolTip(index, filePath);
    folder->setCurrentIndex(index);
    view->setFocus();

    logActivity(tr("Opened %1").arg(filePath));
}

void ProjectView::saveSelected()
{
    auto* service = demo::editorService();
    if (!service)
    {
        return;
    }

    const QFileInfo info = selectedFileInfo();
    QWidget* editor = service->editorForFile(info.absoluteFilePath());
    if (!editor)
    {
        return;
    }

    QString error;
    if (!service->saveEditor(editor, &error))
    {
        QMessageBox::warning(this, tr("Save File"), error);
        return;
    }

    logActivity(tr("Saved %1").arg(info.absoluteFilePath()));
    updateActions();
}

void ProjectView::saveAsSelected()
{
    auto* service = demo::editorService();
    if (!service)
    {
        return;
    }

    const QFileInfo info = selectedFileInfo();
    QWidget* editor = service->editorForFile(info.absoluteFilePath());
    if (!editor)
    {
        return;
    }

    const QString targetPath = QFileDialog::getSaveFileName(this, tr("Save File As"), info.absoluteFilePath());

    if (targetPath.isEmpty())
    {
        return;
    }

    QString error;
    if (!service->saveEditorTo(editor, targetPath, &error))
    {
        QMessageBox::warning(this, tr("Save File As"), error);
        return;
    }

    logActivity(tr("Saved %1 as %2").arg(info.absoluteFilePath(), targetPath));
    updateActions();
}

void ProjectView::saveAll()
{
    auto* service = demo::editorService();
    const QString root = mProjects->currentProject();
    if (!service || root.isEmpty())
    {
        return;
    }

    QStringList errors;
    const int saved = service->saveEditorsUnder(root, &errors);

    if (!errors.isEmpty())
    {
        QMessageBox::warning(this, tr("Save All"), errors.join(QStringLiteral("\n\n")));
        return;
    }

    if (saved > 0)
    {
        logActivity(tr("Saved %1 file(s) in %2").arg(saved).arg(root));
    }
    else
    {
        logActivity(tr("Nothing to save in %1").arg(root));
    }
}

void ProjectView::newFile()
{
    const QString baseDir = targetDirectory();
    if (baseDir.isEmpty())
    {
        return;
    }

    const QString name = createName(tr("New File"));
    if (name.isEmpty())
    {
        return;
    }

    const QString path = QDir { baseDir }.filePath(name);
    if (QFileInfo::exists(path))
    {
        QMessageBox::warning(this, tr("New File"), tr("\"%1\" already exists.").arg(name));
        return;
    }

    QFile file { path };
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, tr("New File"), tr("Cannot create \"%1\":\n%2").arg(path, file.errorString()));
        return;
    }
    file.close();

    logActivity(tr("Created file %1").arg(path));
    refresh();
    openFile(path);
    updateActions();
}

void ProjectView::newFolder()
{
    const QString baseDir = targetDirectory();
    if (baseDir.isEmpty())
    {
        return;
    }

    const QString name = createName(tr("New Folder"));
    if (name.isEmpty())
    {
        return;
    }

    const QString path = QDir { baseDir }.filePath(name);
    if (QFileInfo::exists(path))
    {
        QMessageBox::warning(this, tr("New Folder"), tr("\"%1\" already exists.").arg(name));
        return;
    }

    if (!QDir { baseDir }.mkdir(name))
    {
        QMessageBox::warning(this, tr("New Folder"), tr("Cannot create the folder \"%1\".").arg(path));
        return;
    }

    logActivity(tr("Created folder %1").arg(path));
    refresh();
    updateActions();
}

void ProjectView::deleteSelected()
{
    const QModelIndex treeIndex = mProxy->mapToSource(selectedProxyIndex());
    if (mProjects->isProjectIndex(treeIndex))
    {
        return;
    }

    const QFileInfo info = mProjects->fileInfo(treeIndex);
    const QString path = info.absoluteFilePath();
    if (!info.exists())
    {
        return;
    }

    const auto answer =
        QMessageBox::question(this, tr("Delete"),
                              info.isDir() ? tr("Delete the folder \"%1\" and everything in it?").arg(info.fileName()) :
                                             tr("Delete the file \"%1\"?").arg(info.fileName()),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer != QMessageBox::Yes)
    {
        return;
    }

    const bool success = info.isDir() ? QDir { path }.removeRecursively() : QFile::remove(path);
    if (!success)
    {
        QMessageBox::warning(this, tr("Delete"), tr("Cannot delete \"%1\".").arg(path));
        return;
    }

    if (auto* service = demo::editorService())
    {
        service->closeEditorsUnder(path);
    }

    logActivity(tr("Deleted %1").arg(path));
    refresh();
    updateActions();
}

void ProjectView::renameSelected()
{
    const QModelIndex treeIndex = mProxy->mapToSource(selectedProxyIndex());
    if (mProjects->isProjectIndex(treeIndex))
    {
        return;
    }

    const QFileInfo info = mProjects->fileInfo(treeIndex);
    const QString oldPath = info.absoluteFilePath();
    if (!info.exists())
    {
        return;
    }

    bool accepted = false;
    const QString newName =
        QInputDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, info.fileName(), &accepted)
            .trimmed();
    if (!accepted || newName.isEmpty() || newName == info.fileName())
    {
        return;
    }

    if (newName.contains(QLatin1Char('/')) || newName.contains(QLatin1Char('\\')))
    {
        QMessageBox::warning(this, tr("Rename"), tr("A name cannot contain slashes."));
        return;
    }

    const QString newPath = QDir { info.absolutePath() }.filePath(newName);
    if (QFileInfo::exists(newPath))
    {
        QMessageBox::warning(this, tr("Rename"), tr("\"%1\" already exists.").arg(newName));
        return;
    }

    if (!QFile::rename(oldPath, newPath))
    {
        QMessageBox::warning(this, tr("Rename"), tr("Cannot rename \"%1\" to \"%2\".").arg(oldPath, newPath));
        return;
    }

    if (auto* service = demo::editorService())
    {
        service->remapEditorPaths(oldPath, newPath);
    }

    logActivity(tr("Renamed %1 to %2").arg(oldPath, newPath));
    refresh();
    updateActions();
}

QString ProjectView::createName(const QString& title)
{
    bool accepted = false;
    const QString name =
        QInputDialog::getText(this, title, tr("Name:"), QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted || name.isEmpty())
    {
        return {};
    }

    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')))
    {
        QMessageBox::warning(this, title, tr("A name cannot contain slashes."));
        return {};
    }

    return name;
}

QModelIndex ProjectView::selectedProxyIndex() const
{
    const QModelIndexList selected = mTree->selectionModel()->selectedRows();
    return selected.isEmpty() ? QModelIndex {} : selected.first();
}

QFileInfo ProjectView::selectedFileInfo() const
{
    return mProjects->fileInfo(mProxy->mapToSource(selectedProxyIndex()));
}

QString ProjectView::targetDirectory() const
{
    const QFileInfo info = selectedFileInfo();
    if (info.isDir())
    {
        return info.absoluteFilePath();
    }

    if (info.isFile())
    {
        return info.absolutePath();
    }

    return mProjects->currentProject();
}

QString ProjectView::directoryForTarget(const QPersistentModelIndex& proxyTarget) const
{
    const QFileInfo info = mProjects->fileInfo(mProxy->mapToSource(proxyTarget));
    if (info.isDir())
    {
        return info.absoluteFilePath();
    }

    if (info.isFile())
    {
        return info.absolutePath();
    }

    return mProjects->currentProject();
}

QcwTabFolder* ProjectView::editorFolder() const
{
    if (auto* window = owningWindow())
    {
        if (auto* shared = window->sharedFolder())
        {
            return shared;
        }
    }

    return folderOf(const_cast<ProjectView*>(this));
}

void ProjectView::logActivity(const QString& message)
{
    // qInfo() reaches the process log; the LogView follows QcwLogger, so it shows the line
    // with the same timestamp and level as the log file.
    qInfo().noquote() << message;
}

void ProjectView::setupHandlers()
{
    auto addHandler = [this](auto id, auto exec, auto enabled)
    {
        auto* h = new QcwLambdaCommandHandler(this);
        h->onExecute(exec);
        h->onEnabled(enabled);
        mHandlers.insert(id, h);
    };

    addHandler(demo::ActionNewProject,
               [this](const QVariantMap&)
    {
        newProject();
    }, []()
    {
        return true;
    });
    addHandler(demo::ActionOpenProject,
               [this](const QVariantMap&)
    {
        openProject();
    }, []()
    {
        return true;
    });
    addHandler(demo::ActionCloseProject,
               [this](const QVariantMap&)
    {
        closeCurrentProject();
    }, [this]()
    {
        return !mProjects->currentProject().isEmpty();
    });
    addHandler(demo::ActionNewFile,
               [this](const QVariantMap&)
    {
        newFile();
    }, [this]()
    {
        return !mProjects->currentProject().isEmpty();
    });

    addHandler(QcwWindowActionIds::ActionSave,
               [this](const QVariantMap&)
    {
        saveSelected();
    }, [this]()
    {
        if (!demo::editorService())
        {
            return false;
        }
        return demo::editorService()->editorForFile(selectedFileInfo().absoluteFilePath()) != nullptr;
    });
    addHandler(QcwWindowActionIds::ActionSaveAs,
               [this](const QVariantMap&)
    {
        saveAsSelected();
    }, [this]()
    {
        if (!demo::editorService())
        {
            return false;
        }
        return demo::editorService()->editorForFile(selectedFileInfo().absoluteFilePath()) != nullptr;
    });
    addHandler(QcwWindowActionIds::ActionSaveAll,
               [this](const QVariantMap&)
    {
        saveAll();
    }, [this]()
    {
        return !mProjects->currentProject().isEmpty();
    });

    addHandler(QcwWindowActionIds::ActionDelete,
               [this](const QVariantMap&)
    {
        deleteSelected();
    }, [this]()
    {
        const QModelIndex treeIndex = mProxy->mapToSource(selectedProxyIndex());
        return treeIndex.isValid() && !mProjects->isProjectIndex(treeIndex);
    });
    addHandler(QcwWindowActionIds::ActionRename,
               [this](const QVariantMap&)
    {
        renameSelected();
    }, [this]()
    {
        const QModelIndex treeIndex = mProxy->mapToSource(selectedProxyIndex());
        return treeIndex.isValid() && !mProjects->isProjectIndex(treeIndex);
    });

    addHandler(QcwWindowActionIds::ActionFilter,
               [this](const QVariantMap&)
    {
        showFilterDialog();
    }, [this]()
    {
        return !mProjects->currentProject().isEmpty();
    });
    addHandler(QcwWindowActionIds::ActionRefresh,
               [this](const QVariantMap&)
    {
        refresh();
        logActivity(tr("Refreshed the project tree"));
    }, [this]()
    {
        return !mProjects->currentProject().isEmpty();
    });
}

QHash<QString, QcwCommandHandler*> ProjectView::commandHandlers() const
{
    return mHandlers;
}
