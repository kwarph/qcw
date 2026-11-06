// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwThemeDialog.h"

#include "QcwFixedRowHeightItemDelegate.h"
#include "QcwPathUtils.h"
#include "QcwThemeManager.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

QT_BEGIN_NAMESPACE

namespace
{

constexpr QLatin1StringView kErrorLabelStyle("color: #e42217;");

bool isThemeDirectory(const QString& themeDir)
{
    return !themeDir.isEmpty() && QFileInfo(QDir(themeDir).filePath(QcwThemeManager::PaletteFileName)).isFile();
}

// -----------------------------------------------------------------------------

/// One row per registered theme; column 0's checkbox mirrors QcwThemeManager::currentTheme().
class QcwThemeModel : public QStandardItemModel
{
public:
    explicit QcwThemeModel(QObject* parent = nullptr);

    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

private:
    void appendTheme(const QString& themeName, const QString& themeDir);
    QString themeNameAt(const QModelIndex& index) const;
    void refreshCheckStates();
};

QcwThemeModel::QcwThemeModel(QObject* parent) :
    QStandardItemModel { parent }
{
    for (const auto& [themeName, themeDir] : themeManager->allThemes().asKeyValueRange())
    {
        appendTheme(themeName, themeDir);
    }

    connect(themeManager, &QcwThemeManager::themeAdded, this, [this](const QString& themeName)
    {
        appendTheme(themeName, themeManager->themeDirectory(themeName));
    });
    connect(themeManager, &QcwThemeManager::themeChanged, this, &QcwThemeModel::refreshCheckStates);
}

Qt::ItemFlags QcwThemeModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags itemFlags = QStandardItemModel::flags(index);
    if (index.isValid() && index.column() == 0)
    {
        itemFlags |= Qt::ItemIsUserCheckable;
    }
    return itemFlags;
}

QVariant QcwThemeModel::data(const QModelIndex& index, int role) const
{
    if (index.isValid() && index.column() == 0 && role == Qt::CheckStateRole)
    {
        return themeNameAt(index) == themeManager->currentTheme() ? Qt::Checked : Qt::Unchecked;
    }

    return QStandardItemModel::data(index, role);
}

bool QcwThemeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.column() != 0 || role != Qt::CheckStateRole)
    {
        return false;
    }

    if (value.toInt() == Qt::Checked)
    {
        const bool applied = themeManager->changeTheme(themeNameAt(index));
        if (!applied)
        {
            refreshCheckStates();
        }
        return applied;
    }

    // Only the current theme is checked, and clearing it would leave no current theme. The
    // delegate has already toggled the box off, so repaint it from the manager's state.
    refreshCheckStates();
    return false;
}

void QcwThemeModel::appendTheme(const QString& themeName, const QString& themeDir)
{
    const QString directory = QDir::toNativeSeparators(themeDir);

    auto* nameItem = new QStandardItem(themeName);
    auto* directoryItem = new QStandardItem(directory);
    directoryItem->setToolTip(directory);

    appendRow({ nameItem, directoryItem });
}

QString QcwThemeModel::themeNameAt(const QModelIndex& index) const
{
    return QStandardItemModel::data(index.siblingAtColumn(0), Qt::DisplayRole).toString();
}

void QcwThemeModel::refreshCheckStates()
{
    if (rowCount() > 0)
    {
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), { Qt::CheckStateRole });
    }
}

// -----------------------------------------------------------------------------

/// Name and directory gathered by QcwNewThemeDialog.
struct ThemeInfo
{
    QString name;
    QString directory;
};

/// Collects the name and directory of a theme the user wants to register.
class QcwNewThemeDialog : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(QcwNewThemeDialog)

public:
    explicit QcwNewThemeDialog(QWidget* parent = nullptr);

    void accept() override;

protected:
    /// Re-sets the labels, the browse tooltip and the title, and re-runs the validation.
    void changeEvent(QEvent* event) override;

private:
    ThemeInfo themeInfo() const;
    void validate();
    void retranslateUi();

private:
    QLabel* mNameLabel = nullptr;
    QLineEdit* mNameEdit = nullptr;
    QLabel* mDirectoryLabel = nullptr;
    QLineEdit* mDirectoryEdit = nullptr;
    QPushButton* mBrowseButton = nullptr;
    QLabel* mErrorLabel = nullptr;
    QPushButton* mOkButton = nullptr;
};

