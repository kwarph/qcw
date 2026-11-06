// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTabFolder.h"
#include "QcwTabPaneManager.h"
#include "QcwFilterableListWidget.h"
#include "QcwSerializeUtils.h"
#include "QcwTabFolderRenderer.h"
#include "QcwFloatingFolderMonitor.h"

#include <QApplication>
#include <QMouseEvent>
#include <QStyleHints>
#include <QStyleOption>
#include <QStylePainter>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QToolTip>
#include <QStandardItem>
#include <QMimeData>
#include <QDrag>
#include <QMargins>
#include <QScreen>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QDialog>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <QSpacerItem>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QGroupBox>
#include <QPointer>
#include <QtMath>

QT_BEGIN_NAMESPACE

/**
 * @class QcwTabListButton
 * @brief The button that lists the tabs the tab bar is too narrow to show.
 *
 * @details The button paints itself instead of showing an icon: a double chevron with the
 *          number of hidden tabs under its right end, the way Eclipse's tab overflow button
 *          looks. The count is drawn in the palette's text color, so it follows a theme
 *          change, and the width is measured from the text's advance, not from its ink, so
 *          the hint always covers the number the painter lays out.
 */
class QcwTabListButton : public QToolButton
{
public:
    explicit QcwTabListButton(QWidget* parent = nullptr) :
        QToolButton { parent }
    {
    }

    /// Sets the number of tabs the button stands for and repaints when it changes.
    void setHiddenCount(int count)
    {
        if (count == hiddenCount)
        {
            return;
        }

        hiddenCount = count;
        update();
    }

    /// The width the widest count the button draws, "99+", needs.
    int maxWidth() const
    {
        return widthForText(maxCountText());
    }

    QSize sizeHint() const override
    {
        return QSize(widthForText(countText()), QcwTabFolderRenderer::ButtonSize);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QStylePainter painter(this);
        QStyleOptionToolButton option;
        initStyleOption(&option);
        painter.drawComplexControl(QStyle::CC_ToolButton, option);

        painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);

        const QColor color = palette().color(QPalette::WindowText);
        QPen pen(color, chevronPenWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);

        const qreal midX = chevronRect().center().x();
        const qreal midY = chevronRect().center().y();
        QPainterPath chevron;
        chevron.moveTo(chevronRect().left(), chevronRect().top());
        chevron.lineTo(midX - 0.5, midY);
        chevron.lineTo(chevronRect().left(), chevronRect().bottom());
        chevron.moveTo(midX + 0.5, chevronRect().top());
        chevron.lineTo(chevronRect().right(), midY);
        chevron.lineTo(midX + 0.5, chevronRect().bottom());
        painter.drawPath(chevron);

        // drawText() positions the line by its origin and lays it out by the advance, so both
        // the baseline and the width hint below come from the metrics the painter uses.
        painter.setFont(countFont());
        painter.setPen(color);
        painter.drawText(QPointF(countTextX, height() - textBottomPadding), countText());
    }

private:
    static QString maxCountText()
    {
        return QStringLiteral("99+");
    }

    QString countText() const
    {
        return hiddenCount > 99 ? maxCountText() : QString::number(hiddenCount);
    }

    QFont countFont() const
    {
        QFont f = font();
        f.setPointSize(numberFontSize);
        return f;
    }

    /// The rect the chevron's path is drawn in; its 1.5 px pen reaches half a pixel further.
    static QRectF chevronRect()
    {
        return { chevronLeft, chevronTop, chevronWidth, chevronHeight };
    }

    /// The room @p text needs: its origin, its advance and one right margin. A count too
    /// narrow to fill the button still gets a square one, like the folder's other buttons.
    int widthForText(const QString& text) const
    {
        const QFontMetricsF metrics(countFont());
        const qreal right = countTextX + metrics.horizontalAdvance(text) + textRightPadding;
        return qMax<int>(QcwTabFolderRenderer::ButtonSize, qCeil(right));
    }

    static constexpr qreal chevronLeft { 4.0 };
    static constexpr qreal chevronTop { 4.0 };
    static constexpr qreal chevronWidth { 6.0 };
    static constexpr qreal chevronHeight { 4.0 };
    static constexpr qreal chevronPenWidth { 1.5 };
    /// The count starts under the chevron's right end.
    static constexpr qreal countTextX { chevronLeft + chevronWidth };
    /// Baseline height above the button's bottom, and clear space to the count's right.
    static constexpr int textBottomPadding { 4 };
    static constexpr int textRightPadding { 4 };
    static constexpr int numberFontSize { 8 };
    int hiddenCount { 0 };
};

class QcwTab
{
public:
    QcwTab(QWidget* widget, const QString& text, const QIcon& icon, QcwTabFolderPrivate* parent);
#ifdef QCW_DEBUG
    ~QcwTab()
    {
        qDebug().noquote().nospace() << "QcwTab(" << this << ' ' << text << ") destroyed";
    }
#endif

    QcwTab* clone(QcwTabFolderPrivate* parent) const;

    int thisIndex() const;

    void setText(const QString& s);
    void setTextRectWidth(int w);
    void setShowDirty(bool dirty);
    void updateTextWidth();

    void updateGeometry(int x, int y, bool ignoreFullTitle);

public:
    QcwTabFolderPrivate* owner { nullptr };
    QWidget* widget { nullptr };
    QIcon icon;
    QString text;
    int fullTextWidth { 0 };
    int textWidth { 0 };
    int shortcutId { 0 };
    QString toolTip;
    QMap<QString, QVariant> data;
    QRect rect;
    QRect closeRect;
    bool visible { true };
    bool closable { true };
    bool detachable { true };
    bool dirty { false };
    bool textFullyDisplayed { true };
};

// -----------------------------------------------------------------------------

// Straight border length a corner overlay paints beyond the arc on each side. With it the
// overlay can stroke the arc and both straight borders as one path, so the tangents carry no
// stroke endpoint that could show as a burr.
static constexpr int CornerBorderOverlap { 2 };

/**
 * Paints one rounded folder corner above the page widget.
 *
 * The page fills the whole area below the tab bar and would cover the folder's rounded
 * background there. The overlay covers the radius-sized square at the folder corner with the
 * color outside the folder, except for the quarter disc the page is allowed to fill, and draws
 * the folder border around the corner: a straight stub, the quarter arc and another stub, all
 * as one stroke.
 */
