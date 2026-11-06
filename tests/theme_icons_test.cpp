// Checks that the packaged theme icons exist and that a cached QIcon still re-colours
// itself from the application palette: the same QIcon object has to render dark strokes
// on a light palette and light strokes on a dark one.

#include "QcwThemeIcons.h"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPalette>
#include <QPixmap>
#include <array>
#include <cstdio>

namespace
{

constexpr std::array<qcw::Icon, 8> kIcons {
    qcw::Icon::Perspective,    qcw::Icon::View,          qcw::Icon::NewPerspective, qcw::Icon::NewView,
    qcw::Icon::FolderMaximize, qcw::Icon::FolderRestore, qcw::Icon::FolderMinimize, qcw::Icon::TabList,
};

constexpr std::array<const char*, 8> kFiles {
    ":/qcw/icons/perspective.svg",     ":/qcw/icons/view.svg",
    ":/qcw/icons/new_perspective.svg", ":/qcw/icons/new_view.svg",
    ":/qcw/icons/folder_maximize.svg", ":/qcw/icons/folder_restore.svg",
    ":/qcw/icons/folder_minimize.svg", ":/qcw/icons/tab_list.svg",
};

QPalette
makePalette(const QColor& windowText, const QColor& window, const QColor& disabledText, const QColor& highlightedText)
{
    QPalette palette;
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, windowText);
    palette.setColor(QPalette::Text, windowText);
    palette.setColor(QPalette::ButtonText, windowText);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    palette.setColor(QPalette::HighlightedText, highlightedText);
    return palette;
}

QImage render(qcw::Icon icon, QIcon::Mode mode)
{
    return qcw::themeIcon(icon).pixmap(16, 16, mode).toImage();
}

int opaquePixels(const QImage& image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (qAlpha(image.pixel(x, y)) > 0)
            {
                ++count;
            }
        }
    }
    return count;
}

double averageLuminance(const QImage& image)
{
    double total = 0.0;
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor color = image.pixelColor(x, y);
            if (color.alpha() > 0)
            {
                total += color.lightnessF();
                ++count;
            }
        }
    }

    return count > 0 ? total / count : 0.0;
}

bool isTransparent(const QImage& image, int x, int y)
{
    return qAlpha(image.pixel(x, y)) == 0;
}

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    const QPalette light = makePalette(QColor("#31363b"), QColor("#eff0f1"), QColor("#7f8c8d"), QColor("#ffffff"));
    const QPalette dark = makePalette(QColor("#eff0f1"), QColor("#232629"), QColor("#7f8c8d"), QColor("#ffffff"));

    for (std::size_t i = 0; i < kIcons.size(); ++i)
    {
        check(QFile::exists(QString::fromLatin1(kFiles[i])), kFiles[i]);

        const QImage image = render(kIcons[i], QIcon::Normal);
        check(!image.isNull(), "16x16 pixmap is not null");
        check(opaquePixels(image) > 0, "icon has visible pixels");
        check(isTransparent(image, 0, 0), "top-left corner is transparent");
        check(isTransparent(image, 15, 15), "bottom-right corner is transparent");
    }

    // Still the same cached QIcon objects; only the application palette changes.
    QApplication::setPalette(light);
    const QImage lightPerspective = render(qcw::Icon::Perspective, QIcon::Normal);
    QApplication::setPalette(dark);
    const QImage darkPerspective = render(qcw::Icon::Perspective, QIcon::Normal);

    check(averageLuminance(lightPerspective) < averageLuminance(darkPerspective),
          "light palette draws darker strokes than the dark palette");

    QApplication::setPalette(light);
    const QImage normal = render(qcw::Icon::Perspective, QIcon::Normal);
    const QImage disabled = render(qcw::Icon::Perspective, QIcon::Disabled);
    const QImage selected = render(qcw::Icon::Perspective, QIcon::Selected);

    check(averageLuminance(disabled) > averageLuminance(normal), "disabled mode uses the disabled window text colour");
    check(averageLuminance(selected) > averageLuminance(normal), "selected mode uses the highlighted text colour");

    if (failures == 0)
    {
        std::printf("theme_icons_test: all checks passed\n");
    }

    return failures == 0 ? 0 : 1;
}
