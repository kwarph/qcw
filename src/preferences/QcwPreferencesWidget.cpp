// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwPreferencesWidget.h"
#include "QcwPreferencesPage.h"
#include "QcwSettings.h"
#include "QcwTreeProxyModel.h"
#include "QcwFixedRowHeightItemDelegate.h"
#include "QcwPathUtils.h"
#include "QcwOverlayScrollArea.h"
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QShowEvent>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStringList>
#include <QTreeView>

QT_BEGIN_NAMESPACE

namespace
{

bool isSameFile(const QString& first, const QString& second)
{
    const QFileInfo firstInfo(first);
    const QFileInfo secondInfo(second);

    // Resolve symlinks when both ends exist; a path comparison alone would miss an import
    // that names the live file through a different link.
    if (firstInfo.exists() && secondInfo.exists())
    {
        return firstInfo.canonicalFilePath() == secondInfo.canonicalFilePath();
    }

    return firstInfo.absoluteFilePath() == secondInfo.absoluteFilePath();
}

/// True when @p filePath is the file backing @p settings.
bool isLiveSettingsFile(const QSettings& settings, const QString& filePath)
{
    // The default provider on Windows reports a registry key here, which is not a path the
    // user could have selected in a file dialog.
    const QFileInfo liveInfo(settings.fileName());
    if (!liveInfo.isAbsolute())
    {
        return false;
    }

    return isSameFile(liveInfo.absoluteFilePath(), filePath);
}

/**
 * @brief Path of the backup file that importSettings() writes before replacing @p settings.
 *
 * @details The snapshot belongs beside the store it backs up, which keeps two scopes apart: a
 * project INI and the global QcwSettings.ini must not overwrite each other's backup. A store
 * without a file — the Windows registry is the normal case — falls back to the per-user config
 * directory, where the workbench's own INI lives.
 */
QString backupFilePath(const QSettings& settings)
{
    const QFileInfo liveInfo(settings.fileName());
    if (liveInfo.isAbsolute())
    {
        return liveInfo.absoluteFilePath() + QStringLiteral(".bak");
    }

    return QDir(QcwAppDirectories::configDirectory()).filePath(QStringLiteral("QcwSettings.ini.bak"));
}

/// Writes every key of @p source to @p targetPath as an INI file.
bool writeSettingsSnapshot(const QSettings& source, const QString& targetPath)
{
    // clear() makes the snapshot exact: keys left over in a previously existing file must not
    // reappear as settings the source never had.
    QSettings target(targetPath, QSettings::IniFormat);
    target.clear();
    for (const auto& key : source.allKeys())
    {
        target.setValue(key, source.value(key));
    }

    // Writes are buffered, so only sync() reveals an unwritable destination.
    target.sync();
    return target.status() == QSettings::NoError;
}

} // namespace

// -----------------------------------------------------------------------------

class QcwPreferencesProxyModel : public QcwTreeProxyModel
{
public:
    explicit QcwPreferencesProxyModel(QcwPreferencesWidgetPrivate* impl, QObject* parent = nullptr);

protected:
    bool matchesAdditionalConditions(const QModelIndex& sourceIndex, const QString& filterText) const override;

private:
    QcwPreferencesWidgetPrivate* preferencesWidgetPimpl;
};

// -----------------------------------------------------------------------------

class QcwPreferencesWidgetPrivate
{
public:
    explicit QcwPreferencesWidgetPrivate(QcwPreferencesWidget* interface);

    void setupUi(QLineEdit* filterEdit, QLabel* navigationLabel, QTreeView* tree, QSplitter* navigationSplitter);
    void retranslateUi();
    void buildPreferencesTree();
    QStandardItem* ensureTreeItem(const QString& settingsGroup, QMap<QString, QStandardItem*>& items);
    QcwPreferencesPage* currentPage() const;
    void showPage(const QModelIndex& index);
    void clearPage();
    void selectFirstPage();
    void relabelTreeItems(const QModelIndex& parent);

    QModelIndex firstPageIndex(const QAbstractItemModel& model, const QModelIndex& parent = QModelIndex());

public:
    QcwPreferencesWidget* owner;
    QMap<QString, QcwPreferencesPage*> pages;
    QcwSettingsProvider settingsProvider;
    QLineEdit* lineEdit { nullptr };
    QLabel* pathLabel { nullptr };
    QSplitter* splitter { nullptr };
    QTreeView* treeView { nullptr };
    QStandardItemModel* model { nullptr };
    QcwPreferencesProxyModel* proxyModel { nullptr };
    QScrollArea* rightPanel { nullptr };
    bool treeBuilt { false };
};

// -----------------------------------------------------------------------------

