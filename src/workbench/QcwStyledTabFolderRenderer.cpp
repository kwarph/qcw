// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwStyledTabFolderRenderer.h"
#include "QcwThemeManager.h"
#include "QcwWidgetTheme.h"
#include "QcwTabFolder.h"
#include "QcwThemeUtils.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QJsonObject>
#include <QRegularExpression>

QT_BEGIN_NAMESPACE

namespace
{

QString defaultFolderCss()
{
    return QStringLiteral(R"(
QcwTabFolder {
    background-color: #ffffff;
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
    background-color: #e3e3e3;
}
QcwTabFolder QToolButton::menu-indicator {
    image: none;
    width: 0px;
    height: 0px;
})");
}

/**
 * @struct FolderFrame
 * @brief Corner radius and border read from the folder stylesheet.
 *
 * @details The overlays have to reproduce the shape the stylesheet makes the folder paint, so
 *          folderCss is their only source. A theme that changes the border or the radius there
 *          gets matching overlays without repeating the values in separate keys.
 */
struct FolderFrame
{
    int radius { 0 };
    qreal borderWidth { 0 };
    QColor borderColor;
};

bool isBorderStyleKeyword(const QString& value)
{
    static const QStringList keywords { "none",   "hidden", "dotted", "dashed", "solid",
                                        "double", "groove", "ridge",  "inset",  "outset" };
    return keywords.contains(value);
}

qreal parseLength(const QString& value)
{
    QString length = value.trimmed();
    if (length.endsWith(QLatin1String("px"), Qt::CaseInsensitive))
    {
        length.chop(2);
    }

    bool ok = false;
    const qreal number = length.toDouble(&ok);
    return ok ? number : -1;
}

FolderFrame parseFolderFrame(const QString& css)
{
    FolderFrame frame;

    static const QRegularExpression folderRule(QStringLiteral(R"((?:^|[^,\w])QcwTabFolder\s*\{([^}]*)\})"));
    static const QRegularExpression borderStyle(QStringLiteral(R"(^border-(?:left|right|top|bottom)$)"));

    const QRegularExpressionMatch match = folderRule.match(css);
    if (!match.hasMatch())
    {
        return frame;
    }

    for (const QString& declaration : match.captured(1).split(QLatin1Char(';'), Qt::SkipEmptyParts))
    {
        const int colon = declaration.indexOf(QLatin1Char(':'));
        if (colon < 0)
        {
            continue;
        }

        const QString name = declaration.left(colon).trimmed().toLower();
        const QStringList values = declaration.mid(colon + 1).simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (values.isEmpty())
        {
            continue;
        }

        if (name == QLatin1String("border-radius"))
        {
            const qreal radius = parseLength(values.first());
            if (radius >= 0)
            {
                frame.radius = qRound(radius);
            }
        }
        else if (name == QLatin1String("border") || name == QLatin1String("border-width") ||
                 name == QLatin1String("border-color") || borderStyle.match(name).hasMatch())
        {
            // The folder is masked with one uniform border, so side-specific rules only
            // contribute the width and color they name.
            for (const QString& value : values)
            {
                if (isBorderStyleKeyword(value))
                {
                    if (value == QLatin1String("none") || value == QLatin1String("hidden"))
                    {
                        frame.borderWidth = 0;
                    }
                    continue;
                }

                const qreal length = parseLength(value);
                if (length >= 0)
                {
                    if (name != QLatin1String("border-color"))
                    {
                        frame.borderWidth = length;
                    }
                    continue;
                }

                const QColor color { value };
                if (color.isValid() && name != QLatin1String("border-width"))
                {
                    frame.borderColor = color;
                }
            }
        }
    }

    return frame;
}

struct RenderTheme
{
    static constexpr const char* themeFile = "QcwStyledTabFolderRenderer";

    // Defaults from the Classic theme; reset() and the static member definitions share them.
    static constexpr int DefaultTabBarSpacing { 6 };
    static inline const QMargins DefaultTabBarPadding { 6, 6, 6, 6 };
    // Page inset for a tab bar at the top: the edge next to the bar stays clear, while the
    // sides and the opposite edge keep an opaque page widget off the 10px rounded corners.
    static inline const QMargins DefaultPageMargins { 6, 0, 6, 6 };
    // Matches the radius of the built-in folder stylesheet; the value is then read back from
    // folderCss. surroundColor has no default, because only the theme knows the window color
    // behind the folder.
    static constexpr int DefaultFolderRadius { 10 };
    static inline const QcwBorderRadii DefaultTabBorderRadius { 4, 4, 4, 4 };
    static inline const QColor DefaultTabBorderColor { 0xdd, 0xdd, 0xdd };
    static inline const QColor DefaultDirtyTextColor { 0x00, 0xc8, 0x53 };
    static inline const QColor DefaultHoveredTabBackground { 0xf1, 0xf1, 0xf1 };
    static inline const QColor DefaultSelectedTabBackground { 0xe3, 0xe3, 0xe3 };
    static inline const QColor DefaultActiveTabBackground { 0xb2, 0xeb, 0xf2 };

public:
    /**
     * Installs the built-in defaults. QcwWidgetTheme calls doLoad() with an empty object when
     * the current theme has no renderer file, so loading has to start from these values
     * instead of leaving whatever the previous theme installed.
     */
    static void reset()
    {
        tabBarSpacing = DefaultTabBarSpacing;
        tabBarPadding = DefaultTabBarPadding;
        pageMargins = DefaultPageMargins;
        folderRadius = DefaultFolderRadius;
        surroundColor = QColor();
        folderBorderWidth = 0;
        folderBorderColor = QColor();
        tabBorderRadius = DefaultTabBorderRadius;
        tabBorderColor = DefaultTabBorderColor;
        dirtyTextColor = DefaultDirtyTextColor;
        hoveredTabBackground = DefaultHoveredTabBackground;
        selectedTabBackground = DefaultSelectedTabBackground;
        activeTabBackground = DefaultActiveTabBackground;
        folderCss = defaultFolderCss();
        applyFolderCss();
    }

