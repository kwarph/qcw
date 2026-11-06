// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "PreferencesDialog.h"
#include "ui_PreferencesDialog.h"
#include <QJsonDocument>
#include <QFontDatabase>

PreferencesDialog::PreferencesDialog(QWidget* parent) :
    QDialog(parent), ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);

    buildThemes();
    connectWidgets();

    // Page 1 is the styled theme, matching the folder's initial StyledTabFolderRenderer.
    ui->comboBox->setCurrentIndex(1);
}

PreferencesDialog::~PreferencesDialog()
{
    delete ui;
}

void PreferencesDialog::buildThemes()
{
    {
        QJsonObject light;
        light["tabBarSpacing"] = 6;
        light["tabBarPadding"] = 6;
        light["tabBorderColor"] = "#ddd";
        light["tabBorderRadius"] = 4;
        light["hoveredTabBackground"] = "#f1f1f1";
        light["selectedTabBackground"] = "#e9e9e9";
        light["activeTabBackground"] = "#00e3e3";
        light["folderCss"] = R"(
QcwTabFolder {
    background-color: #fff;
    border-radius: 10px;
}
QcwTabFolder QToolButton {
    border-radius: 3px;
    border: none;
}
QcwTabFolder QToolButton:hover {
    background-color: #f1f1f1;
}
QcwTabFolder QToolButton:pressed {
    background-color: #e9e9e9;
}
QcwTabFolder QToolButton::menu-indicator {
    image: none;
    width: 0px;
    height: 0px;
}
QcwTabFolder QPlainTextEdit {
    border: none;
})";
        styledTheme["Light"] = light;

        QJsonObject dark;
        dark["tabBarSpacing"] = 6;
        dark["tabBarPadding"] = 6;
        dark["tabBorderColor"] = "#4f4f4f";
        dark["tabBorderRadius"] = 4;
        dark["hoveredTabBackground"] = "#3c3c3c";
        dark["selectedTabBackground"] = "#4f4f4f";
        dark["activeTabBackground"] = "#007979";
        dark["folderCss"] = R"(
QcwTabFolder {
    background-color: #000;
    border-radius: 10px;
}
QcwTabFolder QToolButton {
    border-radius: 3px;
    border: none;
}
QcwTabFolder QToolButton:hover {
    background-color: #3c3c3c;
}
QcwTabFolder QToolButton:pressed {
    background-color: #4f4f4f;
}
QcwTabFolder QToolButton::menu-indicator {
    image: none;
    width: 0px;
    height: 0px;
}
QcwTabFolder QPlainTextEdit {
    border: none;
})";
        styledTheme["Dark"] = dark;
    }

    {
        QJsonObject light;
        light["folderCss"] = R"(
QcwTabFolder {
    border: 1px solid palette(mid);
}
QcwTabFolder QToolButton::menu-indicator {
    image: none;
    width: 0px;
    height: 0px;
}
QcwTabFolder QPlainTextEdit {
    border: none;
})";
        defaultTheme["Light"] = light;

        QJsonObject dark;
        dark["folderCss"] = R"(
QcwTabFolder {
    border: 1px solid palette(mid);
}
QcwTabFolder QToolButton::menu-indicator {
    image: none;
    width: 0px;
    height: 0px;
}
QcwTabFolder QPlainTextEdit {
    border: none;
})";
        defaultTheme["Dark"] = dark;
    }
}

void PreferencesDialog::connectWidgets()
{
    ui->buttonApplyDefault->setEnabled(false);
    connect(ui->buttonApplyDefault, &QPushButton::clicked, [this](bool)
    {
        // JSON is UTF-8 by definition, so the text goes through toUtf8(), not toLocal8Bit().
        const auto doc = QJsonDocument::fromJson(ui->editDefault->toPlainText().toUtf8());
        defaultTheme = doc.object();
        emit styleChanged(true);
    });

    ui->buttonApplyStyled->setEnabled(false);
    connect(ui->buttonApplyStyled, &QPushButton::clicked, [this](bool)
    {
        const auto doc = QJsonDocument::fromJson(ui->editStyled->toPlainText().toUtf8());
        styledTheme = doc.object();
        emit styleChanged(false);
    });

    // The compiled-in themes are shown as the editors' initial text, before the textChanged
    // connections below, so seeding them does not mark the Apply buttons.
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    ui->editDefault->appendPlainText(QJsonDocument(defaultTheme).toJson());
    ui->editDefault->setFont(fixedFont);

    ui->editStyled->appendPlainText(QJsonDocument(styledTheme).toJson());
    ui->editStyled->setFont(fixedFont);

    connect(ui->comboBox, &QComboBox::currentIndexChanged, [this](int index)
    {
        ui->stackedWidget->setCurrentIndex(index);
        ui->buttonApplyDefault->setEnabled(true);
        ui->buttonApplyStyled->setEnabled(true);
    });

    connect(ui->comboBoxTabPos, &QComboBox::currentIndexChanged, [this](int index)
    {
        emit tabPositionChanged(index == 0);
    });

    connect(ui->editDefault, &QPlainTextEdit::textChanged, [this]()
    {
        ui->buttonApplyDefault->setEnabled(true);
    });

    connect(ui->editStyled, &QPlainTextEdit::textChanged, [this]()
    {
        ui->buttonApplyStyled->setEnabled(true);
    });
}
