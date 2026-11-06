// Drives the folder feature matrix that shapes the tab bar:
// - MultiTabs off: single-tab mode, only the current tab is shown and the tab
//   list button reaches the others.
// - UseMRU on/off: which tabs give way when the bar is too narrow (least
//   recently used vs earliest added), and that the current tab always stays.
// - ShowFullTitle on/off: full titles hide tabs at a width where elided titles
//   still fit.

#include "QcwDockingTypes.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderRenderer.h"

#include <QApplication>
#include <QImage>
#include <QStringList>
#include <QToolButton>
#include <QWidget>
#include <cstdio>
#include <memory>

namespace
{

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int visibleTabs(QcwTabFolder& folder)
{
    int n = 0;
    for (int i = 0; i < folder.count(); ++i)
    {
        n += folder.isTabVisible(i);
    }
    return n;
}

QString visibleList(QcwTabFolder& folder)
{
    QStringList indexes;
    for (int i = 0; i < folder.count(); ++i)
    {
        if (folder.isTabVisible(i))
        {
            indexes << QString::number(i);
        }
    }
    return indexes.join(',');
}

QToolButton* tabListButton(QcwTabFolder& folder)
{
    return folder.findChild<QToolButton*>("tabListButton");
}

QImage buttonImage(QToolButton* button)
{
    QImage image(button->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    button->render(&image);
    return image;
}

/// The area the button painted in the palette's text color; the white fill stands for its face.
QRect inkedBounds(const QImage& image)
{
    QRect bounds;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (image.pixelColor(x, y).lightness() < 200)
            {
                bounds |= QRect(x, y, 1, 1);
            }
        }
    }

    return bounds;
}

/// Pixels the button painted in the palette's text color; the white fill stands for its face.
int inkedPixels(const QImage& image)
{
    int n = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (image.pixelColor(x, y).lightness() < 200)
            {
                ++n;
            }
        }
    }

    return n;
}

/// The width hint measures the count by its advance while the painter lays it out the same
/// way, so no painted pixel may reach the button's edge.
void checkInkStaysInside(const QImage& image, const char* what)
{
    const QRect ink = inkedBounds(image);
    std::fprintf(stderr, "%s: ink=(%d,%d %dx%d) in %dx%d\n", what, ink.x(), ink.y(), ink.width(),
                 ink.height(), image.width(), image.height());
    check(!ink.isNull(), "the button paints a chevron and a count");
    check(ink.left() >= 1, "the painted count leaves a left margin");
    check(ink.top() >= 1, "the painted count leaves a top margin");
    check(ink.right() < image.width() - 1, "the painted count leaves a right margin");
    check(ink.bottom() < image.height() - 1, "the painted count leaves a bottom margin");
}

/// Narrows the folder in steps until @p target tabs remain visible.
int shrinkTo(QcwTabFolder& folder, int target)
{
    for (int width = folder.width(); width >= 80; width -= 10)
    {
        folder.resize(width, 200);
        QApplication::processEvents();
        if (visibleTabs(folder) <= target)
        {
            return width;
        }
    }
    return -1;
}

void testSingleTabMode()
{
    std::fprintf(stderr, "--- single tab mode ---\n");

    QcwTabFolder folder;
    folder.resize(700, 200);
    folder.setFeature(qcw::MultiTabs, false);
    folder.addTab(new QWidget, "Alpha");
    folder.addTab(new QWidget, "Bravo");
    folder.addTab(new QWidget, "Charlie");
    folder.show();
    QApplication::processEvents();

    check(folder.currentIndex() == 2, "adding a tab makes it current in single-tab mode");
    check(folder.isTabVisible(2) && !folder.isTabVisible(0) && !folder.isTabVisible(1),
          "single-tab mode shows only the current tab");

    auto* listButton = tabListButton(folder);
    check(listButton != nullptr, "the folder has a tab list button");
    check(listButton && listButton->isVisible(), "the tab list button reaches the hidden tabs");

    folder.setCurrentIndex(0);
    QApplication::processEvents();
    check(folder.isTabVisible(0) && !folder.isTabVisible(1) && !folder.isTabVisible(2),
          "switching in single-tab mode moves the single visible tab");
    check(folder.currentWidget() != nullptr && folder.currentWidget()->isVisible(),
          "the current pane is the one on screen");

    folder.setFeature(qcw::MultiTabs, true);
    QApplication::processEvents();
    check(visibleTabs(folder) == 3, "re-enabling multiple tabs shows them again");
    check(listButton && !listButton->isVisible(), "no tab list button is needed once every tab fits");
}