    static void doLoad(const QJsonObject& jobj)
    {
        reset();

        if (jobj.contains("tabBarSpacing"))
        {
            tabBarSpacing = jobj.value("tabBarSpacing").toInt(tabBarSpacing);
        }

        if (jobj.contains("tabBarPadding"))
        {
            tabBarPadding = QcwThemeUtils::parseMargins(jobj.value("tabBarPadding").toString(), tabBarPadding);
        }

        if (jobj.contains("pageMargins"))
        {
            pageMargins = QcwThemeUtils::parseMargins(jobj.value("pageMargins").toString(), pageMargins);
        }

        if (jobj.contains("tabBorderRadius"))
        {
            tabBorderRadius = QcwThemeUtils::parseRadii(jobj.value("tabBorderRadius").toString(), tabBorderRadius);
        }

        const auto loadColor = [&jobj](const QString& key, QColor& target)
        {
            const QColor color = QcwThemeUtils::parseColor(jobj.value(key).toString());
            if (color.isValid())
            {
                target = color;
            }
        };

        loadColor(QStringLiteral("tabBorderColor"), tabBorderColor);
        loadColor(QStringLiteral("dirtyTextColor"), dirtyTextColor);
        loadColor(QStringLiteral("hoveredTabBackground"), hoveredTabBackground);
        loadColor(QStringLiteral("selectedTabBackground"), selectedTabBackground);
        loadColor(QStringLiteral("activeTabBackground"), activeTabBackground);
        loadColor(QStringLiteral("surroundColor"), surroundColor);

        if (jobj.contains("folderCss"))
        {
            folderCss = jobj.value("folderCss").toString(folderCss);
        }

        applyFolderCss();
    }

    /**
     * @brief Reads the corner radius and border back from the folder stylesheet.
     *
     * @details The folder paints along the stylesheet's border and radius, so the overlays have
     *          to use the same values; separate theme keys would let the two drift apart.
     */
    static void applyFolderCss()
    {
        const FolderFrame frame = parseFolderFrame(folderCss);
        folderRadius = frame.radius;
        folderBorderWidth = qRound(frame.borderWidth);
        folderBorderColor = frame.borderColor;
    }

public:
    static int tabBarSpacing;
    static QMargins tabBarPadding;
    static QMargins pageMargins;
    static int folderRadius;
    static QColor surroundColor;
    static int folderBorderWidth;
    static QColor folderBorderColor;
    static QcwBorderRadii tabBorderRadius;
    static QColor tabBorderColor;
    static QColor dirtyTextColor;
    static QColor hoveredTabBackground;
    static QColor selectedTabBackground;
    static QColor activeTabBackground;
    static QString folderCss;
};

int RenderTheme::tabBarSpacing { RenderTheme::DefaultTabBarSpacing };
QMargins RenderTheme::tabBarPadding { RenderTheme::DefaultTabBarPadding };
QMargins RenderTheme::pageMargins { RenderTheme::DefaultPageMargins };
int RenderTheme::folderRadius { RenderTheme::DefaultFolderRadius };
QColor RenderTheme::surroundColor;
int RenderTheme::folderBorderWidth { 0 };
QColor RenderTheme::folderBorderColor;
QcwBorderRadii RenderTheme::tabBorderRadius { RenderTheme::DefaultTabBorderRadius };
QColor RenderTheme::tabBorderColor { RenderTheme::DefaultTabBorderColor };
QColor RenderTheme::dirtyTextColor { RenderTheme::DefaultDirtyTextColor };
QColor RenderTheme::hoveredTabBackground { RenderTheme::DefaultHoveredTabBackground };
QColor RenderTheme::selectedTabBackground { RenderTheme::DefaultSelectedTabBackground };
QColor RenderTheme::activeTabBackground { RenderTheme::DefaultActiveTabBackground };
QString RenderTheme::folderCss { defaultFolderCss() };

void changeTheme(QcwTabFolder* folder)
{
    folder->setStyleSheet(RenderTheme::folderCss);
    folder->update();
}

} // namespace

