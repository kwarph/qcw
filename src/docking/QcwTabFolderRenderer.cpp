// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTabFolderRenderer.h"
#include "QcwDefaultTabFolderRenderer.h"
#include "QcwTabFolder.h"
#include "QcwThemeIcons.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QApplication>
#include <QStyleHints>
#include <QStyle>

QT_BEGIN_NAMESPACE

QcwTabFolderRenderer::QcwTabFolderRenderer(QcwTabFolder* folder) :
    mFolder { folder }
{
}

QcwTabFolderRenderer::~QcwTabFolderRenderer()
{
}

QcwTabFolderRenderer* QcwTabFolderRenderer::createDefaultRenderer(QcwTabFolder* folder)
{
    return new QcwDefaultTabFolderRenderer(folder);
}

int QcwTabFolderRenderer::tabBarSpacing() const
{
    return 0;
}

QMargins QcwTabFolderRenderer::tabBarPadding() const
{
    return {};
}

QMargins QcwTabFolderRenderer::pageMargins() const
{
    return {};
}

QcwTabFolderRenderer::FrameMask QcwTabFolderRenderer::frameMask() const
{
    return {};
}

QColor QcwTabFolderRenderer::iconColor(bool lightTheme)
{
    QColor baseColor = QApplication::palette().color(QPalette::WindowText);
    if (!lightTheme)
    {
        return baseColor.darker(128);
    }

    // qBound keeps a bright base channel from overflowing past 255.
    return QColor(qBound(0, baseColor.red() + 72, 255), qBound(0, baseColor.green() + 72, 255),
                  qBound(0, baseColor.blue() + 72, 255));
}

QIcon QcwTabFolderRenderer::renderMaximizeIcon(qcw::FolderState state) const
{
    if (state != qcw::FolderState::Normal)
    {
        return qcw::themeIcon(qcw::Icon::FolderRestore);
    }

    return qcw::themeIcon(qcw::Icon::FolderMaximize);
}

QIcon QcwTabFolderRenderer::renderMinimizeIcon() const
{
    return qcw::themeIcon(qcw::Icon::FolderMinimize);
}

QIcon QcwTabFolderRenderer::renderViewIcon() const
{
    return qcw::themeIcon(qcw::Icon::View);
}

void QcwTabFolderRenderer::renderTab(QPainter& painter,
                                     TabState state,
                                     const QRect& tabRect,
                                     TabState closeBtnState,
                                     const QRect& closeBtnRect,
                                     const QIcon& icon,
                                     const QString& text,
                                     int textRectWidth,
                                     bool showIcon,
                                     bool showWholeText,
                                     bool showClose,
                                     bool isDirty) const
{
    // paint tab background, borders, etc.
    renderTabRegion(painter, state, tabRect);

    painter.save();

    // paint icon
    int x = tabRect.x() + TabSpacing;
    if (showIcon && !icon.isNull())
    {
        const auto iconSize = mFolder->iconSize();
        QPixmap pixmap = icon.pixmap(iconSize);

        int y = tabRect.y() + (tabRect.height() - iconSize) / 2;
        painter.drawPixmap(QRect(x, y, iconSize, iconSize), pixmap);

        x += iconSize;
        x += TabSpacing;
    }
    else
    {
        x += tabBarSpacing() > 0 ? TabSpacing : TabSpacing * 2;
    }

    // paint text
    if (!text.isEmpty())
    {
        auto f = mFolder->font();
        QFontMetrics fm(f);
        int y = tabRect.y() + (tabRect.height() - fm.height()) / 2;
        QRect r(x, y, textRectWidth, fm.height());

        painter.setPen(textColor(state, isDirty));
        painter.setFont(f);

        auto textToPaint = isDirty ? QString("*%1").arg(text) : text;
        if (showWholeText)
        {
            painter.drawText(r, TabTextFlags, textToPaint);
        }
        else
        {
            painter.drawText(r, TabTextFlags,
                             fm.elidedText(textToPaint, Qt::ElideRight, textRectWidth, Qt::TextShowMnemonic));
        }
    }

    // paint close button
    if (showClose && closeBtnRect.isValid())
    {
        renderCloseButton(painter, closeBtnState, closeBtnRect);
    }

    painter.restore();
}

void QcwTabFolderRenderer::renderTabBar(QPainter& painter,
                                        const QRect& tabBarRect,
                                        const QRect& selectedTabRect,
                                        int xPosOfDropIndicator) const
{
    renderTabBarRegion(painter, tabBarRect, selectedTabRect);

    if (xPosOfDropIndicator >= 0)
    {
        painter.save();

        painter.setRenderHints(QPainter::Antialiasing, true);

        if (xPosOfDropIndicator < 2)
        {
            ++xPosOfDropIndicator;
        }

        QPen pen(iconColor(isLightTheme()), 2.0);
        painter.setPen(pen);

        painter.drawLine(xPosOfDropIndicator, tabBarRect.top() + tabBarSpacing() + 1, xPosOfDropIndicator,
                         tabBarRect.bottom() - tabBarSpacing() - 1);

        painter.restore();
    }
}

void QcwTabFolderRenderer::renderCloseButton(QPainter& painter,
                                             TabState closeButtonState,
                                             const QRect& closeButtonRect) const
{
    painter.setRenderHints(QPainter::Antialiasing, true);

    bool hovered = closeButtonState != Normal;
    auto palette = QApplication::palette();
    QPen pen = hovered ? QPen(Qt::red, 1.1) : QPen(palette.color(QPalette::WindowText), 1.1);
    painter.setPen(pen);

    int offset = (closeButtonRect.width() - ButtonIconSize) / 2;
    auto x = closeButtonRect.left() + offset;
    int top = closeButtonRect.top() + offset;
    int bottom = top + ButtonIconSize;

    QPoint topLeft(x, top);
    QPoint bottomRight(x + ButtonIconSize, bottom);
    QPoint topRight(x + ButtonIconSize, top);
    QPoint bottomLeft(x, bottom);

    painter.drawLine(topLeft, bottomRight);
    painter.drawLine(topRight, bottomLeft);
}

bool QcwTabFolderRenderer::isLightTheme()
{
    QColor baseColor = QApplication::palette().color(QPalette::Base);
    return baseColor.lightness() > 128;
}

QT_END_NAMESPACE