QcwNewThemeDialog::QcwNewThemeDialog(QWidget* parent) :
    QDialog { parent }
{
    auto* verticalLayout = new QVBoxLayout(this);

    mNameLabel = new QLabel(this);
    mNameEdit = new QLineEdit(this);

    mDirectoryLabel = new QLabel(this);
    mDirectoryEdit = new QLineEdit(this);
    mDirectoryEdit->setReadOnly(true);

    mBrowseButton = new QPushButton(this);
    mBrowseButton->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen));

    auto* gridLayout = new QGridLayout();
    gridLayout->addWidget(mNameLabel, 0, 0);
    gridLayout->addWidget(mNameEdit, 0, 1, 1, 2);
    gridLayout->addWidget(mDirectoryLabel, 1, 0);
    gridLayout->addWidget(mDirectoryEdit, 1, 1);
    gridLayout->addWidget(mBrowseButton, 1, 2);
    gridLayout->setColumnStretch(1, 1);

    verticalLayout->addLayout(gridLayout);

    mErrorLabel = new QLabel(this);
    mErrorLabel->setStyleSheet(QString(kErrorLabelStyle));
    mErrorLabel->setWordWrap(true);
    verticalLayout->addWidget(mErrorLabel);

    verticalLayout->addStretch();

    auto* buttonBox = new QDialogButtonBox(this);
    buttonBox->setOrientation(Qt::Orientation::Horizontal);
    buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel | QDialogButtonBox::StandardButton::Ok);
    mOkButton = buttonBox->button(QDialogButtonBox::StandardButton::Ok);
    mOkButton->setEnabled(false);

    verticalLayout->addWidget(buttonBox);

    connect(mNameEdit, &QLineEdit::textChanged, this, [this](const QString&)
    {
        validate();
    });
    connect(mDirectoryEdit, &QLineEdit::textChanged, this, [this](const QString&)
    {
        validate();
    });

    connect(mBrowseButton, &QPushButton::clicked, this, [this](bool)
    {
        const QString directory =
            QFileDialog::getExistingDirectory(this, tr("Select Theme Directory"), QcwLastVisitPath::get());

        if (!directory.isEmpty())
        {
            mDirectoryEdit->setText(directory);
            QcwLastVisitPath::set(directory);
        }
    });

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]()
    {
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, qOverload<>(&QDialog::reject));

    retranslateUi();
    resize(480, 170);
}

void QcwNewThemeDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }

    QDialog::changeEvent(event);
}

void QcwNewThemeDialog::retranslateUi()
{
    mNameLabel->setText(tr("Theme name:"));
    mDirectoryLabel->setText(tr("Theme directory:"));
    mBrowseButton->setToolTip(tr("Select a theme directory containing QcwPalette.theme"));
    setWindowTitle(tr("New Theme"));

    // The error label holds a message validate() chose, so it is resolved again by running
    // the check rather than by translating the text that happens to be on screen.
    validate();
}

ThemeInfo QcwNewThemeDialog::themeInfo() const
{
    return { mNameEdit->text().trimmed(), mDirectoryEdit->text() };
}

void QcwNewThemeDialog::accept()
{
    const ThemeInfo info = themeInfo();
    if (!themeManager->registerTheme(info.name, info.directory))
    {
        // The registry can change after validate() ran; refresh the message and keep the
        // dialog open so the user can correct the input.
        validate();
        if (mErrorLabel->text().isEmpty())
        {
            mErrorLabel->setText(tr("The theme could not be registered."));
        }
        return;
    }

    QDialog::accept();
}

void QcwNewThemeDialog::validate()
{
    const QString name = mNameEdit->text().trimmed();
    const QString directory = mDirectoryEdit->text();

    QString error;
    if (name.isEmpty())
    {
        error = tr("The theme name cannot be empty.");
    }
    else if (themeManager->hasTheme(name))
    {
        error = tr("A theme named \"%1\" already exists.").arg(name);
    }
    else if (directory.isEmpty())
    {
        error = tr("Select a theme directory containing QcwPalette.theme.");
    }
    else if (themeManager->isThemeDirectoryUsed(directory))
    {
        error = tr("The theme directory \"%1\" is used by an existing theme.").arg(directory);
    }
    else if (!isThemeDirectory(directory))
    {
        error = tr("QcwPalette.theme was not found in the selected directory.");
    }

    mErrorLabel->setText(error);
    mOkButton->setEnabled(error.isEmpty());
}

} // namespace

// -----------------------------------------------------------------------------

class QcwThemeDialogPrivate
{
public:
    explicit QcwThemeDialogPrivate(QcwThemeDialog* interface) :
        owner { interface }
    {
    }

    /// Re-applies the texts the constructor captured, for the language now installed.
    void retranslateUi();

public:
    QcwThemeDialog* owner;
    QLabel* hintLabel { nullptr };
    QStandardItemModel* model { nullptr };
    QPushButton* addButton { nullptr };
    QPushButton* removeButton { nullptr };
};

