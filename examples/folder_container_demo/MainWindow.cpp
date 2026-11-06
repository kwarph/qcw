// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwTabFolderSite.h"
#include "QcwFastViewToolBar.h"
#include "QcwTabPaneManager.h"
#include "QcwSerializeUtils.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QSplitter>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

const QString kFileNameDataKey { "filename" };

/// Plain-text pane that keeps its own folder up to date.
///
/// The pane, not the code that opened it, owns the bookkeeping: a tab can move between folders
/// and a folder can be rebuilt by restoreState(), and both would leave a handler installed at
/// the opening site behind pointing at a pane it no longer knows.
class TextEditor : public QPlainTextEdit
{
public:
    explicit TextEditor(QWidget* parent = nullptr) :
        QPlainTextEdit { parent }
    {
        connect(this, &QPlainTextEdit::textChanged, this, [this]()
        {
            if (auto folder = containingFolder())
            {
                folder->setTabDirty(folder->indexOf(this), true);
            }
        });
    }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::FocusIn)
        {
            auto folder = containingFolder();
            // The container promotes a folder from a click on its tab bar, which keyboard focus
            // never produces, so announce that click here. The index has to be a real tab: the
            // container reads -1 as "this folder has no current tab" and re-announces the change
            // on every focus move.
            if (int index = folder ? folder->indexOf(this) : -1; index >= 0)
            {
                emit folder->tabBarClicked(index);
            }
        }

        return QPlainTextEdit::event(event);
    }

private:
    /// The folder showing this pane, or nullptr while it belongs to none.
    QcwTabFolder* containingFolder() const
    {
        for (auto widget = parentWidget(); widget; widget = widget->parentWidget())
        {
            if (auto folder = qobject_cast<QcwTabFolder*>(widget))
            {
                return folder;
            }
        }

        return nullptr;
    }
};

static QWidget* createLabelPane(const QList<QVariant>& creatingArgs)
{
    QString content;
    if (!creatingArgs.empty())
    {
        content = creatingArgs.front().toString();
    }

    auto label = new QLabel { content };
    label->setProperty("CreatingArguments", creatingArgs);

    return label;
}

static QWidget* createTextEditPane(const QList<QVariant>& creatingArgs)
{
    QString filePath;
    if (!creatingArgs.empty())
    {
        filePath = creatingArgs.front().toString();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return nullptr;
    }

    auto editor = new TextEditor;
    editor->setProperty("CreatingArguments", creatingArgs);
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setPlainText(QString::fromUtf8(file.readAll()));

    return editor;
}

static void createPlaceholder(QcwTabFolder* centralFolder)
{
    auto label = new QLabel(centralFolder);
    label->setText(QObject::tr("Execute '<b>File > Open Files...</b>' to open files."));
    label->setAlignment(Qt::AlignCenter);
    centralFolder->setPlaceholder(label);
}

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Folder Container Demo");

    creators.insert("QLabel", createLabelPane);
    creators.insert("QPlainTextEdit", createTextEditPane);

    createInitialLayout();

    prepareActions();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::createInitialLayout()
{
    auto icon = QIcon::fromTheme(QIcon::ThemeIcon::WindowNew);

    auto folder1 = ui->folderContainer->createFolder(nullptr, qcw::LeftDockArea);
    auto pane = createPane("QLabel", { QString { "Projects" } });

    auto index = folder1->addTab(pane, "Projects", icon);
    folder1->setTabData(index, qcw::TabPaneIdPropertyName, pane->property(qcw::TabPaneIdPropertyName));
    folder1->setTabClosable(index, false);
    folder1->setTabDetachable(index, false);

    auto folder2 = ui->folderContainer->createFolder(folder1, qcw::RightDockArea);
    folder2->setFeature(qcw::ShowFullTitle, false);

    ui->folderContainer->setCentralFolder(folder2);
    createPlaceholder(folder2);

    auto folder3 = ui->folderContainer->createFolder(folder2, qcw::BottomDockArea);
    pane = createPane("QLabel", { QString { "Console" } });

    index = folder3->addTab(pane, "Console", icon);
    folder3->setTabData(index, qcw::TabPaneIdPropertyName, pane->property(qcw::TabPaneIdPropertyName));
    folder3->setTabClosable(index, false);
    folder3->setTabDetachable(index, false);

    auto splitter = ui->folderContainer->parentSplitter(folder1);
    splitter->setSizes({ 240, width() - 240 });

    splitter = ui->folderContainer->parentSplitter(folder3);
    splitter->setSizes({ height() - 240, 240 });
}

