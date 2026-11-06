// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "Workbench.h"
#include "MainWindow.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    Workbench workbench;
    if (!workbench.initialize("workbench_demo", "Workbench Demo", "qcw.org", "qcw", "1.0.0",
                              /*quitOnLastWindowClosed=*/true, /*loadPlugins=*/false))
    {
        return 1;
    }

    if (!workbench.start())
    {
        return 1;
    }

    return app.exec();
}
