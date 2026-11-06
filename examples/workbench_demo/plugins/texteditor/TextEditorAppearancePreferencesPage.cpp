// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "TextEditorAppearancePreferencesPage.h"

#include "DemoFontPreferenceRow.h"
#include "DemoFonts.h"
#include "DemoServices.h"
#include "QcwSettings.h"

#include <QCheckBox>
#include <QEvent>
#include <QFormLayout>
#include <QSettings>
#include <QSignalBlocker>

namespace
{
constexpr QLatin1String ShowLineNumberKey { "showLineNumber" };
constexpr QLatin1String FontKey { "font" };
} // namespace

TextEditorAppearancePreferencesPage::TextEditorAppearancePreferencesPage(QWidget* parent) :
    QcwPreferencesPage { parent },
    mShowLineNumbers { new QCheckBox { this } }
{
    auto* layout = new QFormLayout { this };
    layout->addRow(QString(), mShowLineNumbers);

    mFontRow = std::make_unique<demo::FontPreferenceRow>(this, layout, QString {},
                                                          [this](const QFont&)
                                                          {
                                                              writeSettings();
                                                          });

    connect(mShowLineNumbers, &QCheckBox::toggled, this, &TextEditorAppearancePreferencesPage::writeSettings);

    retranslateUi();
}

TextEditorAppearancePreferencesPage::~TextEditorAppearancePreferencesPage() = default;

void TextEditorAppearancePreferencesPage::retranslateUi()
{
    mShowLineNumbers->setText(tr("Show line numbers"));
    mFontRow->retranslateUi(tr("Font:"));
}

void TextEditorAppearancePreferencesPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }

    QcwPreferencesPage::changeEvent(event);
}

QString TextEditorAppearancePreferencesPage::settingsGroup() const
{
    return QString(demo::TextEditorSettingsGroup);
}

QString TextEditorAppearancePreferencesPage::name() const
{
    return tr("Text Editor");
}

bool TextEditorAppearancePreferencesPage::matchesFilter(const QString& filterText) const
{
    const QString searchable = QStringList {
        name(),
        mShowLineNumbers->text(),
        tr("Font"),
        tr("Monospace"),
    }
                                   .join(QLatin1Char(' '));

    return searchable.contains(filterText, Qt::CaseInsensitive);
}

void TextEditorAppearancePreferencesPage::restoreDefaults()
{
    {
        const QSignalBlocker showBlocker { mShowLineNumbers };
        mShowLineNumbers->setChecked(true);
        mFontRow->setFont(demo::defaultMonospaceFont());
    }

    writeSettings();
}

void TextEditorAppearancePreferencesPage::loadSettings()
{
    QSettings settings = QcwSettings::get();
    settings.beginGroup(settingsGroup());
    const bool showLineNumbers = settings.value(QString(ShowLineNumberKey), true).toBool();
    const bool hasFont = settings.contains(QString(FontKey));
    const QFont storedFont = hasFont ? settings.value(QString(FontKey)).value<QFont>() : QFont();
    settings.endGroup();

    // A missing key means first use, where the editor starts from the system's fixed font.
    const QFont font = hasFont ? storedFont : demo::defaultMonospaceFont();

    const QSignalBlocker showBlocker { mShowLineNumbers };
    mShowLineNumbers->setChecked(showLineNumbers);
    mFontRow->setFont(font);
}

void TextEditorAppearancePreferencesPage::writeSettings()
{
    QSettings settings = QcwSettings::get();
    settings.beginGroup(settingsGroup());
    settings.setValue(QString(ShowLineNumberKey), mShowLineNumbers->isChecked());
    settings.setValue(QString(FontKey), mFontRow->font());
    settings.endGroup();

    emit settingsChanged();
}
