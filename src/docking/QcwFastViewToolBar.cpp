// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwFastViewToolBar.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderSite.h"
#include "QcwTabFolderRenderer.h"
#include "QcwSessionData.h"

#include <QEvent>
#include <QLayout>

QT_BEGIN_NAMESPACE

namespace
{

/// Width of the window-colored margin the popup keeps along each side of the folder.
constexpr int FastViewMargin { 6 };

} // namespace

class QcwFastViewToolBarPrivate
{
public:
    explicit QcwFastViewToolBarPrivate(QcwFastViewToolBar* interface);

    void createActions();
    void returnFolder();
    void discardToolBar();
    void showFastView(int index);
    QPair<bool, QMargins> popupMargins() const;

public:
    QcwFastViewToolBar* owner;
    QList<QMetaObject::Connection> connections;
    QcwTabFolder* folder;
    QWidget* anchorWidget;
    QcwTabFolderSite* originalSite;
    QcwTabFolderSite* fastViewSite;
    QSize fastViewSize;
    qcw::FolderFeatures folderFeatures;
    bool listAllTabs { true };
};

// -----------------------------------------------------------------------------

QcwFastViewToolBar::QcwFastViewToolBar(
    QcwTabFolder* folder, QWidget* anchorWidget, const QSize& fastViewSize, bool listAllTabs, QWidget* parent) :
    QToolBar { parent }, impl { new QcwFastViewToolBarPrivate { this } }
{
    impl->folder = folder;
    impl->anchorWidget = anchorWidget;
    impl->fastViewSize = fastViewSize;
    impl->listAllTabs = listAllTabs;

    // Both are read before the popup site is built, because installing the folder there reparents
    // it out of the site it came from, and disabling Minimizable changes features().
    impl->originalSite = qobject_cast<QcwTabFolderSite*>(folder->parentWidget());
    impl->folderFeatures = folder->features();

    impl->fastViewSite = new QcwTabFolderSite(folder, this);
    impl->fastViewSite->setObjectName("fastViewSite"); // object name for QSS
    // The site's margins depend on the edge the toolbar is docked to, so showFastView() sets them.
    impl->fastViewSite->setWindowFlag(Qt::Popup);

    // Minimizing the folder from inside its own fast view would park it in a second one.
    folder->setFeature(qcw::Minimizable, false);

    setIconSize(QSize(16, 16));
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setAllowedAreas(Qt::LeftToolBarArea | Qt::RightToolBarArea);

    auto conn = connect(impl->folder, &QcwTabFolder::tabsChanged, [this]()
    {
        impl->createActions();
    });
    impl->connections.append(conn);

    // Either the layout took the folder back or the folder is going away; both mean it must not
    // stay parented to this toolbar, which would take the folder down with it when it is deleted.
    conn = connect(impl->folder, &QcwTabFolder::aboutToRestore, [this]()
    {
        impl->discardToolBar();
    });
    impl->connections.append(conn);

    conn = connect(impl->folder, &QcwTabFolder::aboutToDelete, [this]()
    {
        impl->discardToolBar();
    });
    impl->connections.append(conn);

    impl->createActions();

#ifdef QCW_DEBUG
    qDebug() << this << "created";
#endif
}

QcwFastViewToolBar::~QcwFastViewToolBar()
{
    for (auto& conn : impl->connections)
    {
        disconnect(conn);
    }
    impl->connections.clear();

    delete impl;

#ifdef QCW_DEBUG
    qDebug() << this << "destroyed";
#endif
}

QcwTabFolder* QcwFastViewToolBar::folder() const
{
    return impl->folder;
}

bool QcwFastViewToolBar::parksFolder(const QcwTabFolder* folder) const
{
    return folder && impl->folder == folder && folder->parentWidget() == impl->fastViewSite;
}

bool QcwFastViewToolBar::releaseFolder()
{
    if (!parksFolder(impl->folder))
    {
        return false;
    }

    impl->returnFolder();
    return true;
}

void QcwFastViewToolBar::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        impl->createActions();
    }

    QToolBar::changeEvent(event);
}

// -----------------------------------------------------------------------------

QcwFastViewToolBarPrivate::QcwFastViewToolBarPrivate(QcwFastViewToolBar* interface) :
    owner { interface }
{
}

void QcwFastViewToolBarPrivate::createActions()
{
    // QToolBar::clear() only detaches the actions, it does not delete them, and they are children
    // of the toolbar here, so without this every tab change would leave a whole set of them behind.
    auto acts = owner->actions();
    for (auto action : std::as_const(acts))
    {
        action->deleteLater();
    }
    owner->clear();

    auto action = new QAction(owner);
    action->setToolTip(QcwFastViewToolBar::tr("Restore"));
    action->setIcon(folder->renderer()->renderMaximizeIcon(qcw::FolderState::Minimized));

    QObject::connect(action, &QAction::triggered, [this](bool)
    {
        // The folder has to sit in originalSite before the request goes out, because the layout
        // restores whatever site folder->parentWidget() reports. aboutToRestore then discards this
        // toolbar, so the two calls together complete the restore.
        returnFolder();
        emit folder->restoreRequested();
    });

    owner->addAction(action);

    if (folder->count() == 0 || !listAllTabs)
    {
        auto text = QcwFastViewToolBar::tr("Show Folder");
        action = new QAction(text, owner);
        action->setToolTip(text);
        action->setIcon(folder->renderer()->renderViewIcon());

        QObject::connect(action, &QAction::triggered, [this](bool)
        {
            showFastView(-1);
        });

        owner->addAction(action);
    }
    else
    {
        for (int index = 0; index < folder->count(); ++index)
        {
            auto text = folder->tabText(index);
            action = new QAction(text, owner);
            action->setToolTip(text);
            auto icon = folder->tabIcon(index);
            if (icon.isNull())
            {
                icon = folder->renderer()->renderViewIcon();
            }
            action->setIcon(icon);

            QObject::connect(action, &QAction::triggered, [this, index](bool)
            {
                showFastView(index);
            });

            owner->addAction(action);
        }
    }
}