void MainWindow::prepareActions()
{
    // A folder reaches connectFolder() either from createInitialLayout() or from the folders
    // restoreState() recreates; the signal covers both, so wire it before the first folder exists.
    connect(ui->folderContainer, &QcwTabFolderContainer::folderCreated, this, &MainWindow::connectFolder);

    for (auto folder : ui->folderContainer->folders())
    {
        connectFolder(folder);
    }

    connect(ui->folderContainer, &QcwTabFolderContainer::currentFolderChanged, [this](QcwTabFolder* folder)
    {
        if (folder)
        {
            folder->setActive(true);
            for (auto other : ui->folderContainer->folders())
            {
                if (other != folder)
                {
                    other->setActive(false);
                }
            }
        }
    });

    connect(ui->folderContainer, &QcwTabFolderContainer::folderMinimized,
            [this](QcwTabFolder* folder, Qt::ToolBarArea fastViewToolBarArea)
    {
        // One toolbar per signal: the toolbar takes the folder over, and QcwTabFolder hands it
        // back and schedules the toolbar for deletion when the folder is restored or destroyed.
        auto listAllTabs = !ui->folderContainer->isCentralFolder(folder);
        auto toolBar = new QcwFastViewToolBar(folder, ui->folderContainer, QSize(600, 400), listAllTabs, this);
        addToolBar(fastViewToolBarArea, toolBar);
    });

    connect(ui->actionDockingDialog, &QAction::triggered, this, &MainWindow::openDockingDialog);

    connect(ui->actionOpenFiles, &QAction::triggered, [this](bool)
    {
        const auto files = QFileDialog::getOpenFileNames(this, "Open Files");

        QStringList failed;
        for (const auto& filename : files)
        {
            if (!openFile(filename))
            {
                failed.append(filename);
            }
        }

        if (!failed.empty())
        {
            QMessageBox::warning(this, "Open Files", tr("These files could not be opened:\n%1").arg(failed.join('\n')));
        }
    });

    connect(ui->actionSave, &QAction::triggered, [this](bool)
    {
        auto folder = ui->folderContainer->currentFolder();
        if (!folder)
        {
            return;
        }

        // An empty folder reports -1, which isTabDirty() answers with "not dirty".
        QStringList error;
        saveFile(folder, folder->currentIndex(), &error);

        if (!error.empty())
        {
            QMessageBox::warning(this, "Save", error.join(QStringLiteral("\n\n")));
        }
    });

    connect(ui->actionSaveAll, &QAction::triggered, [this](bool)
    {
        saveAll();
    });

    connect(ui->actionSaveState, &QAction::triggered, [this](bool)
    {
        saveState(QString("%1/state.xml").arg(QApplication::applicationDirPath()));
    });

    connect(ui->actionRestoreState, &QAction::triggered, [this](bool)
    {
        const auto xmlFile = QString("%1/state.xml").arg(QApplication::applicationDirPath());
        if (restoreState(xmlFile))
        {
            return;
        }

        // Restoring clears the container as its first step, so a failure leaves a partial layout
        // where the old one was. Start over from the initial three folders rather than keep it.
        ui->folderContainer->clear();
        tabPaneManager->clear(true);
        openedFiles.clear();

        createInitialLayout();

        QMessageBox::warning(this, "Restore State",
                             tr("%1 does not hold a layout this build can restore."
                                " The window was reset to its initial folders.")
                                 .arg(xmlFile));
    });

    connect(ui->actionExit, &QAction::triggered, qApp, &QApplication::quit);
}