QcwStyledTabFolderRenderer::QcwStyledTabFolderRenderer(QcwTabFolder* folder) :
    QcwTabFolderRenderer { folder }
{
    static QcwWidgetTheme<RenderTheme> theme;

    auto conn = QObject::connect(themeManager, &QcwThemeManager::themeChanged, [this]()
    {
        changeTheme(mFolder);
    });
    mConnections.append(conn);

    changeTheme(mFolder);
}

QcwStyledTabFolderRenderer::~QcwStyledTabFolderRenderer()
{
    for (auto& conn : mConnections)
    {
        QObject::disconnect(conn);
    }
    mConnections.clear();
}

QcwTabFolderRenderer* QcwStyledTabFolderRenderer::clone(QcwTabFolder* folder) const
{
    return new QcwStyledTabFolderRenderer(folder);
}

int QcwStyledTabFolderRenderer::tabBarSpacing() const
{
    return RenderTheme::tabBarSpacing;
}

QMargins QcwStyledTabFolderRenderer::tabBarPadding() const
{
    return RenderTheme::tabBarPadding;
}

QMargins QcwStyledTabFolderRenderer::pageMargins() const
{
    // The theme describes the inset for a tab bar at the top. With the bar at the bottom the
    // page has to keep clear of the top corners instead, so the two edges swap.
    const QMargins margins = RenderTheme::pageMargins;
    if (mFolder->tabPosition() == qcw::TabPosition::Bottom)
    {
        return QMargins { margins.left(), margins.bottom(), margins.right(), margins.top() };
    }

    return margins;
}

QcwTabFolderRenderer::FrameMask QcwStyledTabFolderRenderer::frameMask() const
{
    // The folder decides which corners and edges need the overlays; only the shape and colors
    // come from the theme. Unset colors leave the matching overlay disabled.
    return { RenderTheme::folderRadius, RenderTheme::surroundColor, RenderTheme::folderBorderWidth,
             RenderTheme::folderBorderColor };
}

void QcwStyledTabFolderRenderer::renderTabBarRegion(QPainter& painter,
                                                    const QRect& rect,
                                                    const QRect& selectedTabRect) const
{
    painter.setRenderHints(QPainter::Antialiasing, false);

    QPen pen(RenderTheme::tabBorderColor, 1.0);
    painter.setPen(pen);

    int y = mFolder->tabPosition() == qcw::TabPosition::Top ? rect.bottom() : rect.top();
    painter.drawLine(rect.left(), y, rect.right(), y);
}

void QcwStyledTabFolderRenderer::renderTabRegion(QPainter& painter, TabState state, const QRect& rect) const
{
    int st = state;
    if (st == Normal)
    {
        return;
    }

    painter.save();
    painter.setRenderHints(QPainter::Antialiasing, true);

    QColor bgColor;
    if ((st & Pressed) != 0)
    {
        if (mFolder->isActive())
        {
            bgColor = RenderTheme::activeTabBackground;
        }
        else
        {
            bgColor = RenderTheme::selectedTabBackground;
        }
    }
    else
    {
        bgColor = RenderTheme::hoveredTabBackground;
    }

    painter.setBrush(bgColor);
    painter.setPen(QPen(RenderTheme::tabBorderColor, 1.0));

    // Do not assume that the radii of the four corners of the tab are equal.
    const auto& radii = RenderTheme::tabBorderRadius;
    QPainterPath path;

    path.moveTo(rect.left() + radii.topLeft, rect.top());
    path.lineTo(rect.right() - radii.topRight, rect.top());
    path.arcTo(rect.right() - 2 * radii.topRight, rect.top(), 2 * radii.topRight, 2 * radii.topRight, 90, -90);
    path.lineTo(rect.right(), rect.bottom() - radii.bottomRight);
    path.arcTo(rect.right() - 2 * radii.bottomRight, rect.bottom() - 2 * radii.bottomRight, 2 * radii.bottomRight,
               2 * radii.bottomRight, 0, -90);
    path.lineTo(rect.left() + radii.bottomLeft, rect.bottom());
    path.arcTo(rect.left(), rect.bottom() - 2 * radii.bottomLeft, 2 * radii.bottomLeft, 2 * radii.bottomLeft, -90, -90);
    path.lineTo(rect.left(), rect.top() + radii.topLeft);
    path.arcTo(rect.left(), rect.top(), 2 * radii.topLeft, 2 * radii.topLeft, 180, -90);
    path.closeSubpath();

    painter.drawPath(path);

    painter.restore();
}

QColor QcwStyledTabFolderRenderer::textColor(TabState tabState, bool isDirty) const
{
    Q_UNUSED(tabState);

    return isDirty ? RenderTheme::dirtyTextColor : QApplication::palette().color(QPalette::WindowText);
}

QT_END_NAMESPACE
