// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwTabFolderRenderer.h"
#include <QList>

QT_BEGIN_NAMESPACE

/**
 * @class QcwStyledTabFolderRenderer
 * @brief Renderer driven by the current theme's QcwStyledTabFolderRenderer.theme data.
 *
 * @details Reads the tab spacing, tab-bar padding, page margins, tab colors and folder
 * stylesheet from the theme and paints rounded, filled tab backgrounds. The folder's corner
 * radius and border are read back from the folder stylesheet's @c QcwTabFolder rule, so
 * @c folderCss alone decides the card's shape; the data is shared by all instances and reloaded
 * whenever the theme changes, and each instance then re-applies the folder stylesheet. When the
 * theme names the window color around the folder, the renderer also asks the folder to mask the
 * rounded corners opposite the tab bar, so an opaque page widget cannot paint over the card; the
 * border read from @c folderCss is restored over the page as well. Themes without that color
 * fall back to page margins large enough to keep such widgets off the corners; a bar at the
 * bottom swaps the two edges.
 * Missing values fall back to the renderer defaults, so a theme without the file still
 * produces a usable folder.
 *
 * The tab label color and the dirty marker come from the application palette and the theme,
 * rather than from QStyle::standardPalette(), so they follow theme changes.
 *
 * @see QcwWidgetTheme, QcwThemeUtils
 */
class QCW_EXPORT QcwStyledTabFolderRenderer : public QcwTabFolderRenderer
{
public:
    /**
     * @brief Constructs a styled renderer for @p folder.
     * @param folder The folder the renderer paints. Must outlive the renderer.
     */
    explicit QcwStyledTabFolderRenderer(QcwTabFolder* folder);

    /// Disconnects the renderer from the process-wide theme-change notification.
    ~QcwStyledTabFolderRenderer() override;

    QcwTabFolderRenderer* clone(QcwTabFolder* folder) const override;

    int tabBarSpacing() const override;

    QMargins tabBarPadding() const override;

    QMargins pageMargins() const override;

    FrameMask frameMask() const override;

protected:
    void renderTabBarRegion(QPainter& painter, const QRect& rect, const QRect& selectedTabRect) const override;
    void renderTabRegion(QPainter& painter, TabState state, const QRect& rect) const override;
    QColor textColor(TabState tabState, bool isDirty) const override;

protected:
    QList<QMetaObject::Connection> mConnections;
};

QT_END_NAMESPACE
