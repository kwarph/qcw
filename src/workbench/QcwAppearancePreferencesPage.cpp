// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwAppearancePreferencesPage.h"

#include "QcwSettings.h"
#include "QcwThemeDialog.h"
#include "QcwThemeManager.h"
#include "QcwTranslations.h"
#include "QcwWindowStyler.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFontDialog>
#include <QFontInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMap>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyleHints>
#include <QVBoxLayout>

QT_BEGIN_NAMESPACE

namespace
{

constexpr QLatin1StringView kFollowSystemKey("appearance/themes/followSystem");
constexpr QLatin1StringView kLightThemeKey("appearance/themes/lightTheme");
constexpr QLatin1StringView kDarkThemeKey("appearance/themes/darkTheme");
constexpr QLatin1StringView kFontKey("appearance/font");
constexpr QLatin1StringView kWindowStyleKey("appearance/windowStyle/currentStyle");

constexpr QLatin1StringView kDefaultLightTheme("QcwLight");
constexpr QLatin1StringView kDefaultDarkTheme("QcwDark");

/// The application font from before a stored font was applied, for Restore Defaults.
QFont defaultApplicationFont()
{
    static const QFont font = QApplication::font();
    return font;
}

/// Applies the application font and repaints the widgets that are already there.
///
/// A font change only repaints the widgets whose laid-out size it changes; QMenuBar, for
/// one, recomputes its item rects but does not repaint itself. The preference has to show
/// immediately, so the widgets get an explicit repaint as well.
void applyApplicationFont(const QFont& font)
{
    QApplication::setFont(font);

    const auto widgets = QApplication::allWidgets();
    for (auto widget : widgets)
    {
        widget->update();
    }
}

struct LanguageOption
{
    QString localeName;
    QString displayName;
};

/// The locales QcwTranslations offers, each paired with the name to show in the selector.
QList<LanguageOption> availableLanguages()
{
    const QStringList locales = QcwTranslations::availableLocales();

    QMap<QLocale::Language, int> languageCounts;
    for (const QString& localeName : locales)
    {
        ++languageCounts[QLocale(localeName).language()];
    }

    QList<LanguageOption> options;
    for (const QString& localeName : locales)
    {
        const QLocale locale(localeName);
        QString displayName = locale.nativeLanguageName();
        if (displayName.isEmpty())
        {
            displayName = QLocale::languageToString(locale.language());
        }

        // "de" and "de_DE" would otherwise be indistinguishable.
        if (languageCounts.value(locale.language()) > 1)
        {
            const QString territoryName = locale.nativeTerritoryName();
            if (!territoryName.isEmpty())
            {
                displayName += QStringLiteral(" (%1)").arg(territoryName);
            }
        }

        options.append({ localeName, displayName });
    }

    return options;
}

/// The preferred theme for @p dark, falling back to the default theme when it is unavailable.
QString preferredTheme(bool dark)
{
    const auto settings = QcwSettings::get();
    const QString themeName = settings
                                  .value(dark ? kDarkThemeKey : kLightThemeKey,
                                         dark ? QString(kDefaultDarkTheme) : QString(kDefaultLightTheme))
                                  .toString();

    if (themeManager->hasTheme(themeName))
    {
        return themeName;
    }

    return themeManager->hasTheme(QcwThemeManager::DefaultThemeName) ? QString(QcwThemeManager::DefaultThemeName) :
                                                                       QString();
}

/// Applies the preferred theme matching the system color scheme. Unknown counts as light.
void applyPreferredTheme()
{
    const bool dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    const QString themeName = preferredTheme(dark);
    if (!themeName.isEmpty() && themeName != themeManager->currentTheme())
    {
        themeManager->changeTheme(themeName);
    }
}

} // namespace

// -----------------------------------------------------------------------------

class QcwAppearancePreferencesPagePrivate
{
public:
    explicit QcwAppearancePreferencesPagePrivate(QcwAppearancePreferencesPage* interface);