QcwPreferencesWidget::QcwPreferencesWidget(const QMap<QString, QcwPreferencesPage*>& pages,
                                           QcwSettingsProvider settingsProvider,
                                           QWidget* parent) :
    QcwNavigationContentWidget { parent }, impl { new QcwPreferencesWidgetPrivate { this } }
{
    // An empty provider is the documented shorthand for the process-wide store. Resolve it
    // here, once, so export/import and every page below see the same non-empty provider.
    if (!settingsProvider)
    {
        settingsProvider = []()
        {
            return QcwSettings::get();
        };
    }

    impl->pages = pages;
    impl->settingsProvider = settingsProvider;

    // Pages read their values through QcwPreferencesPage::settings(). Installing the provider
    // here, before the first page is shown, binds them to the same store export/import use.
    for (auto page : impl->pages)
    {
        if (page)
        {
            page->setSettingsProvider(settingsProvider);
        }
    }
    // The base owns the filter, tree, splitter and breadcrumb. Hand those widgets to the
    // private once, so it can drive them without reaching into the base's protected API.
    impl->setupUi(filterEdit(), navigationLabel(), treeView(), splitter());

    // setupUi() creates the page host; the base reparents it into the content pane.
    setContentWidget(impl->rightPanel);
}

QcwPreferencesWidget::~QcwPreferencesWidget()
{
    // Pages are borrowed, not owned by this widget (they belong to the caller, usually
    // QcwPreferencesManager): detach the current one so the scroll area does not delete it
    // with the rest of its children. takeWidget() reparents the page to nullptr, which also
    // hides it.
    impl->rightPanel->takeWidget();

    delete impl;
}

QSize QcwPreferencesWidget::sizeHint() const
{
    return QSize(800, 600);
}

void QcwPreferencesWidget::exportSettings()
{
    auto outputFile = QFileDialog::getSaveFileName(
        this, tr("Export Settings"), QDir(QcwLastVisitPath::get()).filePath(QStringLiteral("Settings.ini")),
        tr("INI files (*.ini)"));
    if (outputFile.isEmpty())
    {
        return;
    }

    // Export the store this widget was built for, which is not necessarily the process-wide
    // one: a project preferences window must not copy the global configuration by mistake.
    auto currentSettings = impl->settingsProvider();
    if (isLiveSettingsFile(currentSettings, outputFile))
    {
        QMessageBox::warning(this, tr("Export Settings Failed"),
                             tr("Choose a file other than the settings file in use."));
        return;
    }

    if (!writeSettingsSnapshot(currentSettings, outputFile))
    {
        QMessageBox::warning(this, tr("Export Settings Failed"),
                             tr("Could not write the settings to %1.").arg(QDir::toNativeSeparators(outputFile)));
        return;
    }

    QcwLastVisitPath::set(outputFile);
}

void QcwPreferencesWidget::importSettings()
{
    auto inputFile = QFileDialog::getOpenFileName(this, tr("Import Settings"), QcwLastVisitPath::get(),
                                                  tr("INI files (*.ini);;All files (*)"));
    if (inputFile.isEmpty())
    {
        return;
    }

    const QFileInfo inputInfo(inputFile);
    if (!inputInfo.isFile() || !inputInfo.isReadable())
    {
        QMessageBox::warning(this, tr("Import Settings Failed"),
                             tr("Cannot read %1.").arg(QDir::toNativeSeparators(inputFile)));
        return;
    }

    // Reject a file QSettings cannot parse before touching the live settings.
    QSettings inputSettings(inputFile, QSettings::IniFormat);
    inputSettings.sync();
    if (inputSettings.status() != QSettings::NoError)
    {
        QMessageBox::warning(this, tr("Import Settings Failed"),
                             tr("%1 is not a valid settings file.").arg(QDir::toNativeSeparators(inputFile)));
        return;
    }

    auto currentSettings = impl->settingsProvider();
    if (isLiveSettingsFile(currentSettings, inputFile))
    {
        QMessageBox::warning(this, tr("Import Settings Failed"),
                             tr("The selected file is already the settings file in use."));
        return;
    }

    // Import replaces this widget's store, so anything the file does not contain is about to
    // disappear. Keep a snapshot first: it covers keys whose owner never takes part in the
    // import, like plugin settings or keys added by a newer application version. The backup
    // sits beside the store, so a project import cannot overwrite the global backup.
    const QString backupFile = backupFilePath(currentSettings);
    if (!writeSettingsSnapshot(currentSettings, backupFile))
    {
        QMessageBox::warning(
            this, tr("Import Settings Failed"),
            tr("Could not back up the current settings to %1.").arg(QDir::toNativeSeparators(backupFile)));
        return;
    }

    // The file decides which keys exist afterwards: keys it does not list are removed instead
    // of surviving from the old settings.
    currentSettings.clear();
    for (const auto& key : inputSettings.allKeys())
    {
        currentSettings.setValue(key, inputSettings.value(key));
    }

    // Flush before the pages and QcwLastVisitPath create their own QSettings objects:
    // otherwise they can still read the old values, and their writes can overwrite the
    // pending ones when their objects are destroyed.
    currentSettings.sync();
    if (currentSettings.status() != QSettings::NoError)
    {
        QMessageBox::warning(this, tr("Import Settings Failed"),
                             tr("Could not write the imported settings to the live store. "
                                "The previous configuration is in %1.")
                                 .arg(QDir::toNativeSeparators(backupFile)));
        return;
    }

    QcwLastVisitPath::set(inputFile);

    // The store changed behind the pages' backs: show the imported values and re-evaluate the
    // filter, because page contents can depend on them. Only this widget's pages use this
    // store, so no other page needs reloading.
    for (auto* page : impl->pages)
    {
        if (page)
        {
            page->loadSettings();
        }
    }
    impl->proxyModel->invalidate();
    impl->selectFirstPage();
}

