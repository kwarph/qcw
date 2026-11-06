// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwDockingTypes.h"
#include "QcwGlobal.h"
#include <QColor>
#include <QIcon>
#include <QMargins>
#include <QPixmap>
#include <QString>

QT_BEGIN_NAMESPACE

class QRect;
class QPainter;
class QcwTabFolder;

/**
 * @class QcwTabFolderRenderer
 * @brief Abstract base class for customizing the visual appearance of QcwTabFolder.
 *
 * @details QcwTabFolderRenderer allows developers to override the default painting
 * logic for tabs, tab bars, and associated buttons (close, maximize, etc.).
 * By subclassing this class and implementing its pure virtual methods, you can
 * create completely custom themes or drawing styles.
 *
 * A renderer does not own its folder and does not take ownership of anything it
 * is handed to paint; it only draws. Instances are created with
 * createDefaultRenderer() or by cloning an existing renderer for another folder,
 * and the owning QcwTabFolder is responsible for deleting them.
 *
 * @see QcwTabFolder::setRenderer()
 */
class QCW_EXPORT QcwTabFolderRenderer
{
public:
    /**
     * @enum TabState
     * @brief Interaction state of a tab or button.
     *
     * @details The values combine as bit flags: a tab is reported with Pressed
     * when it is the selected tab and with Hovered when the pointer is over it,
     * so a selected tab under the cursor carries both bits (Hovered | Pressed).
     * Implementations should test with bitwise AND rather than equality, except
     * for Normal, which is the empty (0) state.
     */
    enum TabState
    {
        Normal,  ///< Default state: neither selected nor hovered.
        Hovered, ///< Pointer is over the element (also set together with Pressed).
        Pressed  ///< The element is the selected tab.
    };

    /**
     * @brief Constants for default dimensions and flags.
     */
    enum
    {
        ButtonSize = 22,    ///< Default size for square buttons (e.g., close button).
        ButtonIconSize = 8, ///< Default icon size inside buttons.
        CloseRectSize = 16, ///< Default hit area size for the close button.
        TabSpacing = 4,     ///< Default spacing between tabs.
        TabTextFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic ///< Default text alignment flags.
    };

    /**
     * @struct FrameMask
     * @brief Rounded corners and border the folder keeps visible above the page widget.
     *
     * @details A page widget fills the whole area below the tab bar and paints opaquely, so it
     *          covers the folder's rounded background and its border there. The folder covers
     *          the corners opposite its tab bar with small overlay widgets painted in
     *          @c surroundColor and clipped by a quarter disc of @c cornerRadius, then restores
     *          the border over the page along the covered edges and around the corner arcs.
     *          A zero radius or an invalid surround color disables the corner masks; a zero
     *          border width or an invalid border color disables the border overlay.
     */
    struct FrameMask
    {
        int cornerRadius { 0 }; ///< Corner radius to restore, in pixels.
        QColor surroundColor;   ///< Color outside the folder, usually the window background.
        int borderWidth { 0 };  ///< Folder border width to restore, in pixels.
        QColor borderColor;     ///< Folder border color.
    };

public:
    /**
     * @brief Constructs a renderer associated with a specific tab folder.
     * @param folder The parent QcwTabFolder instance.
     */
    explicit QcwTabFolderRenderer(QcwTabFolder* folder);

    virtual ~QcwTabFolderRenderer();

    /**
     * @brief Factory method to create the default renderer.
     * @param folder The folder the renderer paints. Must outlive the renderer.
     * @return A new renderer instance; ownership is transferred to the caller.
     */
    static QcwTabFolderRenderer* createDefaultRenderer(QcwTabFolder* folder);

    /**
     * @brief Pixel spacing between adjacent tabs, used when laying out and painting.
     * @return The spacing in pixels; zero selects the compact layout.
     * @details The base returns 0, the former member default. This accessor replaces the
     *          protected @c mTabBarSpacing member, so a subclass that needs another value
     *          stores it itself and overrides this method.
     */
    virtual int tabBarSpacing() const;

    /**
     * @brief Inner padding of the tab bar container.
     * @return The padding in top, right, bottom, left order; the base returns zero.
     * @details This accessor replaces the protected @c mTabBarPadding member.
     */
    virtual QMargins tabBarPadding() const;

    /**
     * @brief Margins kept clear between the folder's edge and the page area.
     * @return The margins in left, top, right, bottom order; the base returns zero.
     * @details QcwTabFolder lays the current page inside the area below (or above) the tab
     *          bar after subtracting these margins. A renderer that fills the folder with a
     *          rounded background returns an inset large enough that an opaque page widget
     *          cannot paint over the corners; the folder queries it for the current tab
     *          position, so the edge next to the tab bar can stay at zero.
     */
    virtual QMargins pageMargins() const;