void QcwFastViewToolBarPrivate::returnFolder()
{
    fastViewSite->hide();

    // Taking the folder out of the popup leaves it parentless, so it goes straight back to the site
    // it was minimized from, together with the features this toolbar changed while it was parked.
    fastViewSite->setFolder(nullptr);

    folder->setFeatures(folderFeatures);
    if (originalSite)
    {
        originalSite->setFolder(folder);
    }
}

void QcwFastViewToolBarPrivate::discardToolBar()
{
    returnFolder();
    owner->hide();
    owner->deleteLater();
}

QPair<bool, QMargins> QcwFastViewToolBarPrivate::popupMargins() const
{
    // The popup grows towards the inside of the window: the folder keeps the anchor's top-left
    // corner, and the margin goes on the side that lies over the window and off the side that
    // meets the window's own margin - on the right for a toolbar docked on the right, on the
    // left for one docked on the left. Comparing the two global positions tells the edge apart.
    const bool dockedOnRight = anchorWidget->mapToGlobal(anchorWidget->contentsRect().topLeft()).x() <
                               owner->mapToGlobal(owner->rect().topLeft()).x();

    int fastViewMargin = sessionData->value("fastViewMargin", -1);
    if (fastViewMargin == -1)
    {
        fastViewMargin = FastViewMargin;
    }

    return { dockedOnRight, dockedOnRight ? QMargins { fastViewMargin, 0, 0, fastViewMargin } :
                                            QMargins { 0, 0, fastViewMargin, fastViewMargin } };
}

void QcwFastViewToolBarPrivate::showFastView(int index)
{
    const auto marginsInfo = popupMargins();
    const bool hasBorder = sessionData->value("fastViewHasBorder", false);

    // setFrameShape() makes QFrame write its own width into the widget's contents margins, and does
    // so from the stylesheet the widget holds - which on a first show has not been polished yet, so
    // it measures no frame and the margins end up at zero while the frame is still painted. The
    // margins are therefore set explicitly instead, frame width included.
    fastViewSite->setFrameShape(hasBorder ? QFrame::StyledPanel : QFrame::NoFrame);
    const int frameWidth = fastViewSite->frameWidth();
    fastViewSite->setContentsMargins(marginsInfo.second + QMargins(frameWidth, frameWidth, frameWidth, frameWidth));

    // A layout activates only after invalidate(), so without this the one below hands out the
    // margins it cached on an earlier opening and the folder lands on the popup's corner.
    fastViewSite->layout()->invalidate();

    // The folder keeps fastViewSize inside the popup; the frame and the margins widen the popup
    // around it. A stored size narrower than the folder's minimum gives way to that minimum, as it
    // did before the margins existed - but only while the folder has not been laid out yet: once
    // it has, the layout holds the folder at the size it was given and its minimum no longer says
    // anything about fastViewSize.
    const bool laidOut = fastViewSite->layout()->geometry().isValid();
    QSize popup = fastViewSize.grownBy(marginsInfo.second);
    if (!laidOut)
    {
        popup = popup.expandedTo(fastViewSite->minimumSizeHint());
    }
    popup += QSize { 2 * frameWidth, 2 * frameWidth };
    fastViewSite->resize(popup);

    // The layout is activated here rather than left to the folder's show: a folder coming back from
    // its original site is still marked hidden, so the site's show never runs the folder's show
    // path, and the layout keeps the rect it had before, which no longer carries the new margins
    // or frame. Left stale, the folder spills over them.
    fastViewSite->layout()->activate();

    // The folder opens on the anchor's top-left corner and popupMargins() leaves that corner free,
    // so the folder lines up with the folders around it instead of sitting below the menu bar.
    auto globalPos = anchorWidget->mapToGlobal(anchorWidget->contentsRect().topLeft());

    // Docked on the right, the popup hangs off the anchor's right edge instead, so its left
    // margin stays inside the anchor's area; docked on the left, it hangs off the left edge.
    if (marginsInfo.first)
    {
        globalPos.setX(globalPos.x() + anchorWidget->width() - popup.width() - 1 + hasBorder);
    }
    else
    {
        globalPos.setX(globalPos.x() - 1 - hasBorder);
    }

    globalPos.setY(globalPos.y() - hasBorder);

    fastViewSite->move(globalPos);

    // setState() only touches the folder's own maximize button when the value changes, so this
    // round trip is what makes that button be painted again. Inside a fast view it is the only
    // button left on the folder - Minimizable is switched off here - and it carries the restore
    // glyph, which may have been rendered under another theme while the folder sat minimized.
    auto state = folder->state();
    folder->setState(qcw::FolderState::Normal);
    folder->setState(state);
    fastViewSite->show();
    fastViewSite->layout()->activate();

    // The single-icon mode has no tab to pick and passes -1 for "just show the folder". Passing it
    // on would be wrong: setCurrentIndex(-1) means "show no page" and would deselect the open one.
    if (index >= 0)
    {
        folder->setCurrentIndex(index);
    }
}

QT_END_NAMESPACE