void MainWindow::openDockingDialog()
{
    QDialog dialog { this };
    dialog.setWindowTitle(tr("Docking in a Dialog"));
    dialog.resize(800, 520);

    auto layout = new QVBoxLayout { &dialog };
    auto container = new QcwTabFolderContainer { &dialog };
    layout->addWidget(container);

    auto navigation = container->createFolder(nullptr, qcw::LeftDockArea);
    navigation->addTab(createPane("QLabel", { QString { "Navigation" } }), "Navigation");
    navigation->setTabClosable(0, false);

    auto main = container->createFolder(navigation, qcw::RightDockArea);
    main->addTab(createPane("QLabel", { QString { "Main Area" } }), "Main Area");
    main->setTabClosable(0, false);
    container->setCentralFolder(main);

    auto console = container->createFolder(main, qcw::BottomDockArea);
    console->addTab(createPane("QLabel", { QString { "Output" } }), "Output");
    console->setTabClosable(0, false);

    // Every folder: a minimized folder leaves the dialog for the host's tool bar area, and a
    // QDialog has none, so the button is taken away instead of being offered and doing nothing.
    for (auto folder : container->folders())
    {
        folder->setFeature(qcw::Minimizable, false);
    }

    auto buttons = new QDialogButtonBox { QDialogButtonBox::Close, &dialog };
    connect(buttons, &QDialogButtonBox::clicked, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::saveAll()
{
    QStringList error;
    for (auto folder : ui->folderContainer->folders())
    {
        for (int i = 0; i < folder->count(); ++i)
        {
            saveFile(folder, i, &error);
        }
    }

    if (!error.empty())
    {
        QMessageBox::warning(this, "Save All", error.join(QStringLiteral("\n\n")));
    }
}

bool MainWindow::saveFile(QcwTabFolder* folder, int index, QStringList* error)
{
    if (!folder->isTabDirty(index))
    {
        return true;
    }

    // The pane of this tab, not the one currently visible: Save All walks every index, and
    // reading the visible editor wrote its text into the file of each dirty tab in turn.
    auto fileName = folder->tabData(index, kFileNameDataKey).toString();
    auto editor = qobject_cast<QPlainTextEdit*>(folder->tabPane(index));
    if (fileName.isEmpty() || !editor)
    {
        if (error)
        {
            *error << tr("\"%1\" has no text file to write to, so its changes cannot be saved.")
                          .arg(folder->tabText(index));
        }
        return false;
    }

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text))
    {
        if (error)
        {
            *error << tr("\"%1\" could not be written to %2.\n\n%3")
                          .arg(folder->tabText(index), fileName, file.errorString());
        }
        return false;
    }

    file.write(editor->toPlainText().toUtf8());
    folder->setTabDirty(index, false);

    return true;
}

void MainWindow::saveState(const QString& xmlFile)
{
    QByteArray xmlData;
    QXmlStreamWriter writer(&xmlData);
    writer.writeDTD(R"(<?xml version="1.0" encoding="UTF-8"?>)");

    writer.writeStartElement("qcw"); // --> qcw begin
    writer.writeAttribute("version", QString::number(1));

    writer.writeStartElement("window");
    writer.writeAttribute("geometry", saveGeometry().toHex(' '));

    writer.writeStartElement("tabPanes"); // --> tabPanes begin

    const auto tabPanes = tabPaneManager->allTabPanes().keys();
    writer.writeAttribute("count", QString::number(tabPanes.size()));

    for (auto pane : tabPanes)
    {
        writer.writeStartElement("tabPane"); // --> tabPane begin
        writer.writeAttribute("type", pane->property("PaneType").toString());
        writer.writeAttribute("id", QString::number(pane->property(qcw::TabPaneIdPropertyName).toInt()));

        auto args = pane->property("CreatingArguments").toList();
        auto ba = QcwSerializeUtils::serialize(args);
        writer.writeAttribute("creatingArguments", ba.toHex(' '));

        writer.writeEndElement(); // --> tabPane end
    }

    writer.writeEndElement(); // --> tabPanes end

    ui->folderContainer->saveState(writer);

    writer.writeEndElement(); // --> window end

    writer.writeEndElement(); // --> qcw end

    writer.writeEndDocument();

    // QSaveFile writes next to the target and moves the result in, so an interrupted write
    // keeps the previous state.xml instead of leaving a truncated one behind.
    QSaveFile file(xmlFile);
    if (!file.open(QIODevice::WriteOnly) || !file.write(xmlData) || !file.commit())
    {
        QMessageBox::warning(this, "Save State",
                             tr("The layout could not be written to\n%1\n\n%2").arg(xmlFile, file.errorString()));
    }
}

