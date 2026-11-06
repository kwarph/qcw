// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "MainWindow.h"
#include "./ui_MainWindow.h"
#include "DefaultTabFolderRenderer.h"
#include "StyledTabFolderRenderer.h"
#include "PreferencesDialog.h"
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QActionGroup>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFileInfo>

const QString kFileNameDataKey { "filename" };

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow), preferences { new PreferencesDialog(this) }
{
    ui->setupUi(this);
    setWindowTitle("Styled Renderer Demo");

    applyTheme(ui->tabFolder);
    connectFolder(ui->tabFolder);
    ui->tabFolder->setActive(true);

    connect(preferences, &PreferencesDialog::tabPositionChanged, [this](bool atTop)
    {
        ui->tabFolder->setTabPosition(atTop ? qcw::TabPosition::Top : qcw::TabPosition::Bottom);
    });

    connect(preferences, &PreferencesDialog::styleChanged, [this](bool isDefault)
    {
        isDefaultStyle = isDefault;
        for (auto f : std::as_const(folders))
        {
            applyTheme(f);
        }
    });

    // The context is this window: qApp outlives it, so without one the lambda would run
    // on a dangling this after the window is gone.
    connect(qApp, &QApplication::focusWindowChanged, this, [this](QWindow*)
    {
        ui->tabFolder->setActive(isActiveWindow());
    });

    prepareActions();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    preferences->close();

    for (auto f : std::as_const(folders))
    {
        f->close();
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::applyTheme(QcwTabFolder* folder)
{
    const auto theme = preferences->themeData(isDefaultStyle);
    if (isDefaultStyle)
    {
        auto renderer = dynamic_cast<DefaultTabFolderRenderer*>(folder->renderer());
        if (!renderer)
        {
            renderer = new DefaultTabFolderRenderer(folder);
            folder->setRenderer(renderer);
        }
        renderer->setThemeData(theme);
    }
    else
    {
        auto renderer = dynamic_cast<StyledTabFolderRenderer*>(folder->renderer());
        if (!renderer)
        {
            renderer = new StyledTabFolderRenderer(folder);
            folder->setRenderer(renderer);
        }
        renderer->setThemeData(theme);
    }
}

void MainWindow::connectFolder(QcwTabFolder* folder)
{
    // Tabs can arrive in a folder more than once, and a second set of connections would run
    // the close handler twice per request, so handlers go on only folders this window newly knows.
    if (folders.contains(folder))
    {
        return;
    }
    folders.insert(folder);

    // Every folder needs its own receiver: QcwTabFolder only emits tabCloseRequested(), it
    // never removes a tab itself, so a folder without this connection has an inert close button.
    connect(folder, &QcwTabFolder::tabCloseRequested, [this, folder](int index)
    {
        if (folder->isTabDirty(index))
        {
            QMessageBox::StandardButtons buttons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            const auto answer = QMessageBox::question(this, "Close File",
                                                      QString("The file \"%1\" has been modified."
                                                              " Do you want to save your changes or discard it?")
                                                          .arg(folder->tabText(index)),
                                                      buttons, QMessageBox::Cancel);

            if (answer == QMessageBox::Cancel)
            {
                return;
            }

            if (answer == QMessageBox::Save && !saveFile(folder, index))
            {
                // The write failed: keep the tab open, discarding it would lose the changes.
                return;
            }
        }

        openedFiles.remove(folder->tabData(index, kFileNameDataKey).toString());

        folder->removeTab(index);
    });

    connect(folder, &QcwTabFolder::tabTransferred, [this](QcwTabFolder* source, QcwTabFolder* target, QWidget*)
    {
        Q_UNUSED(source);

        connect(target, &QcwTabFolder::aboutToClose, [this, target](QCloseEvent*)
        {
            folders.remove(target);
        });

        applyTheme(target);
        connectFolder(target);
    });
}

bool MainWindow::saveFile(QcwTabFolder* folder, int index)
{
    // The pane of the requested tab, not the current one: the close request can name a tab
    // that is not showing, and writing the current editor into that tab's file would corrupt it.
    auto editor = qobject_cast<QPlainTextEdit*>(folder->tabPane(index));
    if (!editor)
    {
        QMessageBox::warning(this, "Save File", QString("The tab has no text pane to save."));
        return false;
    }

    QFile file(folder->tabData(index, kFileNameDataKey).toString());
    if (!file.open(QFile::WriteOnly | QFile::Text))
    {
        QMessageBox::warning(this, "Save File",
                             QString("Cannot write \"%1\":\n%2").arg(file.fileName(), file.errorString()));
        return false;
    }

    file.write(editor->toPlainText().toUtf8());
    file.close();
    folder->setTabDirty(index, false);

    return true;
}

void MainWindow::openFile(const QString& filePath)
{
    if (openedFiles.contains(filePath))
    {
        return;
    }

    QFile f(filePath);
    if (!f.open(QFile::ReadOnly))
    {
        QMessageBox::warning(this, "Open File", QString("Cannot open \"%1\":\n%2").arg(filePath, f.errorString()));
        return;
    }

    openedFiles.insert(filePath);

    auto editor = new QPlainTextEdit(ui->tabFolder);
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setPlainText(QString::fromUtf8(f.readAll()));

    QIcon icon = QIcon::fromTheme(QIcon::ThemeIcon::DocumentNew);

    const int index = ui->tabFolder->addTab(editor, QFileInfo(filePath).fileName(), icon);
    ui->tabFolder->setTabToolTip(index, filePath);
    ui->tabFolder->setTabData(index, kFileNameDataKey, filePath);

    // The first tab is deliberately pinned: it also shows off setTabClosable()/setTabDetachable().
    if (index == 0)
    {
        ui->tabFolder->setTabClosable(index, false);
        ui->tabFolder->setTabDetachable(index, false);
    }

    // Mark dirty through the editor, not a captured index: closing or transferring a tab
    // shifts every later index, and the captured one would then dirty the wrong tab.
    connect(editor, &QPlainTextEdit::textChanged, [this, editor]()
    {
        for (auto folder : std::as_const(folders))
        {
            const int at = folder->indexOf(editor);
            if (at >= 0)
            {
                folder->setTabDirty(at, true);
            }
        }
    });
}

void MainWindow::prepareActions()
{
    connect(ui->actionOpenFiles, &QAction::triggered, [this](bool)
    {
        const auto files = QFileDialog::getOpenFileNames(this, "Open Files");
        for (const auto& filename : std::as_const(files))
        {
            openFile(filename);
        }
    });

    connect(ui->actionSave, &QAction::triggered, [this](bool)
    {
        saveFile(ui->tabFolder, ui->tabFolder->currentIndex());
    });

    connect(ui->action_Preferences, &QAction::triggered, [this](bool)
    {
        if (preferences->isVisible())
        {
            preferences->raise();
            return;
        }

        preferences->show();
    });
}
