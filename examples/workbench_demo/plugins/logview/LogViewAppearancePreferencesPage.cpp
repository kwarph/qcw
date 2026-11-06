// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "LogViewAppearancePreferencesPage.h"

#include "DemoFontPreferenceRow.h"
#include "DemoFonts.h"
#include "DemoServices.h"
#include "QcwSettings.h"

#include <QEvent>
#include <QFormLayout>
#include <QSettings>

namespace
{
constexpr QLatin1String FontKey { "font" };
} // namespace

LogViewAppearancePreferencesPage::LogViewAppearancePreferencesPage(QWidget* parent) :
    QcwPreferencesPage { parent }
{
    auto* layout = new QFormLayout { this };

    mFontRow = std::make_unique<demo::FontPreferenceRow>(this, layout, QString {},
                                                          [this](const QFont&)
                                                          {
                                                              writeSettings();
                                                          });

    retranslateUi();
}

LogViewAppearancePreferencesPage::~LogViewAppearancePreferencesPage() = default;

void LogViewAppearancePreferencesPage::retranslateUi()
{
    mFontRow->retranslateUi(tr("Font:"));
}

void LogViewAppearancePreferencesPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }

    QcwPreferencesPage::changeEvent(event);
}

QString LogViewAppearancePreferencesPage::settingsGroup() const
{
    return QString(demo::LogViewSettingsGroup);
}

QString LogViewAppearancePreferencesPage::name() const
{
    return tr("Log Viewer");
}

bool LogViewAppearancePreferencesPage::matchesFilter(const QString& filterText) const
{
    const QString searchable = QStringList {
        name(),
        tr("Font"),
        tr("Monospace"),
    }
                                   .join(QLatin1Char(' '));

    return searchable.contains(filterText, Qt::CaseInsensitive);
}

void LogViewAppearancePreferencesPage::restoreDefaults()
{
    mFontRow->setFont(demo::defaultMonospaceFont());
    writeSettings();
}

void LogViewAppearancePreferencesPage::loadSettings()
{
    QSettings settings = QcwSettings::get();
    settings.beginGroup(settingsGroup());
    const bool hasFont = settings.contains(QString(FontKey));
    const QFont storedFont = hasFont ? settings.value(QString(FontKey)).value<QFont>() : QFont();
    settings.endGroup();

    // A missing key means first use, where the log starts from the system's fixed font.
    mFontRow->setFont(hasFont ? storedFont : demo::defaultMonospaceFont());
}

void LogViewAppearancePreferencesPage::writeSettings()
{
    QSettings settings = QcwSettings::get();
    settings.beginGroup(settingsGroup());
    settings.setValue(QString(FontKey), mFontRow->font());
    settings.endGroup();

    emit settingsChanged();
}
