// Checks that QcwSplitter shows its handle as a two-pixel palette groove in both
// orientations: a horizontal splitter gets a vertical, dark-then-light line between the
// panes, a vertical splitter the same line turned sideways, and the pixels next to the
// groove still belong to the panes.

#include "QcwSplitter.h"

#include <QApplication>
#include <QImage>
#include <QPixmap>
#include <cstdio>

namespace
{

const QColor PaneColor { 0, 128, 0 };
const QColor DarkColor { 255, 0, 0 };
const QColor LightColor { 0, 0, 255 };

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

/// A pane that fills itself with PaneColor, so the groove cannot be confused with it.
QWidget* makePane(QWidget* parent)
{
    auto* pane = new QWidget(parent);
    QPalette palette = pane->palette();
    palette.setColor(QPalette::Window, PaneColor);
    pane->setPalette(palette);
    pane->setAutoFillBackground(true);
    return pane;
}

/// Builds a two-pane splitter, paints Dark red and Light blue, and returns its rendering.
QImage renderSplitter(Qt::Orientation orientation, QcwSplitter& splitter)
{
    splitter.resize(200, 120);
    splitter.addWidget(makePane(&splitter));
    splitter.addWidget(makePane(&splitter));

    QPalette palette = splitter.palette();
    palette.setColor(QPalette::Dark, DarkColor);
    palette.setColor(QPalette::Light, LightColor);
    splitter.setPalette(palette);

    splitter.show();
    QApplication::processEvents();

    return splitter.grab().toImage();
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    for (const auto orientation : { Qt::Horizontal, Qt::Vertical })
    {
        const bool horizontal = orientation == Qt::Horizontal;
        std::fprintf(stderr, "--- %s splitter ---\n", horizontal ? "horizontal" : "vertical");

        QcwSplitter splitter { orientation };
        const QImage image = renderSplitter(orientation, splitter);

        auto* handle = splitter.handle(1);
        check(handle != nullptr && qobject_cast<QcwSplitterHandle*>(handle) != nullptr,
              "the splitter creates a QcwSplitterHandle");
        if (!handle)
        {
            continue;
        }

        check(splitter.handleWidth() == 2, "the handle defaults to the groove's two pixels");
        check(handle->isVisible(), "the handle is visible between the panes");

        const QRect contents = handle->contentsRect();
        const int x = horizontal ? handle->x() + contents.left() : splitter.width() / 2;
        const int y = horizontal ? splitter.height() / 2 : handle->y() + contents.top();

        check(horizontal ? contents.width() == 2 : contents.height() == 2,
              "the painted part of the handle is two pixels wide");

        check(image.pixelColor(x, y) == DarkColor, "the groove starts with the Dark role");
        if (horizontal)
        {
            check(image.pixelColor(x + 1, y) == LightColor, "the groove continues with the Light role");
            check(image.pixelColor(x - 1, y) == PaneColor && image.pixelColor(x + 2, y) == PaneColor,
                  "the panes run up to both sides of the vertical groove");
        }
        else
        {
            check(image.pixelColor(x, y + 1) == LightColor, "the groove continues with the Light role");
            check(image.pixelColor(x, y - 1) == PaneColor && image.pixelColor(x, y + 2) == PaneColor,
                  "the panes run up to both sides of the horizontal groove");
        }
    }

    if (failures == 0)
    {
        std::printf("splitter_test: all checks passed\n");
    }

    return failures == 0 ? 0 : 1;
}
