// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QSplitter>
#include <QSplitterHandle>

QT_BEGIN_NAMESPACE

class QPaintEvent;

/**
 * @class QcwSplitterHandle
 * @brief The handle of a QcwSplitter: a two-pixel sunken groove between the panes.
 *
 * @details Fusion's CE_Splitter paints nothing for a handle narrower than two pixels, and a
 * splitter whose panes have no frames of their own therefore shows no line at all. This
 * handle paints the same groove a sunken QFrame separator draws — QPalette::Dark on the
 * leading edge and QPalette::Light behind it — so separating the panes only takes the theme
 * palette. A horizontal splitter (panes side by side) gets a vertical groove and a vertical
 * splitter a horizontal one.
 *
 * QcwSplitter creates these handles; instantiate one directly only from a QSplitter
 * subclass that overrides createHandle().
 *
 * @see QcwSplitter
 */
class QCW_EXPORT QcwSplitterHandle : public QSplitterHandle
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the handle.
     * @param orientation The splitter's orientation, which also decides the groove's
     *                    direction; QSplitterHandle treats it the same way.
     * @param parent The splitter owning the handle.
     */
    explicit QcwSplitterHandle(Qt::Orientation orientation, QSplitter* parent);

protected:
    /// Paints the groove inside contentsRect(), the part a narrow handle does not mask off.
    void paintEvent(QPaintEvent* event) override;
};

/**
 * @class QcwSplitter
 * @brief A QSplitter whose handle is a visible sunken groove.
 *
 * @details A plain QSplitter under Fusion draws nothing at a narrow handle width, so two
 * panes without frames — like the tree and the page area of the preferences widget — run
 * together. This splitter creates a QcwSplitterHandle, which paints QPalette::Dark followed
 * by QPalette::Light along the handle, in either orientation. Both QcwLight and QcwDark
 * therefore separate their panes without any stylesheet rule.
 *
 * The handle width defaults to 2, the groove's width; setHandleWidth() still applies, but
 * only the leading two pixels carry the groove. Sizing and dragging behave exactly like
 * QSplitter.
 *
 * @see QcwSplitterHandle
 */
class QCW_EXPORT QcwSplitter : public QSplitter
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a horizontal splitter.
     * @param parent The parent widget.
     */
    explicit QcwSplitter(QWidget* parent = nullptr);

    /**
     * @brief Constructs a splitter with @p orientation.
     * @param orientation Horizontal lays the panes out side by side, Vertical stacks them.
     * @param parent The parent widget.
     */
    explicit QcwSplitter(Qt::Orientation orientation, QWidget* parent = nullptr);

protected:
    /// Returns a QcwSplitterHandle, one for each gap between the panes.
    QSplitterHandle* createHandle() override;
};

QT_END_NAMESPACE
