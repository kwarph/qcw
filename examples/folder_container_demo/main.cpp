// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "MainWindow.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app { argc, argv };

    // The folders and the panes carry their own borders, so the splitters between them stay
    // invisible apart from a 4px grab area.
    app.setStyleSheet("QcwTabFolder { border: 1px solid lightgray; }"
                      "QPlainTextEdit { border: none; }"
                      "QSplitter::handle { border: none; }"
                      "QSplitter::handle:horizontal { width: 4px; }"
                      "QSplitter::handle:vertical { height: 4px; }");

    MainWindow window;
    window.show();

    return app.exec();
}
