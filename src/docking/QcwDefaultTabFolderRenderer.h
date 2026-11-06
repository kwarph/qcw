// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwTabFolderRenderer.h"

QT_BEGIN_NAMESPACE

/**
 * @class QcwDefaultTabFolderRenderer
 * @brief The built-in flat renderer used when no custom renderer is installed.
 *
 * @details Draws tabs with palette-derived colors and a highlight rule on the
 * selected tab. The colors are recomputed from the current system palette on
 * every query, so switching between light and dark themes takes effect on the
 * next repaint without re-creating the renderer.
 *
 * The three color accessors are virtual so a subclass can keep the layout of
 * the default renderer while changing only its palette.
 */
class QCW_EXPORT QcwDefaultTabFolderRenderer : public QcwTabFolderRenderer
{
public:
    /**
     * @brief Constructs a default renderer for @p folder.
     * @param folder The folder the renderer paints. Must outlive the renderer.
     */
    explicit QcwDefaultTabFolderRenderer(QcwTabFolder* folder);

    /**
     * @brief Clones this renderer for another folder, preserving its tweaks.
     * @param folder The folder the clone will paint.
     * @return A new renderer instance; ownership is transferred to the caller.
     */
    QcwTabFolderRenderer* clone(QcwTabFolder* folder) const override;

    int tabBarSpacing() const override;

    QMargins tabBarPadding() const override;

protected:
    void renderTabBarRegion(QPainter& painter, const QRect& tabBarRect, const QRect& selectedTabRect) const override;
    void renderTabRegion(QPainter& painter, TabState state, const QRect& tabRect) const override;
    QColor textColor(TabState tabState, bool isDirty) const override;

    /// Color of the borders around and beneath the tabs.
    virtual QColor tabBorderColor() const;
    /// Background fill of a tab the pointer is over.
    virtual QColor hoveredTabBackground() const;
    /// Background fill of the selected tab.
    virtual QColor selectedTabBackground() const;
};

QT_END_NAMESPACE