void QcwPreferencesWidget::restoreDefaults()
{
    if (auto* page = impl->currentPage())
    {
        page->restoreDefaults();
    }
}

void QcwPreferencesWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (!impl->treeBuilt)
    {
        impl->buildPreferencesTree();
        impl->treeView->expandAll();
        impl->treeBuilt = true;
    }

    impl->splitter->setSizes({ 240, qMax(0, impl->splitter->width() - 240) });

    impl->selectFirstPage();
}

void QcwPreferencesWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        impl->retranslateUi();
    }

    QWidget::changeEvent(event);
}

// -----------------------------------------------------------------------------

QcwPreferencesProxyModel::QcwPreferencesProxyModel(QcwPreferencesWidgetPrivate* impl, QObject* parent) :
    QcwTreeProxyModel { parent }, preferencesWidgetPimpl { impl }
{
}

bool QcwPreferencesProxyModel::matchesAdditionalConditions(const QModelIndex& sourceIndex,
                                                           const QString& filterText) const
{
    const auto settingsGroup = sourceIndex.data(Qt::UserRole).toString();
    if (auto* page = preferencesWidgetPimpl->pages.value(settingsGroup))
    {
        return page->matchesFilter(filterText);
    }
    return false;
}

// -----------------------------------------------------------------------------

QcwPreferencesWidgetPrivate::QcwPreferencesWidgetPrivate(QcwPreferencesWidget* interface) :
    owner { interface }
{
}

void QcwPreferencesWidgetPrivate::setupUi(QLineEdit* filterEdit,
                                          QLabel* navigationLabel,
                                          QTreeView* tree,
                                          QSplitter* navigationSplitter)
{
    // The base owns these widgets; the private keeps pointers so it can drive the page tree
    // without being a friend of the host (and without exposing the base's protected API).
    lineEdit = filterEdit;
    pathLabel = navigationLabel;
    treeView = tree;
    splitter = navigationSplitter;

    model = new QStandardItemModel(owner);
    proxyModel = new QcwPreferencesProxyModel(this, owner);
    proxyModel->setSourceModel(model);

    treeView->setModel(proxyModel);
    treeView->setItemDelegate(new QcwFixedRowHeightItemDelegate(treeView));

    rightPanel = new QcwOverlayScrollArea(owner);
    rightPanel->setObjectName("rightPanel");
    rightPanel->setWidgetResizable(true);
    rightPanel->setFrameShape(QFrame::NoFrame);

    QObject::connect(lineEdit, &QLineEdit::textChanged, [this](const QString& text)
    {
        proxyModel->setFilterText(text);
        selectFirstPage();
    });

    QObject::connect(treeView->selectionModel(), &QItemSelectionModel::currentChanged,
                     [this](const QModelIndex& current, const QModelIndex&)
    {
        showPage(current);
    });

    retranslateUi();
}

void QcwPreferencesWidgetPrivate::retranslateUi()
{
    owner->setWindowTitle(QCoreApplication::translate("QcwPreferencesWidget", "Preferences"));
    lineEdit->setPlaceholderText(QCoreApplication::translate("QcwPreferencesWidget", "Filter"));

    // Take the source index up front: re-labelling can drop the row out of the filter, which
    // invalidates the index the view holds, and the path label would go empty.
    const auto current = proxyModel->mapToSource(treeView->currentIndex());

    relabelTreeItems({});
    showPage(current);
}