class QcwCornerMaskOverlay : public QWidget
{
public:
    explicit QcwCornerMaskOverlay(QWidget* parent, Qt::Corner corner) :
        QWidget { parent }, mCorner { corner }
    {
        setObjectName(QStringLiteral("qcwCornerMask"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        hide();
    }

    void setCornerShape(int radius, const QColor& surroundColor, int borderWidth, const QColor& borderColor)
    {
        if (mRadius == radius && mSurroundColor == surroundColor && mBorderWidth == borderWidth &&
            mBorderColor == borderColor)
        {
            return;
        }

        mRadius = radius;
        mSurroundColor = surroundColor;
        mBorderWidth = borderWidth;
        mBorderColor = borderColor;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (mRadius <= 0 || !mSurroundColor.isValid() || width() <= 0 || height() <= 0)
        {
            return;
        }

        // The overlay is larger than the radius square by the straight stub on both sides. Only
        // the square at the folder corner is masked; the stub band stays transparent so the
        // folder's own straight border shows through it.
        const qreal r = mRadius;
        const qreal stub = qMax<qreal>(0, width() - r);
        const bool leftEdge = mCorner == Qt::TopLeftCorner || mCorner == Qt::BottomLeftCorner;
        const bool topEdge = mCorner == Qt::TopLeftCorner || mCorner == Qt::TopRightCorner;
        const QRectF cornerSquare { leftEdge ? 0 : stub, topEdge ? 0 : stub, r, r };

        // The arc's centre sits one radius in from the folder corner; its visible quarter
        // matches the folder background's rounded corner.
        const QPointF center { leftEdge ? r : stub, topEdge ? r : stub };

        // Stop the mask at the border's inner edge: the border path below paints the whole band
        // over it, so the mask's antialiased edge cannot wash the border out at the tangents.
        const qreal border = mBorderWidth > 0 && mBorderWidth < mRadius ? mBorderWidth : 0;
        const qreal maskRadius = r - border;
        QPainterPath holePath;
        holePath.addEllipse(
            QRectF { center.x() - maskRadius, center.y() - maskRadius, 2 * maskRadius, 2 * maskRadius });

        QPainterPath mask;
        mask.addRect(cornerSquare);
        mask = mask.subtracted(holePath);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.fillPath(mask, mSurroundColor);

        if (mBorderWidth <= 0 || !mBorderColor.isValid() || mBorderWidth >= 2 * mRadius)
        {
            return;
        }

        // One open path: straight stub, quarter arc, straight stub. Stroking it in one go keeps
        // the tangents inside the path, where no endpoint cap or antialiasing step can show.
        const qreal halfBorder = mBorderWidth / 2.0;
        const qreal strokeRadius = r - halfBorder;
        const QRectF arcRect { center.x() - strokeRadius, center.y() - strokeRadius, 2 * strokeRadius,
                               2 * strokeRadius };
        const qreal w = width();
        const qreal h = height();

        QPainterPath borderPath;
        switch (mCorner)
        {
        case Qt::TopLeftCorner:
            borderPath.moveTo(w, halfBorder);
            borderPath.lineTo(r, halfBorder);
            borderPath.arcTo(arcRect, 90, 90);
            borderPath.lineTo(halfBorder, h);
            break;
        case Qt::TopRightCorner:
            borderPath.moveTo(w - halfBorder, h);
            borderPath.lineTo(w - halfBorder, r);
            borderPath.arcTo(arcRect, 0, 90);
            borderPath.lineTo(0, halfBorder);
            break;
        case Qt::BottomLeftCorner:
            borderPath.moveTo(halfBorder, 0);
            borderPath.lineTo(halfBorder, stub);
            borderPath.arcTo(arcRect, 180, 90);
            borderPath.lineTo(w, h - halfBorder);
            break;
        default:
            borderPath.moveTo(0, h - halfBorder);
            borderPath.lineTo(stub, h - halfBorder);
            borderPath.arcTo(arcRect, 270, 90);
            borderPath.lineTo(w - halfBorder, 0);
            break;
        }

        painter.setPen(QPen { mBorderColor, qreal(mBorderWidth) });
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(borderPath);
    }

private:
    Qt::Corner mCorner;
    int mRadius { 0 };
    QColor mSurroundColor;
    int mBorderWidth { 0 };
    QColor mBorderColor;
};

/**
 * Paints the part of the folder border that an opaque page widget covers.
 *
 * The page reaches the folder's edge, so the folder's own border is under it. The strip lies
 * along that edge, above the page, in the border color.
 */
class QcwBorderStrip : public QWidget
{
public:
    explicit QcwBorderStrip(QWidget* parent) :
        QWidget { parent }
    {
        setObjectName(QStringLiteral("qcwBorderStrip"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        hide();
    }

    void setBorderColor(const QColor& color)
    {
        if (mColor == color)
        {
            return;
        }

        mColor = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!mColor.isValid())
        {
            return;
        }

        QPainter painter(this);
        painter.fillRect(rect(), mColor);
    }

private:
    QColor mColor;
};

// -----------------------------------------------------------------------------

class QcwTabFolderPrivate
{
public:
    explicit QcwTabFolderPrivate(QcwTabFolder* interface);
    ~QcwTabFolderPrivate();

    void createMenu();
    void createTabBarButtons();
    void retranslateUi();
    void createOverlays();
    void updateOverlays();
    void raiseOverlays();

    int addTab(QWidget* widget, const QString& text, const QIcon& icon);
    int addTab(QcwTab* tab);
    int insertTab(int index, QWidget* widget, const QString& text, const QIcon& icon);
    int insertTab(int index, QcwTab* tab);
    bool removeTab(int index);
    void moveTab(int indexFrom, int indexTo);
    bool hasPane(QWidget* pane) const;
    void releasePane(QWidget* pane);
    void detachTab(int index);
    void adoptTab(QcwTabFolder* srcFolder, int srcIndex, int myTargetIndex);

    void checkAfterTabRemoved();

    void paintTabs(QPainter& painter) const;
    QPixmap renderDragImage() const;

    QcwTab* tabAt(const QPoint& pos) const;
    int indexAt(const QPoint& pos) const;

    bool isMouseOn(QWidget* childWidget, const QPoint& pos) const;
    bool isMouseOnAnyWidget(const QPoint& pos) const;
    void clearHoverState();

    void setCurrentIndex(int index);

    void setRightWidget(QWidget* widget, QWidget* widgetOwner);
    void releaseRightWidget();
    void layoutRightWidget();
    QRect pageRect() const;
    int placeRightWidgetInPage(const QRect& page);

    inline bool isValidIndex(int index) const
    {
        return index >= 0 && index < tabs.size();
    }
    int nextVisibleIndex(int index) const;
    int prevVisibleIndex(int index) const;

    int firstVisibleIndex() const;
    int lastVisibleIndex() const;

    void updateRecentIndexes(int indexToRemove);
    int computeMinTabBarWidth();
    void computeTabBarRect();
    int computeTabsGeometry();
    void computeWidgetGeometry();
    void determineVisibleTabs();
    void showWidget(int index);

    int visibleCount() const;

public:
    QcwTabFolder* owner;
    QList<QcwTab*> tabs;
    QWidget* placeholder;
    QWidget* current { nullptr };
    QcwTabFolderRenderer* renderer;
    QList<int> recentIndexes;

    QPointer<QWidget> rightWidget;
    QPointer<QWidget> rightWidgetOwner; // Parent rightWidget had when it was installed.
    QMetaObject::Connection rightWidgetOwnerConnection;
    bool rightWidgetInPage { false };
    int tabListSettling { 0 }; // determineVisibleTabs() is measuring the tabs; see there.
    QToolButton* minimizeButton { nullptr };
    QToolButton* maximizeButton { nullptr };
    QcwTabListButton* tabListButton { nullptr };

    enum BorderStrip
    {
        LeftStrip,
        TopStrip,
        RightStrip,
        BottomStrip,
        BorderStripCount
    };

    // Corner overlays indexed with Qt::Corner.
    QcwCornerMaskOverlay* cornerMasks[4] { nullptr, nullptr, nullptr, nullptr };
    QcwBorderStrip* borderStrips[BorderStripCount] { nullptr, nullptr, nullptr, nullptr };

    qcw::TabPosition tabPosition { qcw::TabPosition::Top };
    qcw::FolderFeatures features { qcw::DefaultTabFeatures };

    QRect tabBarRect;
    QRect tabBarContentsRect;
    int iconSize { 16 };
    int minTextRectWidth { 120 };
    int freeSpace { 0 };

    int currentIndex { -1 };
    int pressedIndex { -1 };
    int hoveredIndex { -1 };

    // first: dragMoveOnIndex, second: true - insert before this tab, false - insert after this tab
    QPair<int, bool> dragMoveInfo { -1, false };
    QPoint dragStartPosition;
    QPoint rightClickPosition;

    qcw::FolderState state { qcw::FolderState::Normal };
    bool floating { false };
    bool active { false };
    bool closeButtonHovered { false };
    bool dragInProcess { false };

    QMenu* menu { nullptr };
    QAction* closeAction { nullptr };
    QAction* closeOthersAction { nullptr };
    QAction* closeLeftTabsAction { nullptr };
    QAction* closeRightTabsAction { nullptr };
    QAction* closeAllTabsAction { nullptr };
    QAction* detachAction { nullptr };
    QAction* settingsAction { nullptr };
};

// -----------------------------------------------------------------------------

class QcwTabFolderSettingsDialog : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(QcwTabFolderSettingsDialog)

public:
    explicit QcwTabFolderSettingsDialog(QcwTabFolder* folder, QWidget* parent = nullptr);

    void addCheckBox(QCheckBox* checkBox, qcw::FolderFeature feature);
    void setupCheckBoxes(qcw::FolderFeatures features);

protected:
    /// Re-sets every label this dialog builds, the feature labels coming from featureLabel().
    void changeEvent(QEvent* event) override;

private:
    /// @brief The checkbox label of @p feature, in the language currently installed.
    static QString featureLabel(qcw::FolderFeature feature);
    /// The checkbox tooltip of @p feature, in the language currently installed.
    static QString featureDescription(qcw::FolderFeature feature);
    void retranslateUi();

private:
    QcwTabFolder* folder;
    qcw::FolderFeatures features;
    int iconSize;
    int minTextWidth;

    QMap<QCheckBox*, qcw::FolderFeature> checkBoxMap;
    QCheckBox* checkBoxMaximizable;
    QCheckBox* checkBoxMinimizable;

    QPushButton* applyButton;
    QPushButton* defaultButton;

    QGroupBox* groupBoxFeatures { nullptr };
    QGroupBox* groupBoxSizes { nullptr };
    QLabel* iconSizeLabel { nullptr };
    QLabel* textWidthLabel { nullptr };
    QSpinBox* iconSizeSpinBox { nullptr };
    QSpinBox* textWidthSpinBox { nullptr };
};

// Implementations of QcwTabFolder
// -----------------------------------------------------------------------------

QcwTabFolder::QcwTabFolder(QWidget* parent) :
    QFrame { parent }, impl { new QcwTabFolderPrivate { this } }
{
    setFrameShape(QFrame::NoFrame);

    setMouseTracking(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAcceptDrops(true);

    impl->createMenu();
    impl->createTabBarButtons();
    impl->retranslateUi();

    impl->placeholder = new QWidget(this);
    impl->placeholder->setObjectName("placeholder");
    impl->current = impl->placeholder;

    impl->createOverlays();

    impl->computeMinTabBarWidth();
    impl->computeTabBarRect();
    impl->computeWidgetGeometry();
    impl->updateOverlays();

    setMinimumSize(impl->computeMinTabBarWidth(), impl->tabBarRect.height() + impl->placeholder->minimumHeight());

    // Promote this folder to the active one when focus moves into any of its
    // tab contents, so a view does not have to reach up and fake a tab-bar
    // click itself. A hidden tab page cannot hold focus, so focus landing
    // inside the folder always belongs to the current tab: activate the folder
    // at its current index instead of calling setCurrentIndex, which would
    // bounce focus back to the page via showWidget(). The isActive() guard
    // keeps an already-active folder from re-firing on every internal focus move.
    QObject::connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now)
    {
        if (!isActive() && now && impl->currentIndex >= 0 && isAncestorOf(now))
        {
            emit tabBarClicked(impl->currentIndex);
        }
    });

    // Also watch mouse presses application-wide: a click deep inside a view is
    // consumed by that child widget, so the page-level filter below never sees
    // it, and re-clicking an already-focused view produces no focusChanged.
    // Removed again in the destructor since qApp outlives the folder.
    qApp->installEventFilter(this);

#ifdef QCW_DEBUG
    qDebug() << this << "created";
#endif
}

QcwTabFolder::~QcwTabFolder()
{
    qApp->removeEventFilter(this);

    // QWidget's own teardown can still report a focus change after this class has deleted its
    // private state, and the focusChanged hook below reads it. Stop listening to the
    // application before that.
    disconnect(qApp, nullptr, this, nullptr);

    // The right widget belongs to its caller, not to this folder, so it is handed back
    // before the folder's children are destroyed with it.
    impl->releaseRightWidget();

    if (!impl->tabs.empty())
    {
        for (int i = impl->tabs.size() - 1; i >= 0; --i)
        {
            auto tab = impl->tabs[i];
            if (tab->widget)
            {
                impl->releasePane(tab->widget);
            }
            delete tab;
        }

        impl->tabs.clear();
    }

    delete impl;

#ifdef QCW_DEBUG
    qDebug() << this << "destroyed";
#endif
}

void QcwTabFolder::saveState(QXmlStreamWriter& writer) const
{
    writer.writeStartElement("folder"); // --> folder begin
    writer.writeAttribute("id", QString::number(property("FolderId").toInt()));
    writer.writeAttribute("currentIndex", QString::number(impl->currentIndex));
    writer.writeAttribute("features", QString::number(impl->features));
    writer.writeAttribute("tabPosition", QString::number(std::to_underlying(impl->tabPosition)));
    writer.writeAttribute("floating", QString("%1").arg(impl->floating));
    writer.writeAttribute("iconSize", QString::number(impl->iconSize));
    writer.writeAttribute("minTextRectWidth", QString::number(impl->minTextRectWidth));
    writer.writeAttribute("state", QString::number(std::to_underlying(impl->state)));

    auto ba = QcwSerializeUtils::serialize(impl->recentIndexes);
    writer.writeAttribute("recentIndexes", ba.toHex(' '));

    writer.writeStartElement("tabs"); // --> tabs begin
    writer.writeAttribute("count", QString::number(impl->tabs.size()));

    for (auto tab : std::as_const(impl->tabs))
    {
        writer.writeStartElement("tab"); // --> tab begin
        writer.writeAttribute("text", tab->text);
        // The title the pane's creator handed out, which is what lets a restore tell an untouched
        // tab from a renamed one once the two languages disagree. Written even when there is
        // nothing to record: an empty value means the tab was named by whoever made it, while no
        // value at all means the document predates the attribute and cannot say.
        writer.writeAttribute("defaultText",
                              tab->widget ? tab->widget->property(qcw::ViewDefaultTitlePropertyName).toString()
                                          : QString {});
        writer.writeAttribute("toolTip", tab->toolTip);

        ba = QcwSerializeUtils::serialize(tab->data);
        writer.writeAttribute("data", ba.toHex(' '));
        writer.writeAttribute("closable", QString("%1").arg(tab->closable));
        writer.writeAttribute("detachable", QString("%1").arg(tab->detachable));

        writer.writeEndElement(); // <-- tab end
    }

    writer.writeEndElement(); // <-- tabs end

    writer.writeEndElement(); // <-- folder end
}

bool QcwTabFolder::restoreState(QXmlStreamReader& reader, const QMap<int, QPair<QWidget*, int>>& tabPanes)
{
    if (reader.name() != "folder" || reader.hasError())
    {
        return false;
    }

    const QXmlStreamAttributes attrs = reader.attributes();
    setProperty("FolderId", attrs.value("id").toInt());

    const int savedIndex = attrs.value("currentIndex").toInt();
    const auto savedState = qcw::FolderState(attrs.value("state").toInt());
    const auto savedFeatures = qcw::FolderFeatures::fromInt(attrs.value("features").toInt());

    auto ba = QByteArray::fromHex(attrs.value("recentIndexes").toLocal8Bit());
    auto savedRecentIndexes = QcwSerializeUtils::deserialize<QList<int>>(ba);

    // Must precede the first addTab(): setCurrentIndex() uses `floating` to
    // decide whether the tab text becomes the window title.
    setFloating(attrs.value("floating").toInt() != 0);

    while (reader.readNextStartElement())
    {
        if (reader.name() == "tabs")
        {
            int tabCount = reader.attributes().value("count").toInt();
            if (tabCount == 0)
            {
                reader.skipCurrentElement();
            }
            else
            {
                while (reader.readNextStartElement())
                {
                    if (reader.name() == "tab")
                    {
                        ba = QByteArray::fromHex(reader.attributes().value("data").toLocal8Bit());
                        auto data = QcwSerializeUtils::deserialize<QMap<QString, QVariant>>(ba);

                        int paneId = data.value(qcw::TabPaneIdPropertyName, -1).toInt();

                        auto widgetPair = tabPanes.value(paneId);
                        if (!widgetPair.first)
                        {
                            // The view was dropped before the folders were read (a saved
                            // duplicate of a per-window singleton), so its tab goes too.
                            reader.skipCurrentElement();
                            continue;
                        }

                        widgetPair.first->setProperty(qcw::TabPaneIdPropertyName, widgetPair.second);
                        QIcon icon = widgetPair.first->windowIcon();

                        // The stored literal is still what the tab shows; the baseline only says
                        // which title it was derived from, so a language change can tell an
                        // untouched tab from a renamed one. The attribute's presence carries the
                        // third case: saveState() always writes it, so a document that has it with
                        // an empty value was written knowing, and its pane has no baseline because
                        // the caller named the view itself - that name is not the creator's to
                        // re-resolve. Only a document that predates the attribute is silent about
                        // its own language, and then the text is the sole evidence of what the
                        // baseline was: pairing it with anything else — above all with the title
                        // the creator hands out in the language installed right now — would leave
                        // the two in different languages, which reads as a rename and freezes the
                        // old text in place for good. Recorded only once the tab is known to be
                        // this pane's own, because a saved duplicate is dropped below and the pane
                        // it repeats still has the baseline of the tab that survived.
                        const auto tabAttrs = reader.attributes();
                        const auto savedText = tabAttrs.value("text").toString();

                        int index = addTab(widgetPair.first, savedText, icon);
                        if (index >= 0) // a saved duplicate is dropped with its tab element
                        {
                            widgetPair.first->setProperty(qcw::ViewDefaultTitlePropertyName,
                                                          tabAttrs.hasAttribute("defaultText")
                                                              ? tabAttrs.value("defaultText").toString()
                                                              : savedText);

                            auto tab = impl->tabs[index];
                            tab->toolTip = tabAttrs.value("toolTip").toString();
                            tab->closable = tabAttrs.value("closable").toInt();
                            tab->detachable = tabAttrs.value("detachable").toInt();

                            if (paneId != widgetPair.second)
                            {
                                data[qcw::TabPaneIdPropertyName] = widgetPair.second;
                            }
                            tab->data = data;
                        }
                    }
                    reader.skipCurrentElement();
                }
            }
        }
        else
        {
            reader.skipCurrentElement();
        }
    }

    // Apply through the setters: they rebuild the tab bar rect, the tab text
    // widths and the maximize button, which raw field writes left stale.
    setTabPosition(qcw::TabPosition(attrs.value("tabPosition").toInt()));
    setIconSize(attrs.value("iconSize").toInt());
    setMinimumTextRectWidth(attrs.value("minTextRectWidth").toInt());
    setFeatures(savedFeatures);

    // addTab() maintains the MRU list itself, so the order saved before the
    // tabs were re-added has to be applied afterwards. Keep the "one entry per
    // tab index" invariant: drop duplicates and indexes of tabs that are gone,
    // append the indexes that the saved list does not cover.
    impl->recentIndexes.clear();
    for (auto idx : std::as_const(savedRecentIndexes))
    {
        if (impl->isValidIndex(idx) && !impl->recentIndexes.contains(idx))
        {
            impl->recentIndexes.append(idx);
        }
    }
    for (int i = 0; i < impl->tabs.size(); ++i)
    {
        if (!impl->recentIndexes.contains(i))
        {
            impl->recentIndexes.append(i);
        }
    }

    impl->computeTabBarRect();
    impl->computeMinTabBarWidth();
    impl->computeTabsGeometry();
    impl->computeWidgetGeometry();

    setState(savedState);

    const int indexToShow =
        impl->isValidIndex(savedIndex) ? savedIndex : (impl->tabs.isEmpty() ? -1 : impl->recentIndexes.back());
    setCurrentIndex(indexToShow);

    update();

    return !reader.hasError();
}

QSize QcwTabFolder::sizeHint() const
{
    int normalHeight = impl->tabBarRect.height() + 100;
    int normalWidth = 200;
    return QSize(normalWidth, normalHeight);
}

QSize QcwTabFolder::minimumSizeHint() const
{
    return QSize(impl->computeMinTabBarWidth(), impl->tabBarRect.height() + impl->placeholder->minimumHeight());
}

bool QcwTabFolder::isFloating() const
{
    return impl->floating;
}

void QcwTabFolder::setFloating(bool floating)
{
    impl->floating = floating;
}

bool QcwTabFolder::isActive() const
{
    return impl->active;
}

void QcwTabFolder::setActive(bool active)
{
    if (active != impl->active)
    {
        impl->active = active;
        update(impl->tabBarRect);
    }
}

int QcwTabFolder::addTab(QWidget* widget, const QString& text)
{
    return addTab(widget, text, QIcon());
}

static void postAddTab(QcwTabFolder* folder, QWidget* widget, const QString& text, int index)
{
    if (widget)
    {
        widget->setWindowTitle(text);
        widget->setMouseTracking(true);
        widget->setAcceptDrops(true);
        tabPaneManager->addReference(widget, folder);
    }

    emit folder->tabAdded(index);
    emit folder->tabsChanged();
}

int QcwTabFolder::addTab(QWidget* widget, const QString& text, const QIcon& icon)
{
    auto index = impl->addTab(widget, text, icon);
    if (index >= 0)
    {
        postAddTab(this, widget, text, index);
    }

    return index;
}

int QcwTabFolder::insertTab(int index, QWidget* widget, const QString& text)
{
    return insertTab(index, widget, text, QIcon());
}

int QcwTabFolder::insertTab(int index, QWidget* widget, const QString& text, const QIcon& icon)
{
    auto newIndex = impl->insertTab(index, widget, text, icon);
    if (newIndex >= 0)
    {
        postAddTab(this, widget, text, newIndex);
    }

    return newIndex;
}

void QcwTabFolder::removeTab(int index)
{
    auto w = tabPane(index);
    if (impl->removeTab(index))
    {
        if (w)
        {
            w->hide();
        }
        emit tabRemoved(index, w);
        emit tabsChanged();

        impl->checkAfterTabRemoved();
    }
}

void QcwTabFolder::takeTab(QcwTabFolder* adoptor, int index, int& indexInAdopter)
{
    if (!isValidIndex(index))
    {
        indexInAdopter = -1;
        return;
    }

    if (adoptor == this)
    {
        if (!isValidIndex(indexInAdopter))
        {
            indexInAdopter = impl->tabs.size() - 1;
            moveTab(index, indexInAdopter);
            // A user-initiated move selects the tab it dropped, unlike a
            // programmatic moveTab().
            setCurrentIndex(indexInAdopter);
        }
        return;
    }

    auto srcTab = impl->tabs[index];
    auto movedWidget = srcTab->widget;

    // The adoptor shows a pane only once. Check before either folder is touched:
    // a rejected transfer must leave both tab lists and the reference counts as
    // they were.
    if (movedWidget && adoptor->impl->hasPane(movedWidget))
    {
        indexInAdopter = -1;
        return;
    }

    auto tab = srcTab->clone(adoptor->impl);

    if (movedWidget)
    {
        tabPaneManager->addReference(movedWidget, adoptor);
    }

    impl->removeTab(index);
    emit tabRemoved(index, movedWidget);
    emit tabsChanged();

    if (adoptor->impl->isValidIndex(indexInAdopter))
    {
        indexInAdopter = adoptor->impl->insertTab(indexInAdopter, tab);
    }
    else
    {
        indexInAdopter = adoptor->impl->addTab(tab);
    }

    emit adoptor->tabAdded(indexInAdopter);
    emit adoptor->tabsChanged();

    impl->pressedIndex = -1;
    impl->dragInProcess = false;

    emit tabTransferred(this, adoptor, tab->widget);
    impl->checkAfterTabRemoved();
}

void QcwTabFolder::moveTab(int indexFrom, int indexTo)
{
    if (impl->isValidIndex(indexFrom) && impl->isValidIndex(indexTo))
    {
        impl->moveTab(indexFrom, indexTo);
        impl->determineVisibleTabs();

        emit tabMoved(indexFrom, indexTo);
        emit tabsChanged();
    }
}

QWidget* QcwTabFolder::tabPane(int index) const
{
    return impl->isValidIndex(index) ? impl->tabs[index]->widget : nullptr;
}

void QcwTabFolder::setTabPane(int index, QWidget* widget)
{
    if (impl->isValidIndex(index))
    {
        auto tab = impl->tabs[index];
        auto oldPane = tab->widget;

        // A folder shows a pane at most once, so this also covers re-setting the
        // pane the tab already has: neither case may touch the references.
        if (impl->hasPane(widget))
        {
            return;
        }

        if (widget)
        {
            widget->setParent(this);

            int paneId = -1;
            if (widget->dynamicPropertyNames().contains(qcw::TabPaneIdPropertyName))
            {
                bool ok;
                int id = widget->property(qcw::TabPaneIdPropertyName).toInt(&ok);
                if (ok)
                {
                    paneId = id;
                }
            }
            tab->data[qcw::TabPaneIdPropertyName] = paneId;

            tabPaneManager->addReference(widget, this);
        }

        tab->widget = widget;
        if (impl->current == oldPane)
        {
            impl->showWidget(index);
        }

        if (oldPane)
        {
            oldPane->hide();
            impl->releasePane(oldPane);
        }
    }
}

int QcwTabFolder::tabPaneId(int index) const
{
    int paneId = -1;
    auto widget = tabPane(index);
    if (widget && widget->dynamicPropertyNames().contains(qcw::TabPaneIdPropertyName))
    {
        bool ok;
        auto id = widget->property(qcw::TabPaneIdPropertyName).toInt(&ok);
        if (ok)
        {
            paneId = id;
        }
    }
    return paneId;
}

QIcon QcwTabFolder::tabIcon(int index) const
{
    return impl->isValidIndex(index) ? impl->tabs[index]->icon : QIcon();
}

void QcwTabFolder::setTabIcon(int index, const QIcon& icon)
{
    if (impl->isValidIndex(index))
    {
        auto tab = impl->tabs[index];
        tab->icon = icon;
        if (tab->visible)
        {
            update(tab->rect);
        }
    }
}

QString QcwTabFolder::tabToolTip(int index) const
{
    return impl->isValidIndex(index) ? impl->tabs[index]->toolTip : QString();
}

void QcwTabFolder::setTabToolTip(int index, const QString& text)
{
    if (impl->isValidIndex(index))
    {
        impl->tabs[index]->toolTip = text;
    }
}

QString QcwTabFolder::tabText(int index) const
{
    return impl->isValidIndex(index) ? impl->tabs[index]->text : QString();
}

void QcwTabFolder::setTabText(int index, const QString& text)
{
    if (impl->isValidIndex(index))
    {
        auto tab = impl->tabs[index];
        if (text != tab->text)
        {
            auto oldText = tab->text;
            tab->setText(text);

            releaseShortcut(tab->shortcutId);
            auto seq = QKeySequence::mnemonic(text);
            if (!seq.isEmpty())
            {
                tab->shortcutId = grabShortcut(seq);
                setShortcutEnabled(tab->shortcutId, tab->visible);
            }

            if (tab->widget)
            {
                tab->widget->setWindowTitle(text);
            }

            if (tab->visible)
            {
                impl->determineVisibleTabs();
            }

            emit tabTextChanged(index, text, oldText);
        }
    }
}

const QMap<QString, QVariant>& QcwTabFolder::tabData(int index) const
{
    static const QMap<QString, QVariant> EMPTY;
    return impl->isValidIndex(index) ? impl->tabs[index]->data : EMPTY;
}

QVariant QcwTabFolder::tabData(int index, const QString& key) const
{
    if (impl->isValidIndex(index))
    {
        const auto& data = impl->tabs[index]->data;
        auto it = data.find(key);
        if (it != data.end())
        {
            return it.value();
        }
    }
    return QVariant {};
}

void QcwTabFolder::setTabData(int index, const QMap<QString, QVariant>& data)
{
    if (impl->isValidIndex(index))
    {
        impl->tabs[index]->data = data;
    }
}

void QcwTabFolder::setTabData(int index, const QString& key, const QVariant& value)
{
    if (impl->isValidIndex(index))
    {
        impl->tabs[index]->data[key] = value;
    }
}

bool QcwTabFolder::isTabVisible(int index) const
{
    return impl->isValidIndex(index) ? impl->tabs[index]->visible : false;
}

bool QcwTabFolder::isTabClosable(int index) const
{
    return impl->isValidIndex(index) ? impl->tabs[index]->closable : false;
}

void QcwTabFolder::setTabClosable(int index, bool closable)
{
    if (impl->isValidIndex(index))
    {
        auto tab = impl->tabs[index];
        if (tab->closable != closable)
        {
            tab->closable = closable;
            if (isVisible())
            {
                impl->determineVisibleTabs();
            }
        }
    }
}

bool QcwTabFolder::isTabDetachable(int index) const
{
    return impl->isValidIndex(index) ? impl->tabs[index]->detachable : false;
}

void QcwTabFolder::setTabDetachable(int index, bool detachable)
{
    if (impl->isValidIndex(index))
    {
        impl->tabs[index]->detachable = detachable;
    }
}

bool QcwTabFolder::isTabDirty(int index) const
{
    return impl->isValidIndex(index) ? impl->tabs[index]->dirty : false;
}

void QcwTabFolder::setTabDirty(int index, bool dirty)
{
    if (impl->isValidIndex(index))
    {
        auto tab = impl->tabs[index];
        if (tab->dirty != dirty)
        {
            tab->setShowDirty(dirty);

            if (isVisible() && tab->visible)
            {
                impl->determineVisibleTabs();
            }
        }
    }
}

QWidget* QcwTabFolder::rightWidget() const
{
    return impl->rightWidget.data();
}

void QcwTabFolder::setRightWidget(QWidget* widget)
{
    impl->setRightWidget(widget, widget ? widget->parentWidget() : nullptr);
}

void QcwTabFolder::setRightWidget(QWidget* widget, QWidget* owner)
{
    impl->setRightWidget(widget, owner);
}

void QcwTabFolder::minimize()
{
    if (impl->features.testFlag(qcw::Minimizable))
    {
        impl->minimizeButton->click();
    }
}

void QcwTabFolder::maximize()
{
    if (impl->features.testFlag(qcw::Maximizable))
    {
        impl->maximizeButton->click();
    }
}

qcw::FolderState QcwTabFolder::state() const
{
    return impl->state;
}

void QcwTabFolder::setState(qcw::FolderState state)
{
    if (state != impl->state)
    {
        impl->state = state;
        impl->maximizeButton->setIcon(impl->renderer->renderMaximizeIcon(impl->state));
        impl->maximizeButton->setToolTip(impl->state == qcw::FolderState::Normal ? tr("Maximize") : tr("Restore"));
    }
}

QMenu* QcwTabFolder::tabMenu() const
{
    return impl->menu;
}

int QcwTabFolder::indexOf(QWidget* widget) const
{
    for (int i = 0; i < impl->tabs.size(); ++i)
    {
        if (impl->tabs[i]->widget == widget)
        {
            return i;
        }
    }
    return -1;
}

int QcwTabFolder::indexAt(const QPoint& pos) const
{
    return impl->indexAt(pos);
}

int QcwTabFolder::tabBarFreeSpace() const
{
    return impl->freeSpace;
}

int QcwTabFolder::currentIndex() const
{
    return impl->currentIndex;
}

void QcwTabFolder::setCurrentIndex(int index)
{
    impl->setCurrentIndex(index);
}

QWidget* QcwTabFolder::currentWidget() const
{
    return impl->current != impl->placeholder ? impl->current : nullptr;
}

bool QcwTabFolder::isValidIndex(int index) const
{
    return impl->isValidIndex(index);
}

QWidget* QcwTabFolder::placeholder() const
{
    return impl->placeholder;
}

void QcwTabFolder::setPlaceholder(QWidget* widget)
{
    if (widget && widget != impl->placeholder)
    {
        auto old = impl->placeholder;

        widget->setParent(this);
        impl->placeholder = widget;
        if (old == impl->current)
        {
            impl->current = widget;
            impl->computeWidgetGeometry();
            widget->show();
        }
        else
        {
            // New placeholder is not current, hide it to prevent overlap
            widget->hide();
        }

        old->hide();
        old->deleteLater();
    }
}

int QcwTabFolder::count() const
{
    return impl->tabs.count();
}

bool QcwTabFolder::event(QEvent* event)
{
    switch (event->type())
    {
    case QEvent::ToolTip:
    {
        auto evt = static_cast<QHelpEvent*>(event);
        if (const auto tab = impl->tabAt(evt->pos()))
        {
            QString s;
            if (tab->closeRect.contains(evt->pos()))
            {
                s = tr("Close");
            }
            else
            {
                if (tab->toolTip.isEmpty())
                {
                    s = tab->textFullyDisplayed ? QString() : tab->text;
                }
                else
                {
                    s = tab->toolTip;
                }
            }

            // It is also necessary to display an empty tooltip
            QToolTip::showText(evt->globalPos(), s, this);
            return true;
        }
        break;
    }
    case QEvent::Shortcut:
    {
        auto se = static_cast<QShortcutEvent*>(event);
        for (int i = 0; i < impl->tabs.size(); ++i)
        {
            const auto* tab = impl->tabs.at(i);
            if (tab->shortcutId == se->shortcutId())
            {
                setCurrentIndex(i);
                return true;
            }
        }
        break;
    }
    case QEvent::ChildAdded:
        // Pages and the right widget are stacked above the corner overlays; put the overlays
        // back on top so they keep covering the folder's rounded corners.
        impl->raiseOverlays();
        break;
    case QEvent::LayoutRequest:
    {
        // A child widget asks the folder to re-layout: the right widget's tool bar
        // does so when its actions change, which moves the width the tab bar has
        // room for. QWidget::event() acts on this request only when a layout is set,
        // and the folder has none, so the re-measure happens here.
        if (impl->rightWidget)
        {
            impl->computeTabsGeometry();
            update(impl->tabBarRect);
        }
        break;
    }
    // case QEvent::ApplicationPaletteChange:
    // {
    //     update();
    //     break;
    // }
    default:
        break;
    }

    return QFrame::event(event);
}

void QcwTabFolder::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QStyleOptionFrame opt;
    opt.initFrom(this);

