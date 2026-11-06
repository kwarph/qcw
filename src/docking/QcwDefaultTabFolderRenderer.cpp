// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwDefaultTabFolderRenderer.h"
#include "QcwTabFolder.h"
#include <QPainter>
#include <QApplication>
#include <QStyle>

QT_BEGIN_NAMESPACE

QcwDefaultTabFolderRenderer::QcwDefaultTabFolderRenderer(QcwTabFolder* folder) :
    QcwTabFolderRenderer { folder }
{
}

QcwTabFolderRenderer* QcwDefaultTabFolderRenderer::clone(QcwTabFolder* folder) const
{
    return new QcwDefaultTabFolderRenderer(folder);
}

int QcwDefaultTabFolderRenderer::tabBarSpacing() const
{
    return 0;
}

QMargins QcwDefaultTabFolderRenderer::tabBarPadding() const
{
    return QMargins { 0, 0, 0, 0 };
}

void QcwDefaultTabFolderRenderer::renderTabBarRegion(QPainter& painter,
                                                     const QRect& tabBarRect,
                                                     const QRect& selectedTabRect) const
{
    painter.save();

    painter.setRenderHints(QPainter::Antialiasing, false);

    QPen pen(tabBorderColor(), 1.0);
    painter.setPen(pen);

    auto margins = tabBarPadding();
    auto tabAtTop = mFolder->tabPosition() == qcw::TabPosition::Top;
    int padding = tabAtTop ? margins.bottom() : margins.top();
    if (padding < 0)
    {
        padding = 0;
    }

    int y = mFolder->tabPosition() == qcw::TabPosition::Top ? tabBarRect.bottom() : tabBarRect.top();
    if (padding == 0)
    {
        bool isFirst = (selectedTabRect.left() - tabBarRect.left()) <= margins.left() + TabSpacing;
        if (!isFirst)
        {
            painter.drawLine(tabBarRect.left(), y, selectedTabRect.left() - 1, y);
        }
        painter.drawLine(selectedTabRect.right() + 1, y, tabBarRect.right(), y);
    }
    else
    {
        painter.drawLine(tabBarRect.left(), y, tabBarRect.right(), y);
    }

    painter.restore();
}

void QcwDefaultTabFolderRenderer::renderTabRegion(QPainter& painter, TabState state, const QRect& tabRect) const
{
    int st = state;
    if (st == Normal)
    {
        return;
    }

    painter.save();

    // the state can be (Hovered | Pressed)
    bool selected = (st & Pressed) != 0;
    if (selected)
    {
        // paint the tab's background
        painter.fillRect(tabRect, selectedTabBackground());

        auto margins = tabBarPadding();
        bool barAtTop = mFolder->tabPosition() == qcw::TabPosition::Top;
        int padding = barAtTop ? margins.bottom() : margins.top();
        if (padding < 0)
        {
            padding = 0;
        }

        // paint the selected tab's borders
        painter.setRenderHints(QPainter::Antialiasing, false);

        int x = tabRect.left();
        int y = tabRect.top();
        bool isFirst = tabRect.left() <= margins.left() + TabSpacing;
        if (padding == 0)
        {
            // paint the left border
            painter.setPen(QPen(tabBorderColor(), 1.0));

            if (!isFirst) // skip the first visible tab
            {
                painter.drawLine(x, y, x, tabRect.bottom());
            }

            // paint the right border
            x = tabRect.right();
            painter.drawLine(x, y, x, tabRect.bottom());
        }

        // An inactive folder loses the highlight color, so it falls back to the
        // same muted glyph color the icons use.
        QColor highlightColor =
            mFolder->isActive() ? QApplication::palette().color(QPalette::Highlight) : iconColor(isLightTheme());
        painter.setPen(QPen(highlightColor, 2.0));

        // If tabs at top, move the line 1px downward
        y = barAtTop ? tabRect.bottom() : tabRect.top() + 1;
        if (padding == 0)
        {
            painter.drawLine(tabRect.left() + 1 + !isFirst, y, tabRect.right() - 1, y);
        }
        else
        {
            painter.drawLine(tabRect.left() + 1, y, tabRect.right(), y);
        }
    }
    else
    {
        // paint the tab's background
        painter.fillRect(tabRect, hoveredTabBackground());
    }

    painter.restore();
}

QColor QcwDefaultTabFolderRenderer::textColor(TabState tabState, bool isDirty) const
{
    Q_UNUSED(tabState);
    Q_UNUSED(isDirty);

    return QApplication::palette().color(QPalette::WindowText);
}

QColor QcwDefaultTabFolderRenderer::tabBorderColor() const
{
    // Derived from the live palette on every call, so a theme switch is picked
    // up on the next repaint instead of being frozen at first construction.
    auto winColor = QApplication::palette().color(QPalette::Window);
    return isLightTheme() ? winColor.darker(115) : winColor.lighter(220);
}

QColor QcwDefaultTabFolderRenderer::hoveredTabBackground() const
{
    auto winColor = QApplication::palette().color(QPalette::Window);
    return isLightTheme() ? winColor.darker(106) : winColor.lighter(160);
}

QColor QcwDefaultTabFolderRenderer::selectedTabBackground() const
{
    return QApplication::palette().color(QPalette::Base);
}

QT_END_NAMESPACE