/// Re-applies every page name in the tree, in the language now installed.
void QcwPreferencesWidgetPrivate::relabelTreeItems(const QModelIndex& parent)
{
    for (int row = 0, rows = model->rowCount(parent); row < rows; ++row)
    {
        const auto index = model->index(row, 0, parent);
        if (auto* page = pages.value(index.data(Qt::UserRole).toString()))
        {
            model->itemFromIndex(index)->setText(page->name());
        }

        relabelTreeItems(index);
    }
}

/// Depth-first search for the first row of @p model that has a page.
QModelIndex QcwPreferencesWidgetPrivate::firstPageIndex(const QAbstractItemModel& model, const QModelIndex& parent)
{
    for (int row = 0, rows = model.rowCount(parent); row < rows; ++row)
    {
        const auto index = model.index(row, 0, parent);

        // Intermediate groups have no page of their own, so the plain first row may be one.
        if (pages.value(index.data(Qt::UserRole).toString()))
        {
            return index;
        }

        if (auto descendant = firstPageIndex(model, index); descendant.isValid())
        {
            return descendant;
        }
    }

    return {};
}

void QcwPreferencesWidgetPrivate::buildPreferencesTree()
{
    QMap<QString, QStandardItem*> items;

    for (const auto& [settingsGroup, page] : pages.asKeyValueRange())
    {
        // The constructor ignores a null entry when it installs providers; keep the tree in
        // step with that instead of dereferencing it here.
        if (!page)
        {
            continue;
        }

        auto* item = ensureTreeItem(settingsGroup, items);
        item->setText(page->name());
        item->setData(settingsGroup, Qt::UserRole);
    }
}

QStandardItem* QcwPreferencesWidgetPrivate::ensureTreeItem(const QString& settingsGroup,
                                                           QMap<QString, QStandardItem*>& items)
{
    if (auto* item = items.value(settingsGroup))
    {
        return item;
    }

    // Intermediate groups may have no page of their own (only "a/b/c" is registered, not
    // "a" or "a/b"). Create them with the raw path segment as label so the registered page
    // still appears at its full depth instead of jumping to the root.
    const auto slash = settingsGroup.lastIndexOf(QLatin1Char('/'));
    const auto parentGroup = slash < 0 ? QString {} : settingsGroup.left(slash);
    const auto label = settingsGroup.mid(slash + 1);

    auto* item = new QStandardItem(label);
    if (parentGroup.isEmpty())
    {
        model->appendRow(item);
    }
    else
    {
        ensureTreeItem(parentGroup, items)->appendRow(item);
    }

    items.insert(settingsGroup, item);
    return item;
}

QcwPreferencesPage* QcwPreferencesWidgetPrivate::currentPage() const
{
    const auto index = treeView->currentIndex();
    if (!index.isValid())
    {
        return nullptr;
    }

    return pages.value(index.data(Qt::UserRole).toString());
}

void QcwPreferencesWidgetPrivate::showPage(const QModelIndex& index)
{
    clearPage();
    if (!index.isValid())
    {
        return;
    }

    QStringList names;
    for (auto current = index; current.isValid(); current = current.parent())
    {
        names.prepend(current.data().toString());
    }

    QString path;
    for (int i = 0; i < names.size(); ++i)
    {
        if (i > 0)
        {
            path += QStringLiteral("<font size='small'>&nbsp;&nbsp;&gt;&nbsp;&nbsp;</font>");
        }
        path += QStringLiteral("<b>%1</b>").arg(names.at(i).toHtmlEscaped());
    }
    pathLabel->setText(path);

    if (auto* page = pages.value(index.data(Qt::UserRole).toString()))
    {
        page->loadSettings();
        // setWidget() reparents the page to the viewport and shows it.
        rightPanel->setWidget(page);
        // The scroll area can shrink below its content, which would clip a form page or give
        // it a horizontal scrollbar. Keep the page's natural width as a floor: the splitter
        // narrows the tree instead, and tall pages still scroll vertically.
        rightPanel->setMinimumWidth(page->minimumSizeHint().width());
    }
}

void QcwPreferencesWidgetPrivate::clearPage()
{
    // takeWidget() detaches the page from the scroll area and hides it; the page stays owned
    // by QcwPreferencesManager and can be shown again later.
    rightPanel->takeWidget();
    rightPanel->setMinimumWidth(0);

    pathLabel->clear();
}

void QcwPreferencesWidgetPrivate::selectFirstPage()
{
    if (treeView->currentIndex().isValid())
    {
        return;
    }

    // Refiltering resets the proxy, which drops the current index and with it the page shown on
    // the right. A filter that matches again would therefore leave the panel empty; take the
    // first matching page instead.
    const auto index = firstPageIndex(*proxyModel);
    if (index.isValid())
    {
        treeView->setCurrentIndex(index);
    }
}

QT_END_NAMESPACE