    QPainter painter(this);

    auto hints = QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform;
    painter.setRenderHints(hints, true);

    // paint tabs
    impl->paintTabs(painter);

    // paint tab bar
    int indicatorX = -1;
    if (impl->isValidIndex(impl->dragMoveInfo.first))
    {
        auto onTab = impl->tabs[impl->dragMoveInfo.first];
        indicatorX = impl->dragMoveInfo.second ? onTab->rect.left() : onTab->rect.right() + 1;
    }

    QRect selectedRect;
    if (impl->isValidIndex(impl->currentIndex))
    {
        selectedRect = impl->tabs[impl->currentIndex]->rect;
    }

    impl->renderer->renderTabBar(painter, impl->tabBarRect, selectedRect, indicatorX);

    // Draw the frame last: the selected tab's fill starts on the widget's outermost
    // row and would otherwise erase the border above it.
    style()->drawPrimitive(QStyle::PE_Frame, &opt, &painter, this);

    // QFrame::paintEvent(event);
}

void QcwTabFolder::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        auto pos = event->position().toPoint();
        auto index = indexAt(pos);
        if (index != -1)
        {
            impl->pressedIndex = index;
            if (impl->features.testFlag(qcw::TabsMovable) && !impl->dragInProcess)
            {
                impl->dragStartPosition = pos;
            }
        }
    }

    QFrame::mousePressEvent(event);
}

void QcwTabFolder::mouseMoveEvent(QMouseEvent* event)
{
    auto pos = event->position().toPoint();
    int index = indexAt(pos);
    impl->hoveredIndex = index;

    if (event->buttons() == Qt::NoButton)
    {
        if (index != -1)
        {
            impl->closeButtonHovered = impl->tabs[index]->closeRect.contains(pos);
        }
    }
    else if (event->buttons() == Qt::LeftButton) // dragging
    {
        if (!impl->dragInProcess && impl->isValidIndex(impl->pressedIndex) &&
            (pos - impl->dragStartPosition).manhattanLength() > QApplication::startDragDistance())
        {
            if (!isTabDetachable(impl->pressedIndex) && !impl->tabBarRect.contains(pos))
            {
                impl->dragInProcess = false;
                impl->dragStartPosition = pos;
            }
            else
            {
                impl->dragInProcess = true;

                QByteArray tabData;
                QDataStream dataStream(&tabData, QIODevice::WriteOnly);
                dataStream << impl->pressedIndex;

                QMimeData* mimeData = new QMimeData;
                mimeData->setData(qcw::TabMimeType, tabData);

                QDrag* drag = new QDrag(this);
                drag->setMimeData(mimeData);
                drag->setPixmap(impl->renderDragImage());

                if (drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction) == Qt::IgnoreAction)
                {
                    if (isTabDetachable(impl->pressedIndex))
                    {
                        impl->detachTab(impl->pressedIndex);
                    }
                }

                drag->deleteLater();
            }
        }
        else
        {
            impl->dragInProcess = false;
        }
    }

    update(impl->tabBarRect);

    QFrame::mouseMoveEvent(event);
}

void QcwTabFolder::mouseReleaseEvent(QMouseEvent* event)
{
    auto pos = event->position().toPoint();
    if (event->button() == Qt::LeftButton)
    {
        if (impl->tabBarRect.contains(pos))
        {
            bool onWidget = impl->isMouseOnAnyWidget(pos);
            auto index = impl->indexAt(pos);
            if (!impl->dragInProcess)
            {
                if (impl->isValidIndex(index))
                {
                    auto currTab = impl->tabs[index];
                    if (currTab->closeRect.contains(pos))
                    {
                        emit tabCloseRequested(index);
                        onWidget = true;
                    }
                    else if (impl->isValidIndex(impl->pressedIndex))
                    {
                        setCurrentIndex(impl->pressedIndex);
                    }
                }
            }

            if (!onWidget)
            {
                emit tabBarClicked(impl->pressedIndex);
            }
        }

        // Release outside the tab bar must clear the press bookkeeping too: a
        // stale pressedIndex makes the next click on empty tab bar space select
        // that old tab, and a stale dragInProcess keeps mousePressEvent() from
        // recording a new dragStartPosition, which then swallows the next drag.
        impl->dragInProcess = false;
        impl->pressedIndex = -1;
        impl->dragMoveInfo.first = -1;

        update(impl->tabBarRect);
    }

    QFrame::mouseReleaseEvent(event);
}

