// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwFloatingFolderMonitor.h"

QT_BEGIN_NAMESPACE

// Built on the first use of instance() and the same object ever after.
Q_GLOBAL_STATIC(QcwFloatingFolderMonitor, floatingFolderMonitorInstance);

QcwFloatingFolderMonitor::QcwFloatingFolderMonitor(QObject* parent) :
    QObject { parent }
{
}

QcwFloatingFolderMonitor* QcwFloatingFolderMonitor::instance()
{
    return floatingFolderMonitorInstance;
}

QT_END_NAMESPACE
