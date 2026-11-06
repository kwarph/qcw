// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QDialog>
#include <QJsonObject>

namespace Ui
{
class PreferencesDialog;
}

/// Settings dialog of the renderer demo: picks the tab position, edits the two embedded
/// JSON themes and swaps the folder between them.
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override;

    const QJsonObject& themeData(bool isDefault) const
    {
        return isDefault ? defaultTheme : styledTheme;
    }

signals:
    void tabPositionChanged(bool atTop);
    void styleChanged(bool isDefault);

private:
    /// Builds the two compiled-in theme objects.
    void buildThemes();
    /// Connects the widgets and seeds their initial values. Called once, from the
    /// constructor, so a connection can never be installed twice.
    void connectWidgets();

    Ui::PreferencesDialog* ui;
    QJsonObject defaultTheme;
    QJsonObject styledTheme;
};