    /**
     * @brief Describes the rounded corners and border the folder has to protect from the page.
     * @return The mask; the base returns one that disables the overlays.
     * @details QcwTabFolder shows the corner overlays only opposite the tab bar, because the
     *          bar itself keeps its own edge free of an opaque page; the border is restored
     *          along the page-covered edges. The corner radius and border have to match the
     *          folder background painted by the renderer's stylesheet.
     */
    virtual FrameMask frameMask() const;

    /**
     * @brief Creates a clone of this renderer for another tab folder.
     * @param folder The folder the clone will paint. Must outlive the clone.
     * @return A new renderer carrying this one's per-folder tweaks; ownership
     *         is transferred to the caller.
     */
    virtual QcwTabFolderRenderer* clone(QcwTabFolder* folder) const = 0;

    /**
     * @brief Renders the maximize/restore icon based on the folder state.
     * @param state The folder state; anything other than Normal selects the
     *              restore glyph.
     * @return The rendered icon.
     */
    virtual QIcon renderMaximizeIcon(qcw::FolderState state) const;

    /**
     * @brief Renders the minimize icon.
     * @return The rendered icon.
     */
    virtual QIcon renderMinimizeIcon() const;

    /**
     * @brief Renders the view/options icon.
     * @return The rendered icon.
     */
    virtual QIcon renderViewIcon() const;

    /**
     * @brief Renders a single tab item: background, icon, text and close button.
     *
     * The tab background is delegated to renderTabRegion(); this function draws
     * the content on top of it.
     *
     * @param painter The painter to draw with.
     * @param state The interaction state of the tab (see TabState, a bit set).
     * @param tabRect The bounding rectangle of the tab.
     * @param closeBtnState The interaction state of the tab's close button.
     * @param closeBtnRect The bounding rectangle of the close button.
     * @param icon The tab icon (ignored when @p showIcon is false or it is null).
     * @param text The tab label.
     * @param textRectWidth The width available for the text.
     * @param showIcon Whether to draw the icon.
     * @param showWholeText When false the label is elided to fit textRectWidth.
     * @param showClose Whether to draw the close button.
     * @param isDirty Whether the tab content has unsaved changes (prefixes "*").
     */
    void renderTab(QPainter& painter,
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
                   bool isDirty) const;

    /**
     * @brief Renders the tab bar background and the drag-and-drop indicator.
     * @param painter The painter to draw with.
     * @param tabBarRect The bounding rectangle of the tab bar.
     * @param selectedTabRect The rectangle of the selected tab, for visual
     *                        continuity with the bar background.
     * @param xPosOfDropIndicator X-coordinate of the drop indicator line, or -1
     *                            to draw none.
     */
    void renderTabBar(QPainter& painter,
                      const QRect& tabBarRect,
                      const QRect& selectedTabRect,
                      int xPosOfDropIndicator = -1) const;

protected:
    /**
     * @brief Renders the background region of the tab bar.
     * @param painter The painter to draw with.
     * @param tabBarRect The area to fill.
     * @param selectedTabRect The rect of the selected tab, used for continuity.
     */
    virtual void renderTabBarRegion(QPainter& painter, const QRect& tabBarRect, const QRect& selectedTabRect) const = 0;

    /**
     * @brief Renders the background shape of a single tab.
     * @param painter The painter to draw with.
     * @param state The interaction state of the tab.
     * @param tabRect The bounding rectangle of the tab.
     */
    virtual void renderTabRegion(QPainter& painter, TabState state, const QRect& tabRect) const = 0;

    /**
     * @brief Renders a tab's close button.
     * @param painter The painter to draw with.
     * @param closeButtonState The interaction state of the button.
     * @param closeButtonRect The bounding rectangle of the button; expected to be square.
     *
     * @details The default draws a 1.1px X centered in @p closeButtonRect, using the palette's
     *          WindowText and switching to red while the pointer is over the button. Subclasses
     *          may override it to draw a different glyph.
     */
    virtual void renderCloseButton(QPainter& painter, TabState closeButtonState, const QRect& closeButtonRect) const;

    /**
     * @brief Returns the color to draw a tab's label in.
     * @param tabState The interaction state of the tab.
     * @param isDirty Whether the tab has unsaved changes.
     */
    virtual QColor textColor(TabState tabState, bool isDirty) const = 0;

    /**
     * @brief Determines whether the current system theme is light.
     * @return True for a light theme, false for a dark one.
     */
    static bool isLightTheme();

    /**
     * @brief Returns a color for line-art glyphs (icons, drop indicators).
     *
     * Derived from the palette's WindowText so it reads on both light and dark
     * themes, with channels clamped to the valid range.
     *
     * @param lightTheme Whether to build the color for a light theme.
     */
    static QColor iconColor(bool lightTheme);

protected:
    QcwTabFolder* mFolder { nullptr }; ///< Pointer to the associated tab folder.
};

QT_END_NAMESPACE