void QcwTabFolder::mouseDoubleClickEvent(QMouseEvent* event)
{
    auto pos = event->position().toPoint();
    if (event->button() == Qt::LeftButton && impl->tabBarRect.contains(pos))
    {
        bool onWidget = impl->isMouseOnAnyWidget(pos);

        auto index = indexAt(pos);
        if (impl->isValidIndex(index))
        {
            auto currTab = impl->tabs[index];
            if (currTab->closeRect.contains(pos))
            {
                onWidget = true;
            }
        }

        if (!onWidget)
        {
            if (impl->features.testFlag(qcw::Maximizable))
            {
                impl->maximizeButton->click();
            }
            emit tabBarDoubleClicked(index);
        }
    }

    QFrame::mouseDoubleClickEvent(event);
    // mousePressEvent(event);
}

void QcwTabFolder::dragEnterEvent(QDragEnterEvent* event)
{
    auto pos = event->position().toPoint();
    if (event->mimeData()->hasFormat(qcw::TabMimeType) && impl->tabBarRect.contains(pos))
    {
        if (event->source() == this)
        {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        }
        else
        {
            if (impl->features.testFlag(qcw::AcceptDroppedTabs))
            {
                event->acceptProposedAction();
            }
            else
            {
                event->ignore();
            }
        }
    }
    else
    {
        event->ignore();
    }
}

void QcwTabFolder::dragMoveEvent(QDragMoveEvent* event)
{
    auto pos = event->position().toPoint();
    if (event->mimeData()->hasFormat(qcw::TabMimeType) && impl->tabBarRect.contains(pos))
    {
        if (!impl->features.testFlag(qcw::AcceptDroppedTabs) && event->source() != this)
        {
            event->ignore();
            return;
        }

        auto index = indexAt(pos);
        impl->hoveredIndex = index;

        if (index == -1)
        {
            auto lastIdx = impl->lastVisibleIndex();
            if (lastIdx != -1)
            {
                auto lastTab = impl->tabs[lastIdx];
                if (pos.x() > lastTab->rect.right())
                {
                    index = lastIdx;
                }
            }
        }

        if (impl->isValidIndex(index))
        {
            auto onTab = impl->tabs[index];
            impl->dragMoveInfo.second = onTab->rect.center().x() > pos.x();
        }
        else // the tabbar is empty
        {
            impl->dragMoveInfo.second = true;
        }

        // Calculate 'dragMoveOnIndex' for drawing the dropping indicator
        if (event->source() == this)
        {
            if (!impl->features.testFlag(qcw::TabsMovable))
            {
                impl->dragMoveInfo.first = -1; // do not draw the dropping indicator
            }
            else
            {
                impl->dragMoveInfo.first = (index != impl->pressedIndex ? index : -1);

                if (impl->isValidIndex(impl->dragMoveInfo.first))
                {
                    auto dragOnIndx = impl->dragMoveInfo.first;
                    if (dragOnIndx == impl->nextVisibleIndex(impl->pressedIndex)) // pressedIndex < dragOnIndx
                    {
                        impl->dragMoveInfo.second = false;
                    }
                    else if (dragOnIndx == impl->prevVisibleIndex(impl->pressedIndex)) // pressedIndex > dragOnIndx
                    {
                        impl->dragMoveInfo.second = true;
                    }
                }
            }
        }
        else // from another QcwTabFolder object
        {
            impl->dragMoveInfo.first = index;
        }

        update(impl->tabBarRect);

        auto srcFolder = qobject_cast<QcwTabFolder*>(event->source());
        if (srcFolder)
        {
            bool isSelf = srcFolder == this;
            bool movable = srcFolder->impl->features.testFlag(qcw::TabsMovable);
            bool detachable = srcFolder->isTabDetachable(srcFolder->impl->pressedIndex);

            if (!movable || (!detachable && (!isSelf || !srcFolder->impl->tabBarRect.contains(pos))))
            {
                impl->dragMoveInfo.first = -1;
                event->ignore();
            }
            else
            {
                if (!isSelf)
                {
                    if (features().testFlag(qcw::AcceptDroppedTabs))
                    {
                        event->setDropAction(Qt::CopyAction);
                        event->accept();
                    }
                    else
                    {
                        impl->dragMoveInfo.first = -1;
                        event->ignore();
                    }
                }
                else
                {
                    event->setDropAction(Qt::MoveAction);
                    event->accept();
                }
            }
        }
        else
        {
            event->acceptProposedAction();
        }
    }
    else
    {
        event->ignore();
    }
}

void QcwTabFolder::dragLeaveEvent(QDragLeaveEvent* event)
{
    impl->clearHoverState();
    event->ignore();
}

void QcwTabFolder::dropEvent(QDropEvent* event)
{
    auto pos = event->position().toPoint();
    if (event->mimeData()->hasFormat(qcw::TabMimeType) && impl->tabBarRect.contains(pos))
    {
        auto dragOnIndx = impl->dragMoveInfo.first;
        auto moveLeft = impl->dragMoveInfo.second;

        if (event->source() == this)
        {
            if (impl->features.testFlag(qcw::TabsMovable))
            {
                if (impl->isValidIndex(dragOnIndx) && dragOnIndx != impl->pressedIndex)
                {
                    if (impl->pressedIndex < dragOnIndx)
                    {
                        if (moveLeft)
                        {
                            dragOnIndx = impl->prevVisibleIndex(dragOnIndx);
                        }
                    }
                    else
                    {
                        if (!moveLeft)
                        {
                            dragOnIndx = impl->nextVisibleIndex(dragOnIndx);
                        }
                    }

                    if (dragOnIndx != impl->pressedIndex)
                    {
                        moveTab(impl->pressedIndex, dragOnIndx);
                        // Reordering by hand also selects the moved tab, unlike
                        // a programmatic moveTab().
                        setCurrentIndex(dragOnIndx);
                    }
                }

                event->setDropAction(Qt::MoveAction);
                event->accept();
            }
            else
            {
                event->ignore();
            }
        }
        else
        {
            auto srcFolder = qobject_cast<QcwTabFolder*>(event->source());
            if (srcFolder)
            {
                if (srcFolder->isTabDetachable(srcFolder->impl->pressedIndex))
                {
                    QByteArray itemData = event->mimeData()->data(qcw::TabMimeType);
                    QDataStream dataStream(&itemData, QIODevice::ReadOnly);
                    int index = -1;
                    dataStream >> index;

                    if (impl->visibleCount() == 0)
                    {
                        dragOnIndx = impl->tabs.size(); // maybe impl->tabs is empty
                    }
                    else if (!impl->isValidIndex(dragOnIndx))
                    {
                        dragOnIndx = moveLeft ? impl->firstVisibleIndex() : impl->lastVisibleIndex();
                    }
                    else
                    {
                        if (!moveLeft)
                        {
                            if (dragOnIndx == impl->lastVisibleIndex())
                            {
                                ++dragOnIndx;
                            }
                            else
                            {
                                dragOnIndx = impl->nextVisibleIndex(dragOnIndx);
                            }
                        }
                    }

                    impl->adoptTab(srcFolder, index, dragOnIndx);

                    event->setDropAction(Qt::CopyAction);
                    event->accept();
                }
                else
                {
                    event->ignore();
                }
            }
            else
            {
                event->acceptProposedAction();
            }
        }
    }
    else
    {
        event->ignore();
    }

    impl->dragInProcess = false;
    impl->pressedIndex = -1;
    impl->dragMoveInfo.first = -1;
}

void QcwTabFolder::showEvent(QShowEvent* event)
{
    impl->computeTabBarRect();

    if (impl->tabs.empty())
    {
        impl->computeTabsGeometry();
    }
    else
    {
        impl->determineVisibleTabs();
    }

    impl->computeWidgetGeometry();
    impl->updateOverlays();

    QFrame::showEvent(event);
}

void QcwTabFolder::closeEvent(QCloseEvent* event)
{
    emit aboutToClose(event);

    if (event->isAccepted())
    {
        QFrame::closeEvent(event);
    }
}

void QcwTabFolder::resizeEvent(QResizeEvent* event)
{
    if (isVisible())
    {
        impl->computeTabBarRect();
        if (impl->tabs.empty())
        {
            impl->computeTabsGeometry();
        }
        else
        {
            impl->determineVisibleTabs();
        }
        impl->computeWidgetGeometry();
        impl->updateOverlays();
    }

    QFrame::resizeEvent(event);
}

void QcwTabFolder::leaveEvent(QEvent* event)
{
    impl->clearHoverState();
    QFrame::leaveEvent(event);
}

void QcwTabFolder::changeEvent(QEvent* event)
{
    switch (event->type())
    {
    case QEvent::LanguageChange:
        // The menu and the button tooltips are strings captured at construction. Resolving them
        // again lands right in the block below: a different language is different text of the
        // same font, so the tab widths measured from the old text have to be recomputed too.
        impl->retranslateUi();
        Q_FALLTHROUGH();
    case QEvent::StyleChange:
    case QEvent::FontChange:
    case QEvent::ApplicationFontChange:
    {
        // Tab widths are measured once, when the text is set, so a new font invalidates them.
        // Without this the tabs keep their old width and the renderer has to elide the new text.
        for (auto tab : std::as_const(impl->tabs))
        {
            tab->updateTextWidth();
        }

        impl->computeTabBarRect();
        // The tab list button's widest count is measured in the current font, so the minimum
        // width reserved for it has to be measured again as well.
        setMinimumWidth(impl->computeMinTabBarWidth());
        impl->determineVisibleTabs();
        impl->computeWidgetGeometry();
        impl->updateOverlays();
        update(impl->tabBarRect);
        break;
    }
    default:
        break;
    }

    QFrame::changeEvent(event);
}

bool QcwTabFolder::eventFilter(QObject* obj, QEvent* event)
{
    // The right widget spends its time in two places: the tab bar, where the folder
    // lays it out, and the top of the pane, where the pane's size drives it and its own
    // size hint may make it fit the bar again.
    if (impl->rightWidget)
    {
        if (obj == impl->current && event->type() == QEvent::Resize)
        {
            // Only the in-pane position is driven by the pane; whether the bar can host
            // the widget is decided by this folder's own resize handling.
            if (impl->rightWidgetInPage)
            {
                impl->layoutRightWidget();
            }
        }
        else if (event->type() == QEvent::LayoutRequest && obj == impl->rightWidget->parentWidget())
        {
            // The widget's size hint changed — posted to this folder, which hosts the
            // widget in both positions. Either way, the bar is measured again.
            if (isVisible())
            {
                impl->determineVisibleTabs();
                update(impl->tabBarRect);
            }
        }
    }

    switch (event->type())
    {
    case QEvent::Enter:
    case QEvent::DragEnter:
        // This filter is also installed on qApp, so ignore events coming from
        // widgets outside our tab pages.
        if (indexOf(qobject_cast<QWidget*>(obj)) >= 0)
        {
            impl->clearHoverState();
        }
        break;
    case QEvent::MouseButtonPress:
    {
        if (auto widget = qobject_cast<QWidget*>(obj))
        {
            if (int index = indexOf(widget); index >= 0)
            {
                emit tabBarClicked(index);
            }
            else if (!isActive() && widget != this && isAncestorOf(widget))
            {
                // A click landed deep inside a view (e.g. a child editor that
                // consumes the press, so the page-level filter never sees it)
                // while this folder is not the active one. Re-clicking an
                // already-focused view does not emit focusChanged, so promote
                // the folder here. The focused view is necessarily the current
                // tab, since hidden pages cannot receive input.
                //
                // The folder itself is excluded: isAncestorOf() answers true for
                // its own events, and a press on the tab bar or the close button
                // would then report the current index before mouseReleaseEvent()
                // reports the real one (or none at all).
                emit tabBarClicked(impl->currentIndex);
            }
        }
        break;
    }
    default:
        break;
    }

    return QFrame::eventFilter(obj, event);
}

qcw::TabPosition QcwTabFolder::tabPosition() const
{
    return impl->tabPosition;
}

void QcwTabFolder::setTabPosition(qcw::TabPosition pos)
{
    if (pos != impl->tabPosition)
    {
        impl->tabPosition = pos;
        impl->computeTabBarRect();
        impl->computeTabsGeometry();
        impl->computeWidgetGeometry();
        impl->updateOverlays();
        update();
    }
}

qcw::FolderFeatures QcwTabFolder::features() const
{
    return impl->features;
}

void QcwTabFolder::setFeatures(qcw::FolderFeatures features)
{
    if (features != impl->features)
    {
        auto oldOn = impl->features.testFlag(qcw::ShowFullTitle);
        auto newOn = features.testFlag(qcw::ShowFullTitle);
        if (oldOn != newOn)
        {
            for (auto tab : std::as_const(impl->tabs))
            {
                if (tab->visible)
                {
                    tab->setTextRectWidth(newOn ? tab->fullTextWidth : impl->minTextRectWidth);
                }
            }
        }

        impl->features = features;
        impl->determineVisibleTabs();
    }
}

void QcwTabFolder::setFeature(qcw::FolderFeature feature, bool on)
{
    if (impl->features.testFlag(feature) != on)
    {
        impl->features.setFlag(feature, on);
        if (feature == qcw::ShowFullTitle)
        {
            for (auto tab : std::as_const(impl->tabs))
            {
                if (tab->visible)
                {
                    tab->setTextRectWidth(on ? tab->fullTextWidth : impl->minTextRectWidth);
                }
            }
        }

        impl->computeTabsGeometry();
        impl->determineVisibleTabs();
    }
}

// tab theming
QcwTabFolderRenderer* QcwTabFolder::renderer() const
{
    return impl->renderer;
}

void QcwTabFolder::setRenderer(QcwTabFolderRenderer* renderer)
{
    if (impl->renderer != renderer)
    {
        delete impl->renderer;
        impl->renderer = renderer;

        if (!impl->renderer)
        {
            impl->renderer = QcwTabFolderRenderer::createDefaultRenderer(this);
        }

        if (isVisible())
        {
            impl->computeTabBarRect();
            impl->determineVisibleTabs();
            impl->computeWidgetGeometry();
            impl->updateOverlays();
            update();
        }
    }
}

int QcwTabFolder::iconSize() const
{
    return impl->iconSize;
}

void QcwTabFolder::setIconSize(int size)
{
    if (impl->iconSize != size)
    {
        impl->iconSize = size;
        impl->computeTabBarRect();
        impl->determineVisibleTabs();
        impl->computeWidgetGeometry();
    }
}

int QcwTabFolder::minimumTextRectWidth() const
{
    return impl->minTextRectWidth;
}

void QcwTabFolder::setMinimumTextRectWidth(int textRectWidth)
{
    if (impl->minTextRectWidth != textRectWidth)
    {
        impl->minTextRectWidth = textRectWidth;
        if (!impl->features.testFlag(qcw::ShowFullTitle))
        {
            impl->determineVisibleTabs();
        }
    }
}