void QcwThemeDialogPrivate::retranslateUi()
{
    hintLabel->setText(
        QCoreApplication::translate("QcwThemeDialog", "Check a theme's checkbox or double-click its row to make it "
                                                      "current."));
    model->setHorizontalHeaderLabels(
        { QCoreApplication::translate("QcwThemeDialog", "Theme"), QCoreApplication::translate("QcwThemeDialog", "Path") });
    addButton->setText(QCoreApplication::translate("QcwThemeDialog", "&Add"));
    removeButton->setText(QCoreApplication::translate("QcwThemeDialog", "&Remove"));
    owner->setWindowTitle(QCoreApplication::translate("QcwThemeDialog", "Themes"));
}

QcwThemeDialog::QcwThemeDialog(QWidget* parent) :
    QDialog { parent }, impl { new QcwThemeDialogPrivate { this } }
{
    auto* verticalLayout = new QVBoxLayout(this);
    verticalLayout->setSpacing(9);

    impl->hintLabel = new QLabel(this);
    impl->hintLabel->setWordWrap(true);
    verticalLayout->addWidget(impl->hintLabel);

    auto* tableView = new QTableView(this);
    impl->model = new QcwThemeModel(this);
    impl->model->setColumnCount(2);

    tableView->setModel(impl->model);
    tableView->setItemDelegate(new QcwFixedRowHeightItemDelegate(tableView));
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView->setSortingEnabled(true);
    tableView->sortByColumn(0, Qt::AscendingOrder);

    tableView->setFrameShape(QFrame::StyledPanel);
    tableView->setFrameShadow(QFrame::Sunken);

    tableView->setColumnWidth(0, 150);
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->horizontalHeader()->setHighlightSections(false);

    tableView->verticalHeader()->setVisible(false);
    tableView->verticalHeader()->setDefaultSectionSize(QcwFixedRowHeightItemDelegate::RowHeight);

    verticalLayout->addWidget(tableView);

    auto* buttonLayout = new QHBoxLayout();
    impl->addButton = new QPushButton(this);
    impl->removeButton = new QPushButton(this);
    impl->removeButton->setEnabled(false);

    buttonLayout->addWidget(impl->addButton);
    buttonLayout->addWidget(impl->removeButton);
    buttonLayout->addStretch();

    auto* buttonBox = new QDialogButtonBox(this);
    buttonBox->setOrientation(Qt::Orientation::Horizontal);
    buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Close);
    buttonLayout->addWidget(buttonBox);

    verticalLayout->addLayout(buttonLayout);

    auto* model = impl->model;
    auto* addButton = impl->addButton;
    auto* removeButton = impl->removeButton;

    const auto updateRemoveButton = [this, tableView, removeButton]()
    {
        const QModelIndex current = tableView->currentIndex();
        bool removable = current.isValid();
        if (removable)
        {
            const QString themeName = current.siblingAtColumn(0).data(Qt::DisplayRole).toString();
            removable = themeName != themeManager->currentTheme() && themeName != QcwThemeManager::DefaultThemeName;
        }
        removeButton->setEnabled(removable);
    };

    connect(tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [updateRemoveButton](const QModelIndex&, const QModelIndex&)
    {
        updateRemoveButton();
    });
    connect(themeManager, &QcwThemeManager::themeChanged, this, [updateRemoveButton]()
    {
        updateRemoveButton();
    });

    connect(addButton, &QPushButton::clicked, this, [this](bool)
    {
        auto* dialog = new QcwNewThemeDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->open();
    });

    connect(removeButton, &QPushButton::clicked, this, [tableView, model](bool)
    {
        const QModelIndex current = tableView->currentIndex();
        if (!current.isValid())
        {
            return;
        }

        const QString themeName = current.siblingAtColumn(0).data(Qt::DisplayRole).toString();

        // A second theme dialog may have removed the theme already; drop the stale row too.
        if (themeManager->removeTheme(themeName) || !themeManager->hasTheme(themeName))
        {
            model->removeRow(current.row());
        }
    });

    connect(tableView, &QTableView::doubleClicked, this, [model](const QModelIndex& index)
    {
        if (index.isValid())
        {
            model->setData(index.siblingAtColumn(0), Qt::Checked, Qt::CheckStateRole);
        }
    });

    connect(buttonBox, &QDialogButtonBox::rejected, this, qOverload<>(&QDialog::reject));

    impl->retranslateUi();
    resize(560, 360);
}

QcwThemeDialog::~QcwThemeDialog()
{
    delete impl;
}

void QcwThemeDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        impl->retranslateUi();
    }

    QDialog::changeEvent(event);
}

QT_END_NAMESPACE
