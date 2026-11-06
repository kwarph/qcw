// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwView.h"

#include <QVariantMap>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

/**
 * @brief Lists the symbols of the file the current editor's cursor is in.
 *
 * @details The view is the consumer side of the editor-selection topic: subscribeWindowTopic()
 * follows demo::TopicEditorSelection through every attach, move and teardown, so this view holds
 * no window pointer, and a payload without a path leaves the outline empty (the editor clears the
 * topic when it hides). The view asks the editor service for the document text when the revision
 * changes. Clicking a symbol calls IEditorService::revealAt() — a navigation request, not a
 * published selection — so the editor moves and the outline follows, rather than the two chasing
 * each other.
 *
 * The demonstration parses headings, class/namespace declarations and function definitions
 * with a few regular expressions; a real outline would use a language parser. What matters
 * here is the flow: selection describes where the user is, the document service supplies the
 * content, and the session stays local to this view.
 */
class OutlineView : public QcwView
{
    Q_OBJECT
public:
    explicit OutlineView(QWidget* parent = nullptr);
    ~OutlineView() override;

protected:
    void retranslateUi() override;

private:
    void applySelection(const QVariantMap& payload);
    void clearOutline();
    void rebuild(const QString& filePath, const QString& text, int revision);
    void updateHighlight(int position);
    void revealSymbol(QTreeWidgetItem* item);

    QLabel* mTitle;
    QTreeWidget* mTree;
    QString mPath;
    int mRevision { -1 };
};