    void setupUi();
    void loadFromSettings();
    void retranslateUi();
    void populateThemeCombos();
    void populatePreferredThemeCombo(QComboBox* comboBox, QLatin1StringView key, QLatin1StringView defaultTheme);
    void populateStyleCombo();
    void populateLanguageCombo();
    void updateThemeControls(bool followSystem);
    void updateFontDisplay(const QFont& font);
    void applyDefaultFont();
    void setFollowSystem(bool followSystem);
    void savePreferredTheme(QComboBox* comboBox, QLatin1StringView key);
    void chooseFont();

public:
    QcwAppearancePreferencesPage* owner;
    QGroupBox* themesGroupBox { nullptr };
    QCheckBox* followOSCheckBox { nullptr };
    QLabel* themeLabel { nullptr };
    QComboBox* themeComboBox { nullptr };
    QPushButton* themesButton { nullptr };
    QLabel* lightLabel { nullptr };
    QComboBox* lightComboBox { nullptr };
    QLabel* darkLabel { nullptr };
    QComboBox* darkComboBox { nullptr };
    QGroupBox* styleGroupBox { nullptr };
    QLabel* styleLabel { nullptr };
    QComboBox* styleComboBox { nullptr };
    QGroupBox* interfaceGroupBox { nullptr };
    QLabel* langLabel { nullptr };
    QComboBox* langComboBox { nullptr };
    QLabel* fontLabel { nullptr };
    QLineEdit* fontEdit { nullptr };
    QPushButton* fontButton { nullptr };
    QPushButton* fontDefaultButton { nullptr };
};

// -----------------------------------------------------------------------------

QcwAppearancePreferencesPage::QcwAppearancePreferencesPage(QWidget* parent) :
    QcwPreferencesPage { parent }, impl { new QcwAppearancePreferencesPagePrivate { this } }
{
    impl->setupUi();

    // The page is created at startup and stays alive: it must know the persisted follow-system
    // state before the preferences dialog is ever opened, so system scheme changes are applied.
    impl->loadFromSettings();
}

QcwAppearancePreferencesPage::~QcwAppearancePreferencesPage()
{
    delete impl;
}

void QcwAppearancePreferencesPage::applyStoredSettings()
{
    // Capture the application's font before a stored font replaces it; restoreDefaults()
    // returns to that font.
    defaultApplicationFont();

    QcwTranslations::applyStoredLanguage();

    const auto settings = QcwSettings::get();
    QFont font;
    const QString serialized = settings.value(kFontKey).toString();
    if (!serialized.isEmpty() && font.fromString(serialized))
    {
        applyApplicationFont(font);
    }

    if (settings.value(kFollowSystemKey, false).toBool())
    {
        applyPreferredTheme();
    }
}

QString QcwAppearancePreferencesPage::settingsGroup() const
{
    return QLatin1String("appearance");
}

QString QcwAppearancePreferencesPage::name() const
{
    return tr("Appearance");
}

bool QcwAppearancePreferencesPage::matchesFilter(const QString& filterText) const
{
    const auto contains = [&filterText](const QString& text)
    {
        return text.contains(filterText, Qt::CaseInsensitive);
    };

    for (const auto* label : findChildren<QLabel*>())
    {
        if (contains(label->text()))
        {
            return true;
        }
    }

    for (const auto* button : findChildren<QAbstractButton*>())
    {
        if (contains(button->text()) || contains(button->toolTip()))
        {
            return true;
        }
    }

    for (const auto* groupBox : findChildren<QGroupBox*>())
    {
        if (contains(groupBox->title()))
        {
            return true;
        }
    }

    for (const auto* comboBox : findChildren<QComboBox*>())
    {
        for (int i = 0; i < comboBox->count(); ++i)
        {
            if (contains(comboBox->itemText(i)))
            {
                return true;
            }
        }
    }

    for (const auto* lineEdit : findChildren<QLineEdit*>())
    {
        if (contains(lineEdit->text()))
        {
            return true;
        }
    }

    return false;
}

void QcwAppearancePreferencesPage::restoreDefaults()
{
    auto settings = QcwSettings::get();
    settings.remove(kFollowSystemKey);
    settings.remove(kLightThemeKey);
    settings.remove(kDarkThemeKey);
    settings.remove(kWindowStyleKey);

    // The empty choice is "follow the system": setLanguage() clears the key and applies it.
    QcwTranslations::setLanguage(QString());
    impl->applyDefaultFont();

    // Refresh the controls, including the follow-system state, before choosing the default
    // theme: the themeChanged() handler must not re-apply a preferred theme.
    loadSettings();
    windowStyler->applyStyle();

    if (themeManager->hasTheme(QcwThemeManager::DefaultThemeName))
    {
        themeManager->changeTheme(QcwThemeManager::DefaultThemeName);
    }
}