// Implementations of QcwTab
// -----------------------------------------------------------------------------

QcwTab::QcwTab(QWidget* w, const QString& text, const QIcon& icon, QcwTabFolderPrivate* parent) :
    owner { parent }, widget { w }, icon { icon }
{
    setText(text);
    setTextRectWidth(owner->minTextRectWidth);
    int paneId = -1;
    if (w && w->dynamicPropertyNames().contains(qcw::TabPaneIdPropertyName))
    {
        bool ok;
        int id = w->property(qcw::TabPaneIdPropertyName).toInt(&ok);
        if (ok)
        {
            paneId = id;
        }
    }
    data.insert(qcw::TabPaneIdPropertyName, paneId);

#ifdef QCW_DEBUG
    qDebug().noquote().nospace() << "QcwTab(" << this << ' ' << text << ") created";
#endif
}

QcwTab* QcwTab::clone(QcwTabFolderPrivate* parent) const
{
    auto tab = new QcwTab(widget, text, icon, parent);
    tab->toolTip = toolTip;
    tab->data = data;
    tab->fullTextWidth = fullTextWidth;
    tab->textWidth = textWidth;
    tab->shortcutId = 0;
    tab->rect = rect;
    tab->closeRect = closeRect;
    tab->visible = visible;
    tab->closable = closable;
    tab->detachable = detachable;
    tab->dirty = dirty;
    tab->textFullyDisplayed = textFullyDisplayed;

    return tab;
}

int QcwTab::thisIndex() const
{
    return owner->tabs.indexOf(this);
}

void QcwTab::setShowDirty(bool d)
{
    if (dirty != d)
    {
        dirty = d;
        updateTextWidth();
    }
}

void QcwTab::setText(const QString& s)
{
    if (s != text)
    {
        text = s;
        updateTextWidth();
    }
}

void QcwTab::updateTextWidth()
{
    const auto s2 = dirty ? QStringLiteral("*%1").arg(text) : text;

    QFontMetrics fm(owner->owner->font());
    fullTextWidth = fm.boundingRect(0, 0, 0, 0, QcwTabFolderRenderer::TabTextFlags, s2).width();
}

void QcwTab::setTextRectWidth(int w)
{
    if (w != textWidth)
    {
        if (w >= fullTextWidth)
        {
            textWidth = fullTextWidth;
            textFullyDisplayed = true;
        }
        else
        {
            textWidth = w;
            textFullyDisplayed = false;
        }
    }
}

void QcwTab::updateGeometry(int x, int y, bool ignoreFullTitle)
{
    if (!visible || (icon.isNull() && text.isEmpty()))
    {
        rect = QRect();
        closeRect = QRect();
        return;
    }

    const int tabBarSpacing = owner->renderer->tabBarSpacing();
    const int spacing = QcwTabFolderRenderer::TabSpacing;

    int w = spacing;
    bool hasIcon = owner->features.testFlag(qcw::ShowIcon) && !icon.isNull();
    if (hasIcon)
    {
        w += owner->iconSize;
        w += spacing;
    }
    else
    {
        w += tabBarSpacing > 0 ? spacing : spacing * 2; // add spacing even if the icon is not displayed
    }

    if (!text.isEmpty())
    {
        if (fullTextWidth <= textWidth || (owner->features.testFlag(qcw::ShowFullTitle) && !ignoreFullTitle))
        {
            textWidth = fullTextWidth;
            textFullyDisplayed = true;
        }
        else
        {
            textFullyDisplayed = false;
        }

        w += textWidth;
        w += spacing;
    }

    if (y == 0)
    {
        y += owner->tabPosition == qcw::TabPosition::Top ? 0 : 1;
    }

    auto myIndex = thisIndex();
    auto showClose = closable && owner->features.testFlag(qcw::ShowClose);
    auto showUnselectedClose = owner->features.testFlag(qcw::ShowUnselectedClose);
    if (showClose && (showUnselectedClose || myIndex == owner->currentIndex))
    {
        constexpr int closeRectSize = QcwTabFolderRenderer::CloseRectSize;
        closeRect =
            QRect(x + w, y + (owner->tabBarContentsRect.height() - closeRectSize) / 2, closeRectSize, closeRectSize);
        w += closeRectSize;
        w += spacing;
    }
    else
    {
        closeRect = QRect();

        // if the close button is not displayed, additional spacing needs to be added
        w += tabBarSpacing > 0 ? spacing : spacing * 2;
    }

    rect = QRect(x, y, w, owner->tabBarContentsRect.height());
}

// Implementations of QcwTabFolderPrivate
// -----------------------------------------------------------------------------

QcwTabFolderPrivate::QcwTabFolderPrivate(QcwTabFolder* interface) :
    owner { interface }, renderer { QcwTabFolderRenderer::createDefaultRenderer(interface) }
{
}

QcwTabFolderPrivate::~QcwTabFolderPrivate()
{
    if (renderer)
    {
        delete renderer;
    }
}

void QcwTabFolderPrivate::createMenu()
{
    menu = new QMenu(owner);

    closeAction = new QAction(owner);
    QObject::connect(closeAction, &QAction::triggered, [this](bool)
    {
        auto index = indexAt(rightClickPosition);
        if (!isValidIndex(index))
        {
            index = currentIndex;
            if (!isValidIndex(index))
            {
                return;
            }
        }

        if (tabs[index]->closable)
        {
            emit owner->tabCloseRequested(index);
        }
    });

    closeOthersAction = new QAction(owner);
    QObject::connect(closeOthersAction, &QAction::triggered, [this](bool)
    {
        auto index = indexAt(rightClickPosition);
        if (!isValidIndex(index))
        {
            index = currentIndex;
            if (!isValidIndex(index))
            {
                return;
            }
        }

        for (int i = tabs.size() - 1; i >= 0; --i)
        {
            if (i != index && tabs[i]->closable)
            {
                emit owner->tabCloseRequested(i);
            }
        }
    });

    closeLeftTabsAction = new QAction(owner);
    QObject::connect(closeLeftTabsAction, &QAction::triggered, [this](bool)
    {
        auto index = indexAt(rightClickPosition);
        if (!isValidIndex(index))
        {
            index = currentIndex;
            if (!isValidIndex(index))
            {
                return;
            }
        }

        for (int i = index - 1; i >= 0; --i)
        {
            if (tabs[i]->closable)
            {
                emit owner->tabCloseRequested(i);
            }
        }
    });

    closeRightTabsAction = new QAction(owner);
    QObject::connect(closeRightTabsAction, &QAction::triggered, [this](bool)
    {
        auto index = indexAt(rightClickPosition);
        if (!isValidIndex(index))
        {
            index = currentIndex;
            if (!isValidIndex(index))
            {
                return;
            }
        }

        for (int i = tabs.size() - 1; i > index; --i)
        {
            if (tabs[i]->closable)
            {
                emit owner->tabCloseRequested(i);
            }
        }
    });

    closeAllTabsAction = new QAction(owner);
    QObject::connect(closeAllTabsAction, &QAction::triggered, [this](bool)
    {
        for (int i = tabs.size() - 1; i >= 0; --i)
        {
            if (tabs[i]->closable)
            {
                emit owner->tabCloseRequested(i);
            }
        }
    });

    detachAction = new QAction(owner);
    QObject::connect(detachAction, &QAction::triggered, [this](bool)
    {
        auto index = indexAt(rightClickPosition);
        if (!isValidIndex(index))
        {
            index = currentIndex;
            if (!isValidIndex(index))
            {
                return;
            }
        }

        if (tabs[index]->detachable)
        {
            detachTab(index);
        }
    });

    settingsAction = new QAction(owner);
    QObject::connect(settingsAction, &QAction::triggered, [this](bool)
    {
        auto dialog = new QcwTabFolderSettingsDialog(owner);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });

    menu->addAction(closeAction);
    menu->addAction(closeOthersAction);
    menu->addAction(closeLeftTabsAction);
    menu->addAction(closeRightTabsAction);
    menu->addSeparator();
    menu->addAction(closeAllTabsAction);
    menu->addSeparator();
    menu->addAction(detachAction);
    menu->addSeparator();
    menu->addAction(settingsAction);

    QObject::connect(owner, &QcwTabFolder::customContextMenuRequested, [this](const QPoint& pos)
    {
        if (!tabBarRect.contains(pos))
        {
            return;
        }

        bool showTabActions = !tabs.empty();

        auto index = indexAt(pos);
        if (!isValidIndex(index))
        {
            index = currentIndex;
            if (!isValidIndex(index))
            {
                showTabActions = false;
            }
        }

        if (showTabActions)
        {
            rightClickPosition = pos;

            int leftClosables = 0;
            int rightClosables = 0;
            for (int i = 0; i < tabs.size(); ++i)
            {
                if (tabs[i]->closable)
                {
                    if (i < index)
                    {
                        ++leftClosables;
                    }
                    else if (i > index)
                    {
                        ++rightClosables;
                    }
                }
            }

            bool closable = owner->isTabClosable(index);
            int allClosables = leftClosables + rightClosables + closable;

            // set actions' status
            closeAction->setVisible(closable);
            closeOthersAction->setVisible(leftClosables || rightClosables);

            closeLeftTabsAction->setVisible(leftClosables);
            closeRightTabsAction->setVisible(rightClosables);

            closeAllTabsAction->setVisible(allClosables > 0 && allClosables != closable);

            detachAction->setVisible(owner->isTabDetachable(index));
        }
        else
        {
            closeAction->setVisible(false);
            closeOthersAction->setVisible(false);

            closeLeftTabsAction->setVisible(false);
            closeRightTabsAction->setVisible(false);

            closeAllTabsAction->setVisible(false);

            detachAction->setVisible(false);
        }

        bool showSettingsAction = true;
        auto site = owner->parentWidget();
        if (site)
        {
            showSettingsAction = !site->windowFlags().testFlag(Qt::Popup);
        }
        settingsAction->setVisible(showSettingsAction);

        if (showTabActions || showSettingsAction)
        {
            menu->exec(owner->mapToGlobal(pos));
        }
    });
}

void QcwTabFolderPrivate::createTabBarButtons()
{
    QSize btnSize(QcwTabFolderRenderer::ButtonSize, QcwTabFolderRenderer::ButtonSize);
    QSize iconSize(btnSize.width() - 4, btnSize.height() - 4);

    minimizeButton = new QToolButton(owner);
    minimizeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    minimizeButton->setObjectName("minButton");
    minimizeButton->setFixedSize(btnSize);
    minimizeButton->setAutoRaise(true);
    minimizeButton->setVisible(features.testFlag(qcw::Minimizable));
    minimizeButton->setFocusPolicy(Qt::ClickFocus);
    minimizeButton->setIcon(renderer->renderMinimizeIcon());

    QObject::connect(minimizeButton, &QToolButton::clicked, [this](bool)
    {
        if (features.testFlag(qcw::Minimizable))
        {
            emit owner->minimizeRequested();
        }
    });

    maximizeButton = new QToolButton(owner);
    maximizeButton->setObjectName("maxButton");
    maximizeButton->setFixedSize(btnSize);
    maximizeButton->setAutoRaise(true);
    maximizeButton->setVisible(features.testFlag(qcw::Maximizable));
    maximizeButton->setFocusPolicy(Qt::ClickFocus);
    maximizeButton->setIcon(renderer->renderMaximizeIcon(state));

    QObject::connect(maximizeButton, &QToolButton::clicked, [this](bool)
    {
        if (features.testFlag(qcw::Maximizable))
        {
            if (state != qcw::FolderState::Normal)
            {
                emit owner->restoreRequested();
            }
            else
            {
                emit owner->maximizeRequested();
            }
        }
    });

    // The width follows the number of hidden tabs, so only the height is fixed here; the
    // folder lays the button out and asks sizeHint() for its width.
    tabListButton = new QcwTabListButton(owner);
    tabListButton->setObjectName("tabListButton");
    tabListButton->setFixedHeight(btnSize.height());
    tabListButton->setAutoRaise(true);
    tabListButton->setVisible(tabs.size() > visibleCount());
    tabListButton->setFocusPolicy(Qt::ClickFocus);

    QObject::connect(tabListButton, &QToolButton::clicked, [this](bool)
    {
        if (tabs.size() <= visibleCount())
        {
            return;
        }

        QMap<int, QPair<QString, QIcon>> hiddenTabs;
        QMap<int, QPair<QString, QIcon>> visibleTabs;

        for (int i = 0; i < tabs.size(); ++i)
        {
            auto tab = tabs[i];
            if (!tab->visible)
            {
                hiddenTabs.insert(i, { tab->text, tab->icon });
            }
            else
            {
                visibleTabs.insert(i, { tab->text, tab->icon });
            }
        }

        auto listWidget = new QcwFilterableListWidget(true, owner);
        listWidget->setWindowFlag(Qt::Popup);
        listWidget->setAttribute(Qt::WA_DeleteOnClose);

        auto font = owner->font();
        font.setBold(true);
        for (const auto& [index, p] : hiddenTabs.asKeyValueRange())
        {
            auto item = listWidget->addItem(p.first, p.second, font);
            item->setData(index, Qt::UserRole);
        }

        for (const auto& [index, p] : visibleTabs.asKeyValueRange())
        {
            auto item = listWidget->addItem(p.first, p.second, owner->font());
            item->setData(index, Qt::UserRole);
        }

        QObject::connect(listWidget, &QcwFilterableListWidget::itemSelected, [this, listWidget](QStandardItem* item)
        {
            setCurrentIndex(item->data(Qt::UserRole).toInt());
            listWidget->close();
        });

        // The screen the popup has to stay inside is the one the button it is anchored to is on.
        // Asking the cursor instead answers nullptr whenever its position belongs to no screen -
        // a synthesised click, or a pointer between monitors - and then clamps the popup against
        // a monitor the folder is not even on.
        auto screen = tabListButton->screen();
        auto screenRect = screen->availableGeometry();

        int spacing = (tabBarRect.height() - tabListButton->height()) / 2;

        QPoint globalPos;
        if (tabPosition == qcw::TabPosition::Top)
        {
            globalPos = tabListButton->mapToGlobal(tabListButton->rect().bottomLeft());
            globalPos.setY(globalPos.y() + spacing + 1);

            int listWidgetBottom = globalPos.y() + listWidget->height();
            if (listWidgetBottom > screenRect.bottom())
            {
                int diff = listWidgetBottom - screenRect.bottom();
                globalPos.setY(globalPos.y() - diff);
            }
        }
        else
        {
            globalPos = tabListButton->mapToGlobal(tabListButton->rect().topLeft());
            globalPos.setY(globalPos.y() - listWidget->height() - spacing - 1);

            if (globalPos.y() < screenRect.top())
            {
                globalPos.setY(screenRect.top());
            }
        }

        int listWidgetRight = globalPos.x() + listWidget->width();
        if (listWidgetRight > screenRect.right())
        {
            int diff = listWidgetRight - screenRect.right();
            globalPos.setX(globalPos.x() - diff);
        }

        listWidget->move(globalPos);
        listWidget->show();
    });
}

