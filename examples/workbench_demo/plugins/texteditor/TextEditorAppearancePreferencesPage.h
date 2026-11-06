// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPreferencesPage.h"

#include <memory>

class QCheckBox;

namespace demo
{
class FontPreferenceRow;
} // namespace demo

/**
 * @brief The TextEditor appearance page: line numbers and the editor font.
 *
 * The font field works like the Appearance page's: it shows the family and point size, Select
 * Font changes both, and Default goes back to the system's fixed font. Both controls write
 * their value as soon as they change; settingsChanged() then lets the plugin's EditorService
 * push the new appearance into the editors that are already open.
 */
class TextEditorAppearancePreferencesPage : public QcwPreferencesPage
{
    Q_OBJECT
public:
    explicit TextEditorAppearancePreferencesPage(QWidget* parent = nullptr);
    ~TextEditorAppearancePreferencesPage() override;

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

    QCheckBox* mShowLineNumbers;
    std::unique_ptr<demo::FontPreferenceRow> mFontRow;
};
