// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "StyledTabFolderRenderer.h"
#include "QcwTabFolder.h"
#include <QPainter>
#include <QApplication>
#include <QJsonObject>

StyledTabFolderRenderer::StyledTabFolderRenderer(QcwTabFolder* folder) :
    QcwTabFolderRenderer { folder }
{
}

int StyledTabFolderRenderer::tabBarSpacing() const
{
    return mTabBarSpacing;
}

QMargins StyledTabFolderRenderer::tabBarPadding() const
{
    return mTabBarPadding;
}

QcwTabFolderRenderer* StyledTabFolderRenderer::clone(QcwTabFolder* folder) const
{
    auto r = new StyledTabFolderRenderer(folder);

    // Copy every value setThemeData() installs, or the cloned folder falls back
    // to default-constructed (invalid) colors and loses the tab-bar metrics.
    r->mTabBarSpacing = mTabBarSpacing;
    r->mTabBarPadding = mTabBarPadding;
    r->mTabBorderColor = mTabBorderColor;
    r->mHoveredTabBackground = mHoveredTabBackground;
    r->mSelectedTabBackground = mSelectedTabBackground;
    r->mActiveTabBackground = mActiveTabBackground;
    r->mTabBorderRadius = mTabBorderRadius;
    r->mFolderCss = mFolderCss;

    // setThemeData() applies the stylesheet through mFolder, so a clone into a
    // fresh folder has to repeat that here.
    r->mFolder->setStyleSheet(mFolderCss);

    return r;
}

void StyledTabFolderRenderer::setThemeData(const QJsonObject& jobj)
{
    const QString style = isLightTheme() ? QStringLiteral("Light") : QStringLiteral("Dark");
    const auto themeObj = jobj[style];

    mTabBarSpacing = themeObj["tabBarSpacing"].toInt();
    const int padding = themeObj["tabBarPadding"].toInt();
    mTabBarPadding = QMargins(padding, padding, padding, padding);
    mTabBorderColor = QColor::fromString(themeObj["tabBorderColor"].toString());
    mHoveredTabBackground = QColor::fromString(themeObj["hoveredTabBackground"].toString());
    mSelectedTabBackground = QColor::fromString(themeObj["selectedTabBackground"].toString());

    mActiveTabBackground = QColor::fromString(themeObj["activeTabBackground"].toString());
    mTabBorderRadius = themeObj["tabBorderRadius"].toInt();
    mFolderCss = themeObj["folderCss"].toString();

    mFolder->setStyleSheet(mFolderCss);
}

void StyledTabFolderRenderer::renderTabBarRegion(QPainter& painter,
                                                 const QRect& rect,
                                                 const QRect& selectedTabRect) const
{
    painter.setRenderHints(QPainter::Antialiasing, false);

    QPen pen(mTabBorderColor, 1.0);
    painter.setPen(pen);

    int y = mFolder->tabPosition() == qcw::TabPosition::Top ? rect.bottom() : rect.top();
    painter.drawLine(rect.left(), y, rect.right(), y);
}

void StyledTabFolderRenderer::renderTabRegion(QPainter& painter, TabState state, const QRect& rect) const
{
    const int st = state;
    if (st == Normal)
    {
        return;
    }

    painter.save();
    painter.setRenderHints(QPainter::Antialiasing, true);

    painter.setPen(QPen(Qt::NoPen));
    QColor bgColor;
    // Pressed is the selected tab (see QcwTabFolderRenderer::TabState); the active-folder
    // branch paints the bright accent, an inactive folder settles for the plain selection color.
    if ((st & Pressed) != 0)
    {
        if (mFolder->isActive())
        {
            bgColor = mActiveTabBackground;
        }
        else
        {
            bgColor = mSelectedTabBackground;
        }
    }
    else
    {
        bgColor = mHoveredTabBackground;
    }

    painter.setBrush(bgColor);
    painter.drawRoundedRect(rect, mTabBorderRadius, mTabBorderRadius);

    painter.restore();
}

QColor StyledTabFolderRenderer::textColor(TabState tabState, bool isDirty) const
{
    Q_UNUSED(tabState);
    Q_UNUSED(isDirty);

    return QApplication::palette().color(QPalette::WindowText);
}