void QcwTabFolderPrivate::retranslateUi()
{
    closeAction->setText(QCoreApplication::translate("QcwTabFolder", "&Close"));
    closeOthersAction->setText(QCoreApplication::translate("QcwTabFolder", "Close &Others"));
    closeLeftTabsAction->setText(QCoreApplication::translate("QcwTabFolder", "Close Tabs to the &Left"));
    closeRightTabsAction->setText(QCoreApplication::translate("QcwTabFolder", "Close Tabs to the &Right"));
    closeAllTabsAction->setText(QCoreApplication::translate("QcwTabFolder", "Close &All"));
    detachAction->setText(QCoreApplication::translate("QcwTabFolder", "&Detach"));
    settingsAction->setText(QCoreApplication::translate("QcwTabFolder", "Folder Settings..."));

    minimizeButton->setToolTip(QCoreApplication::translate("QcwTabFolder", "Minimize"));
    maximizeButton->setToolTip(
        state == qcw::FolderState::Normal ? QCoreApplication::translate("QcwTabFolder", "Maximize")
                                          : QCoreApplication::translate("QcwTabFolder", "Restore"));
    tabListButton->setToolTip(QCoreApplication::translate("QcwTabFolder", "Show Hidden Tabs"));
}

void QcwTabFolderPrivate::createOverlays()
{
    for (int corner = Qt::TopLeftCorner; corner <= Qt::BottomRightCorner; ++corner)
    {
        cornerMasks[corner] = new QcwCornerMaskOverlay(owner, static_cast<Qt::Corner>(corner));
    }

    for (int strip = 0; strip < BorderStripCount; ++strip)
    {
        borderStrips[strip] = new QcwBorderStrip(owner);
    }
}

void QcwTabFolderPrivate::updateOverlays()
{
    const QcwTabFolderRenderer::FrameMask mask = renderer ? renderer->frameMask() : QcwTabFolderRenderer::FrameMask {};
    const QRect folderRect = owner->rect();

    // Corner masks: the tab bar keeps its own edge free, so only the corners opposite it need
    // an overlay.
    const int radius = mask.cornerRadius;
    const bool cornersEnabled = radius > 0 && mask.surroundColor.isValid() && folderRect.width() >= 2 * radius &&
                                folderRect.height() >= 2 * radius;
    const bool showTop = cornersEnabled && tabPosition == qcw::TabPosition::Bottom;
    const bool showBottom = cornersEnabled && tabPosition == qcw::TabPosition::Top;
    const bool cornerVisibles[4] = { showTop, showTop, showBottom, showBottom };
    // Each overlay also paints a short piece of the straight border on both sides of its arc.
    const int overlaySize = radius + CornerBorderOverlap;
    const QPoint cornerPositions[4] = { QPoint { 0, 0 }, QPoint { folderRect.width() - overlaySize, 0 },
                                        QPoint { 0, folderRect.height() - overlaySize },
                                        QPoint { folderRect.width() - overlaySize,
                                                 folderRect.height() - overlaySize } };

    for (int corner = Qt::TopLeftCorner; corner <= Qt::BottomRightCorner; ++corner)
    {
        auto overlay = cornerMasks[corner];
        if (!overlay)
        {
            continue;
        }

        overlay->setCornerShape(radius, mask.surroundColor, mask.borderWidth, mask.borderColor);

        if (!cornerVisibles[corner])
        {
            overlay->hide();
            continue;
        }

        overlay->setGeometry(QRect(cornerPositions[corner], QSize(overlaySize, overlaySize)));
        overlay->show();
    }

    // Border strips: the page covers the border along its own edges. The page area is the one
    // computeWidgetGeometry() gives it.
    const int border = mask.borderWidth;
    const bool borderEnabled =
        border > 0 && mask.borderColor.isValid() && folderRect.width() > 2 * border && folderRect.height() > 2 * border;
    const int pageTop = tabPosition == qcw::TabPosition::Top ? tabBarRect.height() : 0;
    const int pageBottom =
        tabPosition == qcw::TabPosition::Top ? folderRect.height() - 1 : folderRect.height() - 1 - tabBarRect.height();
    const int pageHeight = pageBottom - pageTop + 1;
    const QRect stripRects[BorderStripCount] = {
        QRect { 0, pageTop, border, pageHeight }, QRect { 0, 0, folderRect.width(), border },
        QRect { folderRect.width() - border, pageTop, border, pageHeight },
        QRect { 0, folderRect.height() - border, folderRect.width(), border }
    };
    const bool stripVisibles[BorderStripCount] = { borderEnabled,
                                                   borderEnabled && tabPosition == qcw::TabPosition::Bottom,
                                                   borderEnabled,
                                                   borderEnabled && tabPosition == qcw::TabPosition::Top };

    for (int strip = 0; strip < BorderStripCount; ++strip)
    {
        auto overlay = borderStrips[strip];
        if (!overlay)
        {
            continue;
        }

        overlay->setBorderColor(mask.borderColor);

        if (!stripVisibles[strip])
        {
            overlay->hide();
            continue;
        }

        overlay->setGeometry(stripRects[strip]);
        overlay->show();
    }

    raiseOverlays();
}

void QcwTabFolderPrivate::raiseOverlays()
{
    // The corner overlays repaint the border arc, so they stay above the straight strips.
    for (auto strip : borderStrips)
    {
        if (strip)
        {
            strip->raise();
        }
    }

    for (auto overlay : cornerMasks)
    {
        if (overlay)
        {
            overlay->raise();
        }
    }
}

int QcwTabFolderPrivate::addTab(QWidget* widget, const QString& text, const QIcon& icon)
{
    // A folder shows a pane at most once: the manager keeps one reference per
    // (pane, folder), so a second tab for the same pane would be released by the
    // first removeTab() and delete the pane under the surviving tab.
    if (hasPane(widget))
    {
        return -1;
    }

    return addTab(new QcwTab(widget, text, icon, this));
}

int QcwTabFolderPrivate::addTab(QcwTab* tab)
{
    // Preconditions: the caller owns @p tab and has cleared it against hasPane().
    // takeTab() already holds the adoptor's reference here, so the manager cannot
    // answer the question at this point.
    int index = tabs.size();
    tabs.append(tab);
    recentIndexes.append(index);

    setCurrentIndex(index);

    if (tab->widget && !QKeySequence::mnemonic(tab->text).isEmpty())
    {
        tab->shortcutId = owner->grabShortcut(QKeySequence::mnemonic(tab->text));
        owner->setShortcutEnabled(tab->shortcutId, tab->visible);
    }

    return index;
}

int QcwTabFolderPrivate::insertTab(int index, QWidget* widget, const QString& text, const QIcon& icon)
{
    if (hasPane(widget))
    {
        return -1;
    }

    return insertTab(index, new QcwTab(widget, text, icon, this));
}

int QcwTabFolderPrivate::insertTab(int index, QcwTab* tab)
{
    // Same preconditions as addTab().
    if (!isValidIndex(index))
    {
        index = tabs.size();
        tabs.append(tab);
    }
    else
    {
        tabs.insert(index, tab);
        for (auto& i : recentIndexes)
        {
            if (i >= index)
            {
                ++i;
            }
        }
    }
    recentIndexes.append(index);

    if (tab->widget && !QKeySequence::mnemonic(tab->text).isEmpty())
    {
        tab->shortcutId = owner->grabShortcut(QKeySequence::mnemonic(tab->text));
        owner->setShortcutEnabled(tab->shortcutId, tab->visible);
    }

    setCurrentIndex(index);

    return index;
}

bool QcwTabFolderPrivate::removeTab(int index)
{
    if (isValidIndex(index))
    {
        auto tab = tabs[index];

        emit owner->tabAboutToBeRemoved(index);

        updateRecentIndexes(index);
        tabs.remove(index);

        if (tab->widget)
        {
            releasePane(tab->widget);
        }

        auto newIndex = currentIndex;
        if (index < currentIndex)
        {
            newIndex = currentIndex - 1;
        }
        else if (index == currentIndex)
        {
            newIndex = tabs.empty() || recentIndexes.empty() ? -1 : recentIndexes.back();
        }

        setCurrentIndex(newIndex);
        owner->releaseShortcut(tab->shortcutId);

        delete tab;

        // The remaining tabs shifted down, so any hover state points at a
        // different tab now (or at nothing at all).
        clearHoverState();

        return true;
    }

    return false;
}

bool QcwTabFolderPrivate::hasPane(QWidget* pane) const
{
    // The manager keeps exactly the set of folders that show a pane, so it is
    // both the invariant being protected and the cheapest way to query it.
    return pane && tabPaneManager->getReferences(pane).contains(owner);
}

void QcwTabFolderPrivate::releasePane(QWidget* pane)
{
    // A pane stays a child of the folder that last showed it, and Qt destroys
    // children with their parent. Handing it out of this folder is what keeps a
    // live widget for the folders that still hold it; when this folder is the last
    // holder, the manager deletes the pane anyway.
    if (pane->parentWidget() == owner)
    {
        pane->setParent(nullptr);
    }

    tabPaneManager->removeReference(pane, owner);
}

void QcwTabFolderPrivate::moveTab(int indexFrom, int indexTo)
{
    if (indexFrom == indexTo)
    {
        return;
    }

    tabs.move(indexFrom, indexTo);

    // Every index-typed member refers to positions that just shuffled: remap
    // them all, otherwise MRU hiding picks the wrong tabs and the current tab
    // silently changes under the user.
    const auto remap = [indexFrom, indexTo](int i)
    {
        if (i == indexFrom)
        {
            return indexTo;
        }
        if (indexFrom < indexTo && i > indexFrom && i <= indexTo)
        {
            return i - 1;
        }
        if (indexFrom > indexTo && i >= indexTo && i < indexFrom)
        {
            return i + 1;
        }
        return i;
    };

    for (auto& i : recentIndexes)
    {
        i = remap(i);
    }

    currentIndex = remap(currentIndex);
    hoveredIndex = remap(hoveredIndex);
    pressedIndex = remap(pressedIndex);
}

void QcwTabFolderPrivate::detachTab(int index)
{
    auto folder = new QcwTabFolder;
    folder->setFloating(true);
    folder->setAttribute(Qt::WA_DeleteOnClose);

    // Stamped before the tab arrives: a pane reads its window off the folder it lands in, and
    // the id has to be there by then or the view cannot tell the floating folder from no window.
    folder->setProperty("WindowId", owner->property("WindowId").toString());

    int newIndex = -1;
    owner->takeTab(folder, index, newIndex);

    if (newIndex == -1)
    {
        folder->close();
    }
    else
    {
        folder->setRenderer(renderer->clone(folder));
        folder->resize(owner->size());
        folder->setFeature(qcw::Maximizable, false);
        folder->setFeature(qcw::Minimizable, false);
        auto icon = folder->tabIcon(newIndex);
        if (!icon.isNull())
        {
            folder->setWindowIcon(icon);
        }

        folder->show();

        emit floatingFolderMonitor->floatingFolderCreated(folder);
    }
}

void QcwTabFolderPrivate::adoptTab(QcwTabFolder* srcFolder, int srcIndex, int myTargetIndex)
{
    if (!srcFolder || !srcFolder->isValidIndex(srcIndex))
    {
        return;
    }

    srcFolder->takeTab(owner, srcIndex, myTargetIndex);
}

void QcwTabFolderPrivate::checkAfterTabRemoved()
{
    if (!tabs.empty())
    {
        return;
    }

    // An empty folder must always display its placeholder. Without this, a
    // floating folder whose close() is vetoed keeps living with a null
    // `current`, and the next resize/style change dereferences it.
    showWidget(-1);

    if (floating)
    {
        owner->close();
        return;
    }

    emit owner->tabsCleared();
}

void QcwTabFolderPrivate::paintTabs(QPainter& painter) const
{
    for (int i = 0; i < tabs.size(); ++i)
    {
        auto tab = tabs[i];
        if (!tab->visible)
        {
            continue;
        }

        bool selected = i == currentIndex;
        bool hovered = i == hoveredIndex;

        int state = QcwTabFolderRenderer::Normal;
        if (selected)
        {
            state = (state | QcwTabFolderRenderer::Pressed);
        }
        if (hovered)
        {
            state = (state | QcwTabFolderRenderer::Hovered);
        }

        auto closeBtnState =
            (hovered && closeButtonHovered) ? QcwTabFolderRenderer::Hovered : QcwTabFolderRenderer::Normal;
        auto showClose =
            features.testFlag(qcw::ShowClose) && (selected || (hovered && features.testFlag(qcw::ShowUnselectedClose)));

        renderer->renderTab(painter, QcwTabFolderRenderer::TabState(state), tab->rect, closeBtnState, tab->closeRect,
                            tab->icon, tab->text, tab->textWidth, features.testFlag(qcw::ShowIcon),
                            tab->textFullyDisplayed, showClose, tab->dirty);
    }
}

QPixmap QcwTabFolderPrivate::renderDragImage() const
{
    auto tab = tabs[pressedIndex];
    auto tabRect = tab->rect;

    int w = tabRect.width();
    if (features.testFlag(qcw::ShowClose) && tab->closable &&
        (pressedIndex == currentIndex || features.testFlag(qcw::ShowUnselectedClose)))
    {
        // Subtract the width of the close button (don't render the close button).
        w -= (QcwTabFolderRenderer::CloseRectSize + QcwTabFolderRenderer::TabSpacing);
    }

    QPixmap dragPixmap(w, tabRect.height());
    dragPixmap.fill(Qt::transparent);

    QPainter painter(&dragPixmap);

    bool selected = pressedIndex == currentIndex;
    bool hovered = pressedIndex == hoveredIndex;

    int state = QcwTabFolderRenderer::Normal;
    if (selected)
    {
        state = (state | QcwTabFolderRenderer::Pressed);
    }
    if (hovered)
    {
        state = (state | QcwTabFolderRenderer::Hovered);
    }

    renderer->renderTab(painter, QcwTabFolderRenderer::TabState(state), dragPixmap.rect(), QcwTabFolderRenderer::Normal,
                        QRect(), tab->icon, tab->text, tab->textWidth, features.testFlag(qcw::ShowIcon),
                        tab->textFullyDisplayed, false, tab->dirty);

    painter.end();

    return dragPixmap;
}

void QcwTabFolderPrivate::setCurrentIndex(int index)
{
    if (!isValidIndex(index))
    {
        // Only -1 is meaningful here ("show no page"). Any other invalid index
        // is ignored, rather than reported as a change that never happened.
        if (index != -1 || currentIndex == -1)
        {
            return;
        }

        currentIndex = index;
        showWidget(index);
        determineVisibleTabs();

        emit owner->currentChanged(index);
        return;
    }

    recentIndexes.removeAll(index);
    recentIndexes.append(index);
    currentIndex = index;

    determineVisibleTabs();
    showWidget(index);

    if (floating)
    {
        auto tab = tabs[index];

        owner->setWindowTitle(tab->text);
        if (!tab->icon.isNull())
        {
            owner->setWindowIcon(tab->icon);
        }
    }

    emit owner->currentChanged(index);
}

QcwTab* QcwTabFolderPrivate::tabAt(const QPoint& pos) const
{
    for (auto tab : tabs)
    {
        if (tab->visible && tab->rect.contains(pos))
        {
            return tab;
        }
    }

    return nullptr;
}