void QcwAppearancePreferencesPage::loadSettings()
{
    impl->loadFromSettings();
}

void QcwAppearancePreferencesPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        impl->retranslateUi();
    }

    QcwPreferencesPage::changeEvent(event);
}

// -----------------------------------------------------------------------------

QcwAppearancePreferencesPagePrivate::QcwAppearancePreferencesPagePrivate(QcwAppearancePreferencesPage* interface) :
    owner { interface }
{
    // Capture the fallback font before the page itself can apply a stored or selected one.
    defaultApplicationFont();
}

void QcwAppearancePreferencesPagePrivate::loadFromSettings()
{
    const auto settings = QcwSettings::get();

    {
        const QSignalBlocker blocker(followOSCheckBox);
        followOSCheckBox->setChecked(settings.value(kFollowSystemKey, false).toBool());
    }

    populateThemeCombos();
    populateStyleCombo();
    populateLanguageCombo();

    QFont font;
    const QString serialized = settings.value(kFontKey).toString();
    if (serialized.isEmpty() || !font.fromString(serialized))
    {
        font = QApplication::font();
    }
    updateFontDisplay(font);

    updateThemeControls(followOSCheckBox->isChecked());
}

void QcwAppearancePreferencesPagePrivate::updateThemeControls(bool followSystem)
{
    lightLabel->setEnabled(followSystem);
    lightComboBox->setEnabled(followSystem);
    darkLabel->setEnabled(followSystem);
    darkComboBox->setEnabled(followSystem);

    themeComboBox->setEnabled(!followSystem);
    themesButton->setEnabled(!followSystem);
}

void QcwAppearancePreferencesPagePrivate::updateFontDisplay(const QFont& font)
{
    const QFontInfo info(font);

    QString text = info.family();
    if (info.pointSize() > 0)
    {
        text += QStringLiteral(" %1").arg(info.pointSize());
    }

    fontEdit->setText(text);
    fontEdit->setCursorPosition(0);
}

void QcwAppearancePreferencesPagePrivate::applyDefaultFont()
{
    // Dropping the key is what makes the page follow the default again: both loadFromSettings()
    // and chooseFont() fall back to QApplication::font() when nothing is stored.
    QcwSettings::get().remove(kFontKey);
    applyApplicationFont(defaultApplicationFont());
    updateFontDisplay(defaultApplicationFont());
}

void QcwAppearancePreferencesPagePrivate::populatePreferredThemeCombo(QComboBox* comboBox,
                                                                      QLatin1StringView key,
                                                                      QLatin1StringView defaultTheme)
{
    const QSignalBlocker blocker(comboBox);
    const QString stored = QcwSettings::get().value(key, QString(defaultTheme)).toString();

    comboBox->clear();
    for (const QString& themeName : themeManager->allThemes().keys())
    {
        comboBox->addItem(themeName);
    }

    int index = comboBox->findText(stored);
    if (index < 0)
    {
        index = comboBox->findText(QString(defaultTheme));
    }
    if (index < 0 && comboBox->count() > 0)
    {
        index = 0;
    }
    comboBox->setCurrentIndex(index);
}

void QcwAppearancePreferencesPagePrivate::populateThemeCombos()
{
    {
        const QSignalBlocker blocker(themeComboBox);
        themeComboBox->clear();
        for (const QString& themeName : themeManager->allThemes().keys())
        {
            themeComboBox->addItem(themeName);
        }
        themeComboBox->setCurrentIndex(themeComboBox->findText(themeManager->currentTheme()));
    }

    populatePreferredThemeCombo(lightComboBox, kLightThemeKey, kDefaultLightTheme);
    populatePreferredThemeCombo(darkComboBox, kDarkThemeKey, kDefaultDarkTheme);
}

void QcwAppearancePreferencesPagePrivate::populateStyleCombo()
{
    const QSignalBlocker blocker(styleComboBox);
    const QString currentStyle = windowStyler->currentStyle();

    styleComboBox->clear();
    for (const QString& styleName : windowStyler->allStyles())
    {
        styleComboBox->addItem(styleName);
    }

    int index = styleComboBox->findText(currentStyle);
    if (index < 0 && styleComboBox->count() > 0)
    {
        index = 0;
    }
    styleComboBox->setCurrentIndex(index);
}