void testMruChoosesHiddenTab()
{
    std::fprintf(stderr, "--- mru overflow ---\n");

    auto makeFolder = [](bool useMRU)
    {
        auto* folder = new QcwTabFolder;
        folder->resize(1000, 200);
        folder->setFeature(qcw::UseMRU, useMRU);
        folder->addTab(new QWidget, "A very long tab title that takes quite a lot of room");
        folder->addTab(new QWidget, "Another very long tab title that takes quite a lot of room");
        folder->addTab(new QWidget, "A third very long tab title that takes quite a lot of room");
        // Visit every tab so the MRU order differs from the tab order: 2 is the
        // least recently used, while 0 is the earliest added.
        folder->setCurrentIndex(2);
        folder->setCurrentIndex(0);
        folder->setCurrentIndex(1);
        folder->show();
        QApplication::processEvents();
        return folder;
    };

    std::unique_ptr<QcwTabFolder> mruOwner { makeFolder(true) };
    std::unique_ptr<QcwTabFolder> plainOwner { makeFolder(false) };
    QcwTabFolder& mru = *mruOwner;
    QcwTabFolder& plain = *plainOwner;
    mru.resize(1000, 200);
    plain.resize(1000, 200);
    QApplication::processEvents();

    const int mruWidth = shrinkTo(mru, 2);
    const int plainWidth = shrinkTo(plain, 2);
    std::fprintf(stderr, "mru width=%d visible=%s plain width=%d visible=%s\n", mruWidth,
                 qUtf8Printable(visibleList(mru)), plainWidth, qUtf8Printable(visibleList(plain)));

    check(mruWidth > 0 && plainWidth > 0, "the overflow scene fits two tabs");
    check(visibleTabs(mru) == 2 && visibleTabs(plain) == 2, "exactly two tabs stay visible");
    check(mru.isTabVisible(0) && mru.isTabVisible(1) && !mru.isTabVisible(2),
          "with MRU the least recently used tab is hidden first");
    check(plain.isTabVisible(1) && plain.isTabVisible(2) && !plain.isTabVisible(0),
          "without MRU the earliest added non-current tab is hidden first");
    check(mru.isTabVisible(mru.currentIndex()) && plain.isTabVisible(plain.currentIndex()),
          "the current tab stays visible once the bar overflows");
}

void testCurrentTabSurvivesExtremeNarrowing()
{
    std::fprintf(stderr, "--- current tab while narrow, no MRU ---\n");

    QcwTabFolder folder;
    folder.resize(1000, 200);
    folder.setFeature(qcw::UseMRU, false);
    folder.addTab(new QWidget, "First long tab title");
    folder.addTab(new QWidget, "Second long tab title");
    folder.addTab(new QWidget, "Third long tab title");
    folder.addTab(new QWidget, "Fourth long tab title");
    folder.setCurrentIndex(0);
    folder.show();
    QApplication::processEvents();

    const int width = shrinkTo(folder, 1);
    std::fprintf(stderr, "narrow width=%d current=%d visible=%s\n", width, folder.currentIndex(),
                 qUtf8Printable(visibleList(folder)));

    check(width > 0, "the bar narrows down to a single tab");
    check(visibleTabs(folder) == 1, "one tab remains visible");
    check(folder.isTabVisible(folder.currentIndex()), "the current tab is the one that remains visible");
}

void testShowFullTitle()
{
    std::fprintf(stderr, "--- show full title ---\n");

    auto addTabs = [](QcwTabFolder& folder)
    {
        folder.addTab(new QWidget, "A long tab title that is only shown in full when there is room for it");
        folder.addTab(new QWidget, "Another long tab title that is only shown in full when there is room");
        folder.addTab(new QWidget, "A third long tab title that is only shown in full when there is room");
    };

    QcwTabFolder full;
    full.resize(1600, 200);
    full.setFeature(qcw::ShowFullTitle, true);
    addTabs(full);
    full.show();
    QApplication::processEvents();

    const int width = shrinkTo(full, 2);
    check(width > 0, "the full titles stop fitting when the bar narrows");
    check(visibleTabs(full) < 3, "a full-title folder hides tabs that do not fit");

    QcwTabFolder elided;
    elided.resize(width, 200);
    elided.setFeature(qcw::ShowFullTitle, false);
    addTabs(elided);
    elided.show();
    QApplication::processEvents();

    std::fprintf(stderr, "width=%d full=%s elided=%s\n", width, qUtf8Printable(visibleList(full)),
                 qUtf8Printable(visibleList(elided)));
    check(visibleTabs(elided) == 3, "elided titles still fit at the width where full titles do not");

    elided.setFeature(qcw::ShowFullTitle, true);
    QApplication::processEvents();
    check(visibleTabs(elided) < 3, "turning full titles on hides the tabs that no longer fit");
}