bool MainWindow::restoreState(const QString& xmlFile)
{
    QFile file(xmlFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QXmlStreamReader reader(&file);
    reader.readNextStartElement();
    if (reader.name() != "qcw")
    {
        return false;
    }

    reader.readNextStartElement();
    if (reader.name() != "window")
    {
        return false;
    }
    auto winState = reader.attributes().value("geometry").toLocal8Bit();

    const auto fastViewToolBars = findChildren<QcwFastViewToolBar*>();
    for (auto* toolBar : fastViewToolBars)
    {
        removeToolBar(toolBar);
        delete toolBar;
    }

    ui->folderContainer->clear();
    tabPaneManager->clear(true);

    // The open set follows the restored tabs below, so drop what the discarded layout held.
    openedFiles.clear();

    int maxPaneId = -1;
    QMap<int, QPair<QWidget*, int>> panes;
    bool foldersRestored = true;
    while (foldersRestored && reader.readNextStartElement())
    {
        if (reader.name() == "tabPanes")
        {
            while (reader.readNextStartElement())
            {
                if (reader.name() == "tabPane")
                {
                    QString type = reader.attributes().value("type").toString();
                    int id = reader.attributes().value("id").toInt();
                    auto ba = reader.attributes().value("creatingArguments").toLocal8Bit();
                    auto args = QcwSerializeUtils::deserialize<QList<QVariant>>(ba.fromHex(ba));

                    auto pane = createPane(type, args);
                    if (pane)
                    {
                        // The pair's second id is the one the pane should carry after the restore;
                        // this demo keeps the saved id, so the two are the same.
                        pane->setProperty(qcw::TabPaneIdPropertyName, id);
                        panes.insert(id, { pane, id });
                    }
                    reader.skipCurrentElement();

                    if (id > maxPaneId)
                    {
                        maxPaneId = id;
                    }
                }
                else
                {
                    reader.skipCurrentElement();
                }
            }
        }
        else if (reader.name() == "folderContainer")
        {
            // A folder that refuses its element leaves the container holding half a layout,
            // which the caller has to discard, so the document is not simply carried on past it.
            foldersRestored = ui->folderContainer->restoreState(reader, nullptr, panes);
        }
        else
        {
            reader.skipCurrentElement();
        }
    }

    if (!foldersRestored || reader.hasError())
    {
#ifdef QCW_DEBUG
        qDebug() << "restore failed" << reader.error();
#endif
        return false;
    }

    auto icon = QIcon::fromTheme(QIcon::ThemeIcon::WindowNew);
    for (auto folder : ui->folderContainer->folders())
    {
        for (int i = 0; i < folder->count(); ++i)
        {
            auto pane = folder->tabPane(i);
            if (!pane)
            {
                // A tab whose file disappeared: it has the saved text but no pane to show.
                continue;
            }

            folder->setTabIcon(i, icon);

            const auto fileName = folder->tabData(i, kFileNameDataKey).toString();
            if (!fileName.isEmpty())
            {
                openedFiles.insert(fileName);
            }
        }

        if (ui->folderContainer->isCentralFolder(folder))
        {
            createPlaceholder(folder);
        }
    }

    tabPaneManager->resetStartingPaneId(maxPaneId + 1);

    restoreGeometry(QByteArray::fromHex(winState));

    update();

    return true;
}

void MainWindow::connectFolder(QcwTabFolder* folder)
{
    connect(folder, &QcwTabFolder::tabCloseRequested, [this, folder](int index)
    {
        const auto fileName = folder->tabData(index, kFileNameDataKey).toString();

        if (folder->isTabDirty(index))
        {
            QMessageBox::StandardButtons btns(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            auto rt = QMessageBox::question(this, "Close File",
                                            QString("The file \"%1\" has been modified."
                                                    " Do you want to save your changes or discard it?")
                                                .arg(folder->tabText(index)),
                                            btns, QMessageBox::Cancel);

            if (rt == QMessageBox::Cancel)
            {
                return;
            }

            if (rt == QMessageBox::Save)
            {
                QStringList error;
                if (!saveFile(folder, index, &error))
                {
                    // Keep the tab: closing it here would drop changes that never reached the disk.
                    QMessageBox::warning(this, "Close File", error.join(QStringLiteral("\n\n")));
                    return;
                }
            }
        }

        openedFiles.remove(fileName);

        folder->removeTab(index);
    });
}

bool MainWindow::openFile(const QString& filePath)
{
    if (openedFiles.contains(filePath))
    {
        return true;
    }

    auto pane = createPane("QPlainTextEdit", { filePath });
    if (!pane)
    {
        return false;
    }

    openedFiles.insert(filePath);

    auto icon = QIcon::fromTheme(QIcon::ThemeIcon::WindowNew);
    auto centralFolder = ui->folderContainer->centralFolder();
    auto index = centralFolder->addTab(pane, QFileInfo(filePath).fileName(), icon);

    centralFolder->setTabToolTip(index, filePath);
    centralFolder->setTabData(index, kFileNameDataKey, filePath);

    return true;
}

QWidget* MainWindow::createPane(const QString& paneType, const QList<QVariant>& creatingArgs)
{
    auto creator = creators.value(paneType);
    QWidget* pane { nullptr };
    if (creator)
    {
        pane = creator(creatingArgs);
        if (pane)
        {
            pane->setProperty("PaneType", paneType);
            pane->setProperty(qcw::TabPaneIdPropertyName, tabPaneManager->nextPaneId());
        }
    }
    return pane;
}