void QcwAppearancePreferencesPagePrivate::populateLanguageCombo()
{
    const QString storedLocale = QcwTranslations::storedLanguage();

    const QSignalBlocker blocker(langComboBox);
    langComboBox->clear();
    langComboBox->addItem(QCoreApplication::translate("QcwAppearancePreferencesPage", "System Default"), QString());

    int selectedIndex = 0;
    for (const LanguageOption& option : availableLanguages())
    {
        langComboBox->addItem(option.displayName, option.localeName);
        if (option.localeName == storedLocale)
        {
            selectedIndex = langComboBox->count() - 1;
        }
    }

    langComboBox->setCurrentIndex(selectedIndex);
}

void QcwAppearancePreferencesPagePrivate::setFollowSystem(bool followSystem)
{
    QcwSettings::get().setValue(kFollowSystemKey, followSystem);
    updateThemeControls(followSystem);

    if (followSystem)
    {
        applyPreferredTheme();
    }
}

void QcwAppearancePreferencesPagePrivate::savePreferredTheme(QComboBox* comboBox, QLatin1StringView key)
{
    const QString themeName = comboBox->currentText();
    if (themeName.isEmpty())
    {
        return;
    }

    QcwSettings::get().setValue(key, themeName);

    if (followOSCheckBox->isChecked())
    {
        applyPreferredTheme();
    }
}

void QcwAppearancePreferencesPagePrivate::chooseFont()
{
    QFont font;
    const QString serialized = QcwSettings::get().value(kFontKey).toString();
    if (serialized.isEmpty() || !font.fromString(serialized))
    {
        font = QApplication::font();
    }

    bool accepted = false;
    const QFont selected = QFontDialog::getFont(
        &accepted, font, owner, QCoreApplication::translate("QcwAppearancePreferencesPage", "Select Font"));
    if (!accepted)
    {
        return;
    }

    applyApplicationFont(selected);
    QcwSettings::get().setValue(kFontKey, selected.toString());
    updateFontDisplay(selected);
}

void QcwAppearancePreferencesPagePrivate::retranslateUi()
{
    styleGroupBox->setTitle(QCoreApplication::translate("QcwAppearancePreferencesPage", "Window Styles"));
    styleLabel->setText(QCoreApplication::translate("QcwAppearancePreferencesPage", "Window Style:"));

    interfaceGroupBox->setTitle(QCoreApplication::translate("QcwAppearancePreferencesPage", "User Interface"));
    langLabel->setText(QCoreApplication::translate("QcwAppearancePreferencesPage", "Language:"));
    fontLabel->setText(QCoreApplication::translate("QcwAppearancePreferencesPage", "Font:"));
    fontButton->setText(QCoreApplication::translate("QcwAppearancePreferencesPage", "Select &Font..."));
    fontDefaultButton->setText(QCoreApplication::translate("QcwAppearancePreferencesPage", "&Default"));
#if QT_CONFIG(tooltip)
    fontDefaultButton->setToolTip(
        QCoreApplication::translate("QcwAppearancePreferencesPage", "Reset to the default font"));
#endif // QT_CONFIG(tooltip)
    themesGroupBox->setTitle(QCoreApplication::translate("QcwAppearancePreferencesPage", "Themes"));
    themeLabel->setText(QCoreApplication::translate("QcwAppearancePreferencesPage", "Theme:"));
    themesButton->setText(QCoreApplication::translate("QcwAppearancePreferencesPage", "&Themes..."));
#if QT_CONFIG(tooltip)
    themesButton->setToolTip(QCoreApplication::translate("QcwAppearancePreferencesPage", "Theme management"));
#endif // QT_CONFIG(tooltip)
    lightLabel->setText(QCoreApplication::translate("QcwAppearancePreferencesPage", "Preferred light theme:"));
    darkLabel->setText(QCoreApplication::translate("QcwAppearancePreferencesPage", "Preferred dark theme:"));
    followOSCheckBox->setText(QCoreApplication::translate("QcwAppearancePreferencesPage",
                                                          "Detect &system theme changes and follow the system theme"));

    // Item texts are endonyms, so only the system entry follows the UI language.
    const QSignalBlocker blocker(langComboBox);
    if (langComboBox->count() > 0)
    {
        langComboBox->setItemText(0, QCoreApplication::translate("QcwAppearancePreferencesPage", "System Default"));
    }
}