// The tab list button draws a chevron with the number of hidden tabs, Eclipse style. Single-tab
// mode keeps exactly one tab visible, so the count is `count() - 1` and each step is predictable.
void testTabListButtonCount()
{
    std::fprintf(stderr, "--- tab list button count ---\n");

    QcwTabFolder folder;
    folder.resize(700, 200);
    folder.setFeature(qcw::MultiTabs, false);
    folder.addTab(new QWidget, "Tab");
    folder.show();
    QApplication::processEvents();

    auto* button = tabListButton(folder);
    check(button != nullptr && !button->isVisible(), "no tab list button while every tab fits");

    folder.addTab(new QWidget, "Tab");
    QApplication::processEvents();
    const QImage oneHidden = buttonImage(button);
    const int oneHiddenWidth = button->width();
    std::fprintf(stderr, "one hidden: button=%dx%d ink=%d\n", button->width(), button->height(),
                 inkedPixels(oneHidden));

    check(button->isVisible(), "the button appears once a tab is hidden");
    check(button->height() == QcwTabFolderRenderer::ButtonSize, "the button keeps the folder button height");
    check(button->width() == button->sizeHint().width(), "the layout gives the button the width its hint asks for");
    check(button->width() >= QcwTabFolderRenderer::ButtonSize, "the button is at least as wide as it is tall");
    check(inkedPixels(oneHidden) > 8, "the button paints a chevron and the hidden count");
    checkInkStaysInside(oneHidden, "one hidden");

    while (folder.count() < 9)
    {
        folder.addTab(new QWidget, "Tab");
    }
    QApplication::processEvents();
    const QImage eightHidden = buttonImage(button);
    check(eightHidden.size() == oneHidden.size(), "single-digit counts share one button width");
    check(eightHidden != oneHidden, "the painted count follows the number of hidden tabs");
    checkInkStaysInside(eightHidden, "eight hidden");

    while (folder.count() < 11) // 10 hidden tabs
    {
        folder.addTab(new QWidget, "Tab");
    }
    QApplication::processEvents();
    const QImage tenHidden = buttonImage(button);
    std::fprintf(stderr, "ten hidden: width=%d\n", button->width());
    check(button->width() > oneHiddenWidth, "a two-digit count widens the button");
    check(button->width() == button->sizeHint().width(), "the widened button still fills its hint");
    checkInkStaysInside(tenHidden, "ten hidden");

    // Past 99 the button shows "99+" and must stop growing with the count.
    while (folder.count() < 101)
    {
        folder.addTab(new QWidget, "Tab");
    }
    QApplication::processEvents();
    const QImage hundredHidden = buttonImage(button);
    checkInkStaysInside(hundredHidden, "hundred hidden");
    folder.addTab(new QWidget, "Tab");
    QApplication::processEvents();
    check(button->width() == hundredHidden.width(), "the count caps the button width at 99+");
    check(buttonImage(button) == hundredHidden, "counts past 99 paint the same \"99+\"");

    folder.resize(folder.minimumSizeHint().width(), 200);
    QApplication::processEvents();
    check(button->isVisible(), "the button still shows at the folder's minimum width");
    check(button->width() == button->sizeHint().width(), "the widest count keeps its width in the minimum bar");
    check(button->geometry().right() <= folder.contentsRect().right(),
          "the widest count stays inside the tab bar");
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    testSingleTabMode();
    testMruChoosesHiddenTab();
    testCurrentTabSurvivesExtremeNarrowing();
    testShowFullTitle();
    testTabListButtonCount();

    if (failures == 0)
    {
        std::printf("tab_folder_features_test: all checks passed\n");
    }
    else
    {
        std::fprintf(stderr, "tab_folder_features_test: %d failure(s)\n", failures);
    }

    return failures == 0 ? 0 : 1;
}
