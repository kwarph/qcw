// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPreferencesPage.h"

#include <memory>

namespace demo
{
class FontPreferenceRow;
} // namespace demo

/**
 * @brief The LogView appearance page: the font the log pane is drawn in.
 *
 * The font field works like the Appearance page's: it shows the family and point size, Select
 * Font changes both, and Default goes back to the system's fixed font. A change is written as
 * soon as it is made; settingsChanged() then lets the plugin push the font into the log views
 * that are already open.
 */
class LogViewAppearancePreferencesPage : public QcwPreferencesPage
{
    Q_OBJECT
public:
    explicit LogViewAppearancePreferencesPage(QWidget* parent = nullptr);
    ~LogViewAppearancePreferencesPage() override;

    QString settingsGroup() const override;
    QString name() const override;
    bool matchesFilter(const QString& filterText) const override;
    void restoreDefaults() override;
    void loadSettings() override;

signals:
    /// Emitted after the page wrote a changed value; the page itself is the context.
    void settingsChanged();

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void writeSettings();

    std::unique_ptr<demo::FontPreferenceRow> mFontRow;
};