void QcwAppearancePreferencesPagePrivate::setupUi()
{
    auto mainLayout = new QVBoxLayout(owner);
    mainLayout->setSpacing(12);

    themesGroupBox = new QGroupBox(owner);
    auto themesLayout = new QVBoxLayout(themesGroupBox);
    themesLayout->setSpacing(9);
    followOSCheckBox = new QCheckBox(themesGroupBox);

    themesLayout->addWidget(followOSCheckBox);

    auto formLayout = new QFormLayout();
    themeLabel = new QLabel(themesGroupBox);
    themeLabel->setAlignment(Qt::AlignmentFlag::AlignLeading | Qt::AlignmentFlag::AlignLeft |
                             Qt::AlignmentFlag::AlignVCenter);

    formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, themeLabel);

    auto themeRow = new QHBoxLayout();
    themeComboBox = new QComboBox(themesGroupBox);
    QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(themeComboBox->sizePolicy().hasHeightForWidth());
    themeComboBox->setSizePolicy(sizePolicy);

    themeRow->addWidget(themeComboBox);

    themesButton = new QPushButton(themesGroupBox);

    themeRow->addWidget(themesButton);

    formLayout->setLayout(0, QFormLayout::ItemRole::FieldRole, themeRow);

    auto line = new QFrame(themesGroupBox);
    line->setFrameShape(QFrame::Shape::HLine);
    line->setFrameShadow(QFrame::Shadow::Sunken);

    formLayout->setWidget(1, QFormLayout::ItemRole::SpanningRole, line);

    lightLabel = new QLabel(themesGroupBox);
    lightLabel->setAlignment(Qt::AlignmentFlag::AlignLeading | Qt::AlignmentFlag::AlignLeft |
                             Qt::AlignmentFlag::AlignVCenter);

    formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, lightLabel);

    lightComboBox = new QComboBox(themesGroupBox);
    sizePolicy.setHeightForWidth(lightComboBox->sizePolicy().hasHeightForWidth());
    lightComboBox->setSizePolicy(sizePolicy);

    formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, lightComboBox);

    darkLabel = new QLabel(themesGroupBox);
    darkLabel->setAlignment(Qt::AlignmentFlag::AlignLeading | Qt::AlignmentFlag::AlignLeft |
                            Qt::AlignmentFlag::AlignVCenter);

    formLayout->setWidget(3, QFormLayout::ItemRole::LabelRole, darkLabel);

    darkComboBox = new QComboBox(themesGroupBox);
    sizePolicy.setHeightForWidth(darkComboBox->sizePolicy().hasHeightForWidth());
    darkComboBox->setSizePolicy(sizePolicy);

    formLayout->setWidget(3, QFormLayout::ItemRole::FieldRole, darkComboBox);

    themesLayout->addLayout(formLayout);

    mainLayout->addWidget(themesGroupBox);

    styleGroupBox = new QGroupBox(owner);
    auto styleFormLayout = new QFormLayout(styleGroupBox);

    styleLabel = new QLabel(styleGroupBox);
    styleLabel->setAlignment(Qt::AlignmentFlag::AlignLeading | Qt::AlignmentFlag::AlignLeft |
                             Qt::AlignmentFlag::AlignVCenter);

    styleFormLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, styleLabel);

    styleComboBox = new QComboBox(styleGroupBox);
    sizePolicy.setHeightForWidth(styleComboBox->sizePolicy().hasHeightForWidth());
    styleComboBox->setSizePolicy(sizePolicy);
    styleComboBox->setMinimumSize(QSize(200, 0));

    styleFormLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, styleComboBox);

    mainLayout->addWidget(styleGroupBox);

    interfaceGroupBox = new QGroupBox(owner);
    auto uiFormLayout = new QFormLayout(interfaceGroupBox);
    langLabel = new QLabel(interfaceGroupBox);
    langLabel->setAlignment(Qt::AlignmentFlag::AlignLeading | Qt::AlignmentFlag::AlignLeft |
                            Qt::AlignmentFlag::AlignVCenter);

    uiFormLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, langLabel);

    langComboBox = new QComboBox(interfaceGroupBox);
    sizePolicy.setHeightForWidth(langComboBox->sizePolicy().hasHeightForWidth());
    langComboBox->setSizePolicy(sizePolicy);
    langComboBox->setMinimumSize(QSize(200, 0));

    uiFormLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, langComboBox);

    fontLabel = new QLabel(interfaceGroupBox);
    fontLabel->setAlignment(Qt::AlignmentFlag::AlignLeading | Qt::AlignmentFlag::AlignLeft |
                            Qt::AlignmentFlag::AlignVCenter);

    uiFormLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, fontLabel);

    auto fontRow = new QHBoxLayout();
    fontEdit = new QLineEdit(interfaceGroupBox);
    sizePolicy.setHeightForWidth(fontEdit->sizePolicy().hasHeightForWidth());
    fontEdit->setSizePolicy(sizePolicy);
    fontEdit->setMinimumSize(QSize(270, 0));
    fontEdit->setReadOnly(true);

    fontRow->addWidget(fontEdit);

    fontButton = new QPushButton(interfaceGroupBox);

    fontRow->addWidget(fontButton);

    fontDefaultButton = new QPushButton(interfaceGroupBox);

    fontRow->addWidget(fontDefaultButton);

    uiFormLayout->setLayout(1, QFormLayout::ItemRole::FieldRole, fontRow);

    mainLayout->addWidget(interfaceGroupBox);

    auto vSpacer = new QSpacerItem(20, 18, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

    mainLayout->addItem(vSpacer);

    QObject::connect(followOSCheckBox, &QCheckBox::checkStateChanged, owner, [this](Qt::CheckState state)
    {
        setFollowSystem(state == Qt::Checked);
    });

    QObject::connect(themeComboBox, &QComboBox::currentTextChanged, owner, [this](const QString& themeName)
    {
        if (followOSCheckBox->isChecked() || themeName.isEmpty())
        {
            return;
        }

        if (!themeManager->changeTheme(themeName))
        {
            // The theme vanished between populating the combo and the selection.
            populateThemeCombos();
        }
    });

    QObject::connect(lightComboBox, &QComboBox::currentTextChanged, owner, [this](const QString&)
    {
        savePreferredTheme(lightComboBox, kLightThemeKey);
    });

    QObject::connect(darkComboBox, &QComboBox::currentTextChanged, owner, [this](const QString&)
    {
        savePreferredTheme(darkComboBox, kDarkThemeKey);
    });

    QObject::connect(langComboBox, &QComboBox::currentIndexChanged, owner, [this](int index)
    {
        if (index < 0)
        {
            return;
        }

        // The empty item data is "System Default": stored as no choice, applied as the system.
        QcwTranslations::setLanguage(langComboBox->itemData(index).toString());
    });

    QObject::connect(fontButton, &QPushButton::clicked, owner, [this]()
    {
        chooseFont();
    });

    QObject::connect(fontDefaultButton, &QPushButton::clicked, owner, [this]()
    {
        applyDefaultFont();
    });

    QObject::connect(themesButton, &QPushButton::clicked, owner, []
    {
        auto* dialog = new QcwThemeDialog;
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });

    QObject::connect(themeManager, &QcwThemeManager::themeAdded, owner, [this](const QString&)
    {
        populateThemeCombos();

        // The preferred theme may have become available.
        if (followOSCheckBox->isChecked())
        {
            applyPreferredTheme();
        }
    });

    QObject::connect(themeManager, &QcwThemeManager::themeChanged, owner, [this]()
    {
        populateThemeCombos();

        // Another component changed the theme; while following, the system-matched theme wins.
        if (followOSCheckBox->isChecked())
        {
            applyPreferredTheme();
        }
    });

    QObject::connect(styleComboBox, &QComboBox::currentTextChanged, owner, [](const QString& styleName)
    {
        if (!styleName.isEmpty())
        {
            windowStyler->setCurrentStyle(styleName);
        }
    });

    QObject::connect(windowStyler, &QcwWindowStyler::stylesChanged, owner, [this]()
    {
        populateStyleCombo();
    });

    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, owner, [this](Qt::ColorScheme)
    {
        if (followOSCheckBox->isChecked())
        {
            applyPreferredTheme();
        }
    });

    retranslateUi();
    populateThemeCombos();
    populateStyleCombo();
    populateLanguageCombo();
    updateFontDisplay(QApplication::font());
    updateThemeControls(followOSCheckBox->isChecked());
}

QT_END_NAMESPACE