int QcwTabFolderPrivate::indexAt(const QPoint& pos) const
{
    for (int i = 0; i < tabs.size(); ++i)
    {
        auto tab = tabs[i];
        if (tab->visible && tab->rect.contains(pos))
        {
            return i;
        }
    }

    return -1;
}

bool QcwTabFolderPrivate::isMouseOn(QWidget* childWidget, const QPoint& pos) const
{
    if (!childWidget || !childWidget->isVisible())
    {
        return false;
    }

    // pos comes from this folder's own mouse events, so it is a point in the folder; geometry()
    // is the child's rect in parent coordinates, which is what they are comparable against.
    return childWidget->geometry().contains(pos);
}

bool QcwTabFolderPrivate::isMouseOnAnyWidget(const QPoint& pos) const
{
    return isMouseOn(minimizeButton, pos) || isMouseOn(maximizeButton, pos) || isMouseOn(tabListButton, pos) ||
           isMouseOn(rightWidget, pos);
}

void QcwTabFolderPrivate::clearHoverState()
{
    hoveredIndex = -1;
    closeButtonHovered = false;
    dragMoveInfo.first = -1;

    owner->update(tabBarRect);
}

int QcwTabFolderPrivate::nextVisibleIndex(int index) const
{
    for (auto i = index + 1; i < tabs.size(); ++i)
    {
        if (tabs[i]->visible)
        {
            return i;
        }
    }
    return -1;
}

int QcwTabFolderPrivate::prevVisibleIndex(int index) const
{
    for (auto i = index - 1; i >= 0; --i)
    {
        if (tabs[i]->visible)
        {
            return i;
        }
    }
    return -1;
}

int QcwTabFolderPrivate::firstVisibleIndex() const
{
    return nextVisibleIndex(-1);
}

int QcwTabFolderPrivate::lastVisibleIndex() const
{
    return prevVisibleIndex(tabs.size());
}

void QcwTabFolderPrivate::updateRecentIndexes(int indexToRemove)
{
    recentIndexes.removeAll(indexToRemove);
    for (auto& i : recentIndexes)
    {
        if (i > indexToRemove)
        {
            --i;
        }
    }
}

int QcwTabFolderPrivate::computeMinTabBarWidth()
{
    const int innerSpacing = QcwTabFolderRenderer::TabSpacing;
    const int tabBarSpacing = renderer->tabBarSpacing();

    int w = innerSpacing;
    if (features.testFlag(qcw::ShowIcon)) // icon
    {
        w += iconSize;
        w += innerSpacing;
    }
    else
    {
        w += tabBarSpacing > 0 ? innerSpacing : innerSpacing * 2;
    }

    w += 60; // minimum text width
    w += innerSpacing;

    if (features.testFlag(qcw::ShowClose)) // close button
    {
        w += QcwTabFolderRenderer::CloseRectSize;
        w += innerSpacing;
    }
    else
    {
        w += tabBarSpacing > 0 ? innerSpacing : innerSpacing * 2;
    }

    // The tab list button has to fit "99+" even when the bar is at its minimum, or the folder
    // would be able to shrink until the button covered the tab it follows.
    w += tabListButton->maxWidth();
    w += QcwTabFolderRenderer::ButtonSize * 2;
    w += innerSpacing * 3;

    return w;
}

void QcwTabFolderPrivate::computeTabBarRect()
{
    const int tabSpacing = QcwTabFolderRenderer::TabSpacing;
    const int closeRectSize = QcwTabFolderRenderer::CloseRectSize;

    QFontMetrics fm(owner->font());
    auto maxh = features.testFlag(qcw::ShowIcon) ? qMax(iconSize, fm.height()) : fm.height();
    int tabHeight = qMax(closeRectSize, maxh) + tabSpacing * 2;

    auto minTabHeight = closeRectSize + tabSpacing * 2;
    if (tabHeight < minTabHeight)
    {
        tabHeight = minTabHeight;
    }

    const auto margins = renderer->tabBarPadding();
    int tabBarHight = tabHeight + margins.top() + margins.bottom();
    tabBarHight = qMax(tabBarHight, QcwTabFolderRenderer::ButtonSize + tabSpacing * 2);

    tabBarRect = owner->contentsRect();
    if (tabPosition == qcw::TabPosition::Top)
    {
        tabBarRect.adjust(0, 0, 0, -(tabBarRect.height() - tabBarHight));
    }
    else
    {
        tabBarRect.adjust(0, tabBarRect.height() - tabBarHight, 0, 0);
    }

    tabBarContentsRect = tabBarRect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
}

void QcwTabFolderPrivate::setRightWidget(QWidget* widget, QWidget* widgetOwner)
{
    if (rightWidget == widget)
    {
        return;
    }

    // The previous widget belongs to its caller as well; hand it back before replacing it.
    // releaseRightWidget() also drops the connection a caller-deleted widget left behind.
    releaseRightWidget();

    rightWidget = widget;
    rightWidgetOwner = widgetOwner;

    // computeTabsGeometry() owns the geometry and the visibility of the widget, so it
    // starts hidden: the folder is not laid out yet here.
    if (widget)
    {
        widget->hide();

        if (widgetOwner)
        {
            // The owner cannot take the widget back once it is gone, and while the widget is
            // parked here the folder is the only one that can clean it up.
            rightWidgetOwnerConnection = QObject::connect(widgetOwner, &QObject::destroyed, owner, [this]()
            {
                // Only the folder hosting the widget still has to delete it; when another
                // folder installed it in the meantime, that folder handles the owner's end.
                if (rightWidget && rightWidget->parentWidget() == owner)
                {
                    rightWidget->hide();
                    rightWidget->deleteLater();
                }

                rightWidget = nullptr;
                rightWidgetOwner = nullptr;
                rightWidgetOwnerConnection = {};
            });
        }
    }
    else
    {
        // Removing the widget gives the pane back the strip the widget occupied.
        rightWidgetInPage = false;
        if (current)
        {
            computeWidgetGeometry();
        }
    }

    if (owner->isVisible())
    {
        if (tabs.empty())
        {
            computeTabsGeometry();
        }
        else
        {
            determineVisibleTabs();
        }

        owner->update(tabBarRect);
    }
}

void QcwTabFolderPrivate::releaseRightWidget()
{
    if (rightWidgetOwnerConnection)
    {
        QObject::disconnect(rightWidgetOwnerConnection);
        rightWidgetOwnerConnection = {};
    }

    if (!rightWidget)
    {
        rightWidgetOwner = nullptr;
        return;
    }

    auto widget = rightWidget;
    auto widgetOwner = rightWidgetOwner;

    rightWidget = nullptr;
    rightWidgetOwner = nullptr;

    // Another folder displaying the same widget may have installed it since this one did.
    // That folder owns the placement then; touching the widget here would take it away and
    // hand it to an owner that may be the other folder's predecessor.
    if (widget->parentWidget() != owner)
    {
        return;
    }

    widget->hide();
    if (widget->parentWidget() != widgetOwner)
    {
        widget->setParent(widgetOwner);
    }
}

void QcwTabFolderPrivate::layoutRightWidget()
{
    if (!rightWidget || !current || !rightWidgetInPage)
    {
        return;
    }

    const QRect page = pageRect();
    const int reserved = placeRightWidgetInPage(page);
    current->setGeometry(page.x(), page.y() + reserved, page.width(), page.height() - reserved);
}

QRect QcwTabFolderPrivate::pageRect() const
{
    auto r = owner->contentsRect();
    if (tabPosition == qcw::TabPosition::Top)
    {
        r.adjust(0, tabBarRect.height(), 0, 0);
    }
    else
    {
        r.adjust(0, 0, 0, -tabBarRect.height());
    }

    // A renderer that paints a rounded folder background needs the page kept clear of the
    // corners; an opaque page widget would otherwise paint over them.
    const auto margins = renderer->pageMargins();
    r.adjust(margins.left(), margins.top(), -margins.right(), -margins.bottom());
    return r;
}

/// Places the right widget in the top-right corner of @p page and returns the height the
/// pane has to leave for it. Zero means the widget is hidden and the pane keeps the page.
int QcwTabFolderPrivate::placeRightWidgetInPage(const QRect& page)
{
    int width = rightWidget->sizeHint().width();
    int height = rightWidget->sizeHint().height();
    width = qBound(rightWidget->minimumWidth(), width, rightWidget->maximumWidth());
    height = qBound(rightWidget->minimumHeight(), height, rightWidget->maximumHeight());
    width = qMin(width, page.width());
    height = qMin(height, page.height());

    // The pane keeps at least part of the page; a widget that would take all of it stays
    // hidden rather than leaving nothing of the view on screen.
    if (width <= 0 || height <= 0 || height >= page.height())
    {
        rightWidget->hide();
        return 0;
    }

    if (rightWidget->parentWidget() != owner)
    {
        rightWidget->setParent(owner);
    }

    rightWidget->setGeometry(page.x() + page.width() - width, page.y(), width, height);
    rightWidget->raise();
    rightWidget->show();
    return height;
}

int QcwTabFolderPrivate::computeTabsGeometry()
{
    const int buttonSize = QcwTabFolderRenderer::ButtonSize;
    const int tabSpacing = QcwTabFolderRenderer::TabSpacing;
    const int tabBarSpacing = renderer->tabBarSpacing();

    int rightSpacing = (tabSpacing > tabBarSpacing ? tabSpacing : 0);
    int btnX = tabBarContentsRect.right() - rightSpacing;
    int btnY = tabBarContentsRect.top() + (tabBarContentsRect.height() - buttonSize) / 2;

    int usedSpace = rightSpacing; // right spacing
    bool prevButton = false;
    if (features.testFlag(qcw::Maximizable))
    {
        btnX -= buttonSize;
        maximizeButton->setGeometry(btnX, btnY, buttonSize, buttonSize);
        maximizeButton->setVisible(true);
        usedSpace += buttonSize;
        prevButton = true;
    }
    else
    {
        maximizeButton->setVisible(false);
    }

    if (features.testFlag(qcw::Minimizable))
    {
        if (prevButton)
        {
            btnX -= tabSpacing;
            usedSpace += tabSpacing;
        }
        btnX -= buttonSize;
        minimizeButton->setGeometry(btnX, btnY, buttonSize, buttonSize);
        minimizeButton->setVisible(true);
        usedSpace += buttonSize;
    }
    else
    {
        minimizeButton->setVisible(false);
    }

    int tabX = tabBarContentsRect.left();
    int tabY = tabBarContentsRect.top();

    for (int i = 0; i < tabs.size(); ++i)
    {
        auto tab = tabs[i];
        if (tab->visible)
        {
            auto ignoreFullTitle = freeSpace < 0 && visibleCount() == 1 && i == currentIndex;

            tab->updateGeometry(tabX, tabY, ignoreFullTitle);
            tabX += tab->rect.width();
            tabX += tabBarSpacing; // space between two tabs
        }
    }

    auto hiddenTabs = tabs.size() - visibleCount();
    // Which answer the button shows is published by the settled layout only, not by every
    // measure that leads up to it; see determineVisibleTabs().
    if (tabListSettling == 0)
    {
        tabListButton->setVisible(hiddenTabs > 0);
    }

    if (hiddenTabs > 0)
    {
        tabX += (tabBarSpacing == 0 ? tabSpacing : 0);

        // The width grows with the count, and this measure is what decides how many tabs fit
        // in the next pass: hiding one more tab can only widen the button, so the passes settle
        // instead of alternating between two counts.
        tabListButton->setHiddenCount(hiddenTabs);
        const int buttonWidth = tabListButton->sizeHint().width();

        tabListButton->setGeometry(tabX, tabBarContentsRect.top() + (tabBarContentsRect.height() - buttonSize) / 2,
                                   buttonWidth, buttonSize);
        tabX += buttonWidth;
    }

    usedSpace += tabX;
    usedSpace += tabSpacing;

    freeSpace = tabBarContentsRect.width() - usedSpace;

    // The tabs decide how much of the strip is theirs; the right widget only takes what
    // is left next to the folder buttons, and steps out of the way while that gap is
    // narrower than its hint or some tabs are still hidden behind the tab list button.
    // freeSpace stays as measured for the tabs, so the widget never changes which tabs
    // are visible. Without room in the strip, the widget moves to the top of the pane and
    // the pane makes room for it instead of being covered.
    if (rightWidget)
    {
        int width = rightWidget->sizeHint().width();
        int height = rightWidget->sizeHint().height();
        width = qBound(rightWidget->minimumWidth(), width, rightWidget->maximumWidth());
        height = qBound(rightWidget->minimumHeight(), height, rightWidget->maximumHeight());
        height = qMin(height, tabBarContentsRect.height());

        if (width > 0 && hiddenTabs == 0 && freeSpace >= width + tabSpacing)
        {
            if (rightWidget->parentWidget() != owner)
            {
                rightWidget->setParent(owner);
            }

            const bool wasInPage = rightWidgetInPage;
            rightWidgetInPage = false;

            rightWidget->setGeometry(btnX - tabSpacing - width,
                                     tabBarContentsRect.top() + (tabBarContentsRect.height() - height) / 2, width,
                                     height);
            rightWidget->show();

            // Back in the bar, the pane no longer leaves the strip it occupied.
            if (wasInPage)
            {
                computeWidgetGeometry();
            }
        }
        else
        {
            rightWidgetInPage = true;
            layoutRightWidget();
        }
    }

    return freeSpace;
}

void QcwTabFolderPrivate::computeWidgetGeometry()
{
    auto r = pageRect();

    int reserved = 0;
    if (rightWidget && rightWidgetInPage)
    {
        reserved = placeRightWidgetInPage(r);
    }

    current->setGeometry(r.x(), r.y() + reserved, r.width(), r.height() - reserved);
}

