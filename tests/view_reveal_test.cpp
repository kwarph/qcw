// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

// QcwWindow::viewOfType()/revealView(): find a view by type, select its tab, restore a
// minimized folder and switch perspectives, without changing focus.

#include "QcwPerspective.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwWindow.h"

#include <QApplication>
#include <cstdio>

namespace
{
int gFailures = 0;

void check(bool ok, const char* what)
{
    std::fprintf(stderr, "%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
    {
        ++gFailures;
    }
}

void settle()
{
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

class TestWindow : public QcwWindow
{
public:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder { this };
    }
};

class TestView : public QcwView
{
public:
    using QcwView::QcwView;
};

class TestPerspective : public QcwPerspective
{
public:
    explicit TestPerspective(QcwWindow* window) :
        QcwPerspective { window }
    {
    }

    void createInitialLayout() override
    {
        auto* container = folderContainer();
        auto* central = window()->sharedFolder();
        if (central)
        {
            container->addSharedFolder(central, nullptr, qcw::RightDockArea);
        }
        else
        {
            central = container->createFolder(nullptr, qcw::LeftDockArea);
        }
        container->setCentralFolder(central);

        mBottom = container->createFolder(central, qcw::BottomDockArea);
        container->setEmptyFolderAction(mBottom, qcw::EmptyFolderAction::Keep);

        mResults = new TestView;
        mResults->setType(QStringLiteral("test.results"));
        mBottom->addTab(mResults, QStringLiteral("Results"));

        mOther = new TestView;
        mOther->setType(QStringLiteral("test.other"));
        mBottom->addTab(mOther, QStringLiteral("Other"));
    }

    QcwTabFolder* bottom() const
    {
        return mBottom;
    }
    TestView* results() const
    {
        return mResults;
    }

private:
    QcwTabFolder* mBottom { nullptr };
    TestView* mResults { nullptr };
    TestView* mOther { nullptr };
};
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    TestWindow window;
    window.setId(QStringLiteral("view-reveal-test"));
    window.setupUi();
    window.show();
    settle();

    auto* first = new TestPerspective { &window };
    first->createInitialLayout();
    window.openPerspective(first);
    settle();

    auto* second = new TestPerspective { &window };
    second->createInitialLayout();

    check(window.viewOfType(QStringLiteral("test.results")) == first->results(),
          "viewOfType finds the view in the current perspective");
    check(window.viewOfType(QStringLiteral("test.missing")) == nullptr, "viewOfType returns null for an unknown type");

    // Selecting a tab.
    first->bottom()->setCurrentIndex(1);
    settle();
    check(window.currentView() == first->bottom()->tabPane(1), "the other tab is current");
    check(window.revealView(first->results()), "revealView accepts a view of the window");
    settle();
    check(first->bottom()->currentIndex() == 0 && window.currentView() == first->results(),
          "revealView selects the view's tab");

    // A minimized folder is restored, not just activated.
    first->bottom()->minimize();
    settle();
    check(first->bottom()->state() == qcw::FolderState::Minimized, "the folder is minimized");
    check(window.revealViewOfType(QStringLiteral("test.results")) == first->results(),
          "revealViewOfType finds the parked view");
    settle();
    check(first->bottom()->state() != qcw::FolderState::Minimized, "revealView restores the folder");

    // A view in a visible perspective wins over one in a hidden perspective.
    window.openPerspective(second);
    settle();
    check(window.currentPerspective() == second, "the second perspective is current");
    check(window.viewOfType(QStringLiteral("test.results")) == second->results(),
          "viewOfType prefers the current perspective");

    // Revealing a view in the hidden perspective switches to it.
    window.setCurrentPerspective(first->id());
    settle();
    check(window.currentPerspective() == first, "the first perspective is current again");
    check(window.revealView(second->results()), "revealView accepts a view in another perspective");
    settle();
    check(window.currentPerspective() == second && window.currentView() == second->results(),
          "revealView switches to the view's perspective");

    return gFailures == 0 ? 0 : 1;
}
