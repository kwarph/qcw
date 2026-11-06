// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPluginTemplate.h"
#include "QcwViewCreator.h"

/**
 * @brief Creates TextEditor views.
 *
 * The creator builds editors and nothing else: they register themselves with the
 * EditorService it contributes as demo::IEditorService, and that service answers the calls
 * other plugins make.
 */
class TextEditorCreator : public QcwViewCreator
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qcw.QcwViewCreator" FILE "texteditor.json")
    Q_INTERFACES(QcwViewCreator)
    QCW_PLUGIN_INTERFACE

public:
    QString defaultWindowTitle() const override;
    QIcon defaultViewIcon() const override;

protected:
    void doRegisterContributions() override;

private:
    QcwView* doCreate(QWidget* parent, const QList<QVariant>& args) override;
};
