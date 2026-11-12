// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QDialog>

QT_BEGIN_NAMESPACE

class QAbstractButton;
class QDialogButtonBox;
class QHBoxLayout;
class QVBoxLayout;
class QcwNavigationContentWidget;

/**
 * @class QcwNavigationContentDialog
 * @brief Dialog frame around a QcwNavigationContentWidget and a standard button row.
 *
 * @details The dialog has a content area, a separator and a button row. The row holds the
 * dialog's action buttons on the left, a stretch, and a QDialogButtonBox on the right, so
 * dialogs keep the familiar left/right alignment instead of packing every button into the
 * box. A subclass installs its navigation frame with setContentWidget(), adds its own action
 * buttons with addActionButton(), and configures the box through buttonBox(); the base adds no
 * buttons by itself. QcwPreferencesDialog adds Close to the box plus Restore, Export and Import
 * as action buttons.
 *
 * The content widget is inserted above the separator and reparented to the dialog. Only the
 * first call has an effect. The dialog owns it through Qt's parent-child mechanism.
 *
 * @see QcwNavigationContentWidget, QcwPreferencesDialog
 */
class QCW_EXPORT QcwNavigationContentDialog : public QDialog
{
    Q_OBJECT
public:
    /// Builds the empty frame: content area, separator and button row.
    explicit QcwNavigationContentDialog(QWidget* parent = nullptr);

    /**
     * @brief Installs @p contentWidget above the separator.
     * @param contentWidget Navigation frame to show; nullptr is ignored.
     *
     * @details Only the first call has an effect. The widget is reparented to the dialog and
     * takes the stretchable part of the vertical layout.
     */
    void setContentWidget(QcwNavigationContentWidget* contentWidget);

    /**
     * @brief Adds @p button to the left side of the button row.
     * @param button Button to add; nullptr is ignored.
     *
     * @details Buttons appear in the order they are added. The dialog keeps a stretch between
     * these action buttons and the button box, so the row stays left/right aligned.
     */
    void addActionButton(QAbstractButton* button);

    /// The button box at the right end of the button row; add standard buttons to it.
    QDialogButtonBox* buttonBox() const
    {
        return mButtonBox;
    }

private:
    QVBoxLayout* mMainLayout { nullptr };
    QHBoxLayout* mButtonLayout { nullptr };
    QcwNavigationContentWidget* mContentWidget { nullptr };
    QDialogButtonBox* mButtonBox { nullptr };
};

QT_END_NAMESPACE
