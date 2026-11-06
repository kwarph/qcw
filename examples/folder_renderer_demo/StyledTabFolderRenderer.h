// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwTabFolderRenderer.h"

class QJsonObject;

/// Rounded card-style tab renderer driven by a JSON theme, the demo's Styled window style.
class StyledTabFolderRenderer : public QcwTabFolderRenderer
{
public:
    explicit StyledTabFolderRenderer(QcwTabFolder* folder);

    QcwTabFolderRenderer* clone(QcwTabFolder* folder) const override;

    int tabBarSpacing() const override;
    QMargins tabBarPadding() const override;

    /// Reads the Light or Dark entry of @p jobj, matching the application palette.
    void setThemeData(const QJsonObject& jobj);

protected:
    void renderTabBarRegion(QPainter& painter, const QRect& rect, const QRect& selectedTabRect) const override;
    void renderTabRegion(QPainter& painter, TabState state, const QRect& rect) const override;
    QColor textColor(TabState tabState, bool isDirty) const override;

    int mTabBarSpacing { 0 };
    QMargins mTabBarPadding;
    QColor mTabBorderColor;
    QColor mHoveredTabBackground;
    QColor mSelectedTabBackground;
    QColor mActiveTabBackground;
    int mTabBorderRadius { 4 };
    QString mFolderCss;
};