void QcwTabFolderPrivate::determineVisibleTabs()
{
    // Every pass below re-answers whether the tab list button is needed, so that answer is
    // held back until the tabs have settled: hiding a child and showing it again posts a
    // QEvent::LayoutRequest to a parent that has no layout (QWidgetPrivate::setVisible_helper),
    // and this folder answers that request by measuring the tabs again - so an answer that only
    // held for one of the passes re-armed the next one, forever, with the button flickering in
    // between. The outermost pass applies the settled answer once.
    ++tabListSettling;

    do
    {
        if (tabs.empty())
        {
            current->show();
            break;
        }

        const bool showFullTitle = features.testFlag(qcw::ShowFullTitle);
        const bool useMRU = features.testFlag(qcw::UseMRU);
        const bool multiTabs = features.testFlag(qcw::MultiTabs);

        if (multiTabs)
        {
            for (auto tab : std::as_const(tabs))
            {
                tab->visible = true;
                tab->setTextRectWidth(showFullTitle ? tab->fullTextWidth : minTextRectWidth);
            }
            computeTabsGeometry();
        }
        else
        {
            for (int i = 0; i < tabs.size(); ++i)
            {
                tabs[i]->visible = false;
            }

            if (isValidIndex(currentIndex))
            {
                auto currTab = tabs[currentIndex];
                currTab->visible = true;
                currTab->setTextRectWidth(showFullTitle ? currTab->fullTextWidth : minTextRectWidth);
            }

            computeTabsGeometry();
            break;
        }

        if (freeSpace < 0)
        {
            if (useMRU)
            {
                for (int i = 0; freeSpace < 0 && i + 1 < recentIndexes.size(); ++i)
                {
                    auto idx = recentIndexes[i];
                    if (isValidIndex(idx))
                    {
                        tabs[idx]->visible = false;
                        computeTabsGeometry();
                    }
                }
            }
            else
            {
                for (int i = 0; freeSpace < 0 && i < tabs.size() - 1; ++i)
                {
                    // The bar must keep showing the page it displays: hiding the
                    // current tab would leave the folder with a view whose tab is
                    // only reachable through the tab list. The loop below and the
                    // MRU branch above both leave the current tab alone.
                    if (i == currentIndex)
                    {
                        continue;
                    }

                    tabs[i]->visible = false;
                    computeTabsGeometry();
                }
            }
        }

        if (freeSpace > 0)
        {
            if (useMRU)
            {
                for (int i = recentIndexes.size() - 2; freeSpace > 0 && i >= 0; --i)
                {
                    auto idx = recentIndexes[i];
                    if (isValidIndex(idx) && !tabs[idx]->visible)
                    {
                        tabs[idx]->visible = true; // try to make the tab visible
                        computeTabsGeometry();
                    }
                }
            }
            else
            {
                for (int i = tabs.size() - 1; freeSpace > 0 && i >= 0; --i)
                {
                    if (!tabs[i]->visible)
                    {
                        tabs[i]->visible = true; // try to make the tab visible
                        computeTabsGeometry();
                    }
                }
            }

            // all tabs are visible and there is free space to make the tabs display more text.
            if (freeSpace > 0 && tabs.size() == visibleCount() && !showFullTitle)
            {
                int shrunkTabs = 0;
                for (auto tab : std::as_const(tabs))
                {
                    shrunkTabs += tab->fullTextWidth > tab->textWidth;
                }

                if (shrunkTabs > 0)
                {
                    int delta = freeSpace / shrunkTabs;
                    if (delta > 0)
                    {
                        // show more chars for tabs
                        for (auto tab : std::as_const(tabs))
                        {
                            tab->setTextRectWidth(tab->textWidth + delta);
                        }
                        computeTabsGeometry();
                    }
                }

                break;
            }
        }

        // all tabs are visible and the tabs should be shrunk.
        if (freeSpace < 0 && tabs.size() == visibleCount() && !showFullTitle)
        {
            int expandedTabs = 0;
            for (auto tab : std::as_const(tabs))
            {
                expandedTabs += tab->textWidth > minTextRectWidth;
            }

            int delta = expandedTabs > 0 ? (freeSpace / expandedTabs - 1) : 0;
            if (delta < 0)
            {
                // Make tabs display fewer characters
                for (auto tab : std::as_const(tabs))
                {
                    if (tab->textWidth > minTextRectWidth)
                    {
                        auto w = tab->textWidth + delta;
                        tab->setTextRectWidth(w < minTextRectWidth ? minTextRectWidth : w);
                    }
                }
                computeTabsGeometry();
            }
        }

        if (freeSpace < 0)
        {
            // Hide the earliest accessed or the earliest added tabs one by one
            // until there is free space (freespace >= 0).
            if (useMRU)
            {
                for (int i = 0; freeSpace < 0 && i < recentIndexes.size(); ++i)
                {
                    auto idx = recentIndexes[i];
                    if (isValidIndex(idx) && tabs[idx]->visible && idx != currentIndex)
                    {
                        tabs[idx]->visible = false;
                        computeTabsGeometry();
                    }
                }
            }
            else
            {
                for (int i = 0; freeSpace < 0 && i < tabs.size(); ++i)
                {
                    if (tabs[i]->visible && i != currentIndex)
                    {
                        tabs[i]->visible = false;
                        computeTabsGeometry();
                    }
                }
            }
        }
    } while (false);

    if (visibleCount() == 1 && freeSpace < 0 && isValidIndex(currentIndex))
    {
        auto tab = tabs[currentIndex];
        tab->setTextRectWidth(tab->textWidth + freeSpace - 1);
        computeTabsGeometry();
    }

    --tabListSettling;
    if (tabListSettling == 0)
    {
        tabListButton->setVisible(tabs.size() > visibleCount());
    }

    owner->update(tabBarContentsRect);
}

void QcwTabFolderPrivate::showWidget(int index)
{
    QWidget* widget = nullptr;
    if (isValidIndex(index))
    {
        widget = tabs[index]->widget;
    }

    if (widget && widget->parentWidget() != owner)
    {
        widget->setParent(owner);
    }

    QWidget* targetWidget = widget ? widget : placeholder;
    if (current == targetWidget && current->isVisible())
    {
        return;
    }

    if (current && current != targetWidget)
    {
        current->hide();
    }

    current = targetWidget;
    current->setFocus();

    computeWidgetGeometry();

    current->show();
}

int QcwTabFolderPrivate::visibleCount() const
{
    int n = 0;
    for (auto tab : tabs)
    {
        n += tab->visible;
    }
    return n;
}

// -----------------------------------------------------------------------------

QcwTabFolderSettingsDialog::QcwTabFolderSettingsDialog(QcwTabFolder* theFolder, QWidget* parent) :
    QDialog { parent }, folder { theFolder }
{
    features = folder->features();
    iconSize = folder->iconSize();
    minTextWidth = folder->minimumTextRectWidth();

    auto verticalLayout = new QVBoxLayout(this);
    verticalLayout->setSpacing(9);

    groupBoxFeatures = new QGroupBox(this);
    auto gridLayout = new QGridLayout(groupBoxFeatures);

    auto checkBox = new QCheckBox(groupBoxFeatures);
    addCheckBox(checkBox, qcw::MultiTabs);
    gridLayout->addWidget(checkBox, 0, 0, 1, 1);

    checkBox = new QCheckBox(groupBoxFeatures);
    addCheckBox(checkBox, qcw::TabsMovable);
    gridLayout->addWidget(checkBox, 0, 1, 1, 1);

    checkBox = new QCheckBox(groupBoxFeatures);
    addCheckBox(checkBox, qcw::AcceptDroppedTabs);
    gridLayout->addWidget(checkBox, 1, 0, 1, 1);

    checkBox = new QCheckBox(groupBoxFeatures);
    addCheckBox(checkBox, qcw::ShowFullTitle);
    gridLayout->addWidget(checkBox, 1, 1, 1, 1);

    checkBox = new QCheckBox(groupBoxFeatures);
    addCheckBox(checkBox, qcw::UseMRU);
    gridLayout->addWidget(checkBox, 2, 0, 1, 1);

    checkBox = new QCheckBox(groupBoxFeatures);
    addCheckBox(checkBox, qcw::ShowIcon);
    gridLayout->addWidget(checkBox, 2, 1, 1, 1);

    checkBoxMinimizable = new QCheckBox(groupBoxFeatures);
    addCheckBox(checkBoxMinimizable, qcw::Minimizable);
    gridLayout->addWidget(checkBoxMinimizable, 3, 0, 1, 1);

    checkBox = new QCheckBox(groupBoxFeatures);
    addCheckBox(checkBox, qcw::ShowClose);
    gridLayout->addWidget(checkBox, 3, 1, 1, 1);

    checkBoxMaximizable = new QCheckBox(groupBoxFeatures);
    addCheckBox(checkBoxMaximizable, qcw::Maximizable);
    gridLayout->addWidget(checkBoxMaximizable, 4, 0, 1, 1);

    checkBox = new QCheckBox(groupBoxFeatures);
    addCheckBox(checkBox, qcw::ShowUnselectedClose);
    gridLayout->addWidget(checkBox, 4, 1, 1, 1);

    verticalLayout->addWidget(groupBoxFeatures);

    groupBoxSizes = new QGroupBox(this);
    auto gridLayout2 = new QGridLayout(groupBoxSizes);

    iconSizeLabel = new QLabel(groupBoxSizes);
    gridLayout2->addWidget(iconSizeLabel, 0, 0, 1, 1);

    iconSizeSpinBox = new QSpinBox(groupBoxSizes);
    iconSizeSpinBox->setMinimum(16);
    iconSizeSpinBox->setMaximum(32);
    iconSizeSpinBox->setSingleStep(2);
    gridLayout2->addWidget(iconSizeSpinBox, 0, 1, 1, 1);

    auto horizontalSpacer1 = new QSpacerItem(158, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

    gridLayout2->addItem(horizontalSpacer1, 0, 2, 1, 1);

    textWidthLabel = new QLabel(groupBoxSizes);
    gridLayout2->addWidget(textWidthLabel, 1, 0, 1, 1);

    textWidthSpinBox = new QSpinBox(groupBoxSizes);
    textWidthSpinBox->setMinimum(50);
    textWidthSpinBox->setMaximum(300);
    textWidthSpinBox->setSingleStep(2);
    textWidthSpinBox->setValue(120);
    gridLayout2->addWidget(textWidthSpinBox, 1, 1, 1, 1);

    auto horizontalSpacer2 = new QSpacerItem(158, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

    gridLayout2->addItem(horizontalSpacer2, 1, 2, 1, 1);

    verticalLayout->addWidget(groupBoxSizes);

    auto verticalSpacer = new QSpacerItem(20, 1, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::MinimumExpanding);

    verticalLayout->addItem(verticalSpacer);

    auto buttonBox = new QDialogButtonBox(this);
    buttonBox->setOrientation(Qt::Orientation::Horizontal);
    buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Apply | QDialogButtonBox::StandardButton::Close |
                                  QDialogButtonBox::StandardButton::RestoreDefaults);

    applyButton = buttonBox->button(QDialogButtonBox::StandardButton::Apply);
    applyButton->setAutoDefault(true);
    applyButton->setDefault(true);

    defaultButton = buttonBox->button(QDialogButtonBox::StandardButton::RestoreDefaults);

    verticalLayout->addWidget(buttonBox);

    setupCheckBoxes(features);

    connect(buttonBox, &QDialogButtonBox::accepted, this, qOverload<>(&QDialog::accept));
    connect(buttonBox, &QDialogButtonBox::rejected, this, qOverload<>(&QDialog::reject));

    connect(iconSizeSpinBox, &QSpinBox::valueChanged, [this](int value)
    {
        iconSize = value;
        applyButton->setEnabled(features != folder->features() || iconSize != folder->iconSize() ||
                                minTextWidth != folder->minimumTextRectWidth());
    });

    connect(textWidthSpinBox, &QSpinBox::valueChanged, [this](int value)
    {
        minTextWidth = value;
        applyButton->setEnabled(features != folder->features() || iconSize != folder->iconSize() ||
                                minTextWidth != folder->minimumTextRectWidth());
    });

    connect(applyButton, &QPushButton::clicked, [this](bool)
    {
        for (const auto& [checkBox, feature] : checkBoxMap.asKeyValueRange())
        {
            features.setFlag(feature, checkBox->checkState() == Qt::Checked);
        }

        folder->setFeatures(features);
        folder->setIconSize(iconSize);
        folder->setMinimumTextRectWidth(minTextWidth);
    });

    connect(defaultButton, &QPushButton::clicked, [this](bool)
    {
        setupCheckBoxes(qcw::DefaultTabFeatures);

        iconSize = 16;
        iconSizeSpinBox->setValue(iconSize);

        minTextWidth = 120;
        textWidthSpinBox->setValue(minTextWidth);
    });

    retranslateUi();
    resize(480, 400);
}

QString QcwTabFolderSettingsDialog::featureLabel(qcw::FolderFeature feature)
{
    switch (feature)
    {
    case qcw::MultiTabs:
        return tr("Multiple tabs");
    case qcw::TabsMovable:
        return tr("Tabs movable");
    case qcw::AcceptDroppedTabs:
        return tr("Accept dropped tabs");
    case qcw::ShowFullTitle:
        return tr("Show full title");
    case qcw::UseMRU:
        return tr("Enable MRU");
    case qcw::ShowIcon:
        return tr("Show icon");
    case qcw::Minimizable:
        return tr("Minimizable");
    case qcw::ShowClose:
        return tr("Show close button");
    case qcw::Maximizable:
        return tr("Maximizable");
    case qcw::ShowUnselectedClose:
        return tr("Show unselected close button");
    }

    return QString {};
}

QString QcwTabFolderSettingsDialog::featureDescription(qcw::FolderFeature feature)
{
    switch (feature)
    {
    case qcw::MultiTabs:
        return tr("Allows the folder to display multiple tabs simultaneously");
    case qcw::TabsMovable:
        return tr("Allows users to manually reorder tabs by dragging");
    case qcw::AcceptDroppedTabs:
        return tr("Allows tabs to be dropped into this folder from other folders");
    case qcw::ShowFullTitle:
        return tr("Displays the full title text of the tab");
    case qcw::UseMRU:
        return tr("Enables Most Recently Used (MRU) ordering for tab switching");
    case qcw::ShowIcon:
        return tr("Displays the icon associated with the tab");
    case qcw::Minimizable:
        return tr("Enables the minimize button/functionality for the folder");
    case qcw::ShowClose:
        return tr(
            "Displays the close button on the active/selected tab (not affect whether the tab can be closed)");
    case qcw::Maximizable:
        return tr("Enables the maximize button/functionality for the folder");
    case qcw::ShowUnselectedClose:
        return tr("Displays the close button when hovering on an unselected tab (not affect whether the tab can be "
                   "closed)");
    }

    return QString {};
}

void QcwTabFolderSettingsDialog::retranslateUi()
{
    groupBoxFeatures->setTitle(tr("Features"));
    groupBoxSizes->setTitle(tr("Sizes"));

    for (const auto& [checkBox, feature] : checkBoxMap.asKeyValueRange())
    {
        checkBox->setText(featureLabel(feature));
        checkBox->setToolTip(featureDescription(feature));
    }

    iconSizeLabel->setText(tr("Tab icon size:"));
    textWidthLabel->setText(tr("Tab text minimum width:"));

    // The suffix belongs to the number, so it is a string of its own rather than part of the
    // label; both spin boxes measure the same unit and share it.
    iconSizeSpinBox->setSuffix(tr(" px"));
    textWidthSpinBox->setSuffix(tr(" px"));

    setWindowTitle(tr("Tab Folder Settings"));
}

void QcwTabFolderSettingsDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }

    QDialog::changeEvent(event);
}

void QcwTabFolderSettingsDialog::addCheckBox(QCheckBox* checkBox, qcw::FolderFeature feature)
{
    checkBoxMap.insert(checkBox, feature);

    connect(checkBox, &QCheckBox::checkStateChanged, [this, feature](Qt::CheckState state)
    {
        features.setFlag(feature, state == Qt::Checked);

        applyButton->setEnabled(features != folder->features() || iconSize != folder->iconSize() ||
                                minTextWidth != folder->minimumTextRectWidth());
    });
}

void QcwTabFolderSettingsDialog::setupCheckBoxes(qcw::FolderFeatures theFeatures)
{
    for (const auto& [checkBox, feature] : checkBoxMap.asKeyValueRange())
    {
        checkBox->setCheckState(theFeatures.testFlag(feature) ? Qt::Checked : Qt::Unchecked);
    }

    if (folder->isFloating())
    {
        checkBoxMaximizable->setEnabled(false);
        checkBoxMinimizable->setEnabled(false);
    }
}

QT_END_NAMESPACE
