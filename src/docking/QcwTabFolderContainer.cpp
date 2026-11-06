// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTabFolderContainer.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderSite.h"
#include "QcwSerializeUtils.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QPainter>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QApplication>
#include <QPointer>
#include <QWindow>

QT_BEGIN_NAMESPACE

class QcwDockAreaOverlay : public QWidget
{
public:
    explicit QcwDockAreaOverlay(QWidget* parent = nullptr);

    void setDockArea(qcw::DockTargetType type, qcw::DockArea area);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor borderColor;
    qcw::DockTargetType targetType {};
    qcw::DockArea dockArea { qcw::NoDockArea };
};

// -----------------------------------------------------------------------------

struct QcwFolderExtraInfo
{
    qcw::DockAreas allowedDockAreas { qcw::AllDockAreas };
    qcw::EmptyFolderAction emptyFolderAction { qcw::EmptyFolderAction::Delete };
};

struct QcwMaximizedFolderInfo
{
    QcwTabFolder* folder {};              // the maximized folder
    QSet<QcwTabFolder*> minimizedFolders; // the folders affected by maximization
};

struct QcwMinimizedFolderInfo
{
    qcw::FolderFeatures features { qcw::DefaultTabFeatures }; // the original features of the minimized folder
    Qt::ToolBarArea fastViewToolBarArea { Qt::RightToolBarArea };
};

// -----------------------------------------------------------------------------

class QcwTabFolderContainerPrivate
{
public:
    explicit QcwTabFolderContainerPrivate(QcwTabFolderContainer* interface);

    qcw::DockArea determineDockArea(const QPoint& pos) const;
    qcw::DockArea determineDockArea(QcwTabFolder* target, const QPoint& pos) const;

    void showOverlay(QWidget* target, qcw::DockArea dockArea, qreal overlayRatio);

    void clearSplitters(QSplitter* parentSplitter, QSplitter* stopSplitter);

    void saveSizes(QSplitter* splitter);
    QList<int> savedSizes(QSplitter* splitter) const;
    bool hasMinimizedDescendant(QSplitter* splitter) const;
    void pruneSizes(QSplitter* splitter);

    void rememberSplitterSizes();
    void applyRememberedSplitterSizes();
    void applyRememberedSplitterSizes(QSplitter* splitter);

    bool isResizingWithContainer(QWidget* widget);
    void updateStretchFactors(QSplitter* splitter);
    void updateStretchFactorsRecursive(QSplitter* splitter);

    void addFolder(QcwTabFolder* folder, QcwTabFolder* reference, qcw::DockArea dockArea);
    void connectFolder(QcwTabFolder* folder);
    void forgetFolder(QcwTabFolder* folder);

    void emitCurrentFolderChanged();

#ifdef QCW_DEBUG
    void dumpLayout(const QString& event);
    void dumpLayout(QSplitter* splitter, int& indentLevel) const;
#endif // QCW_DEBUG

public:
    QcwTabFolderContainer* owner;
    QList<QMetaObject::Connection> connections;
    QVBoxLayout* layout;
    QSplitter* rootSplitter;
    QMap<QcwTabFolder*, QcwFolderExtraInfo> folders;
    QcwTabFolder* currentFolder { nullptr };
    QList<QcwTabFolder*> recentFolders;

    QMap<QcwTabFolder*, QList<int>> originalSizes;

    // Sizes of the last layout this container showed with the shared folder in place. The
    // folder is one widget for every perspective, so while another perspective is current
    // this container's shared site is empty and its live sizes mean nothing.
    QMap<QSplitter*, QList<int>> rememberedSizes;
    bool hasBeenShown { false };

    QcwMaximizedFolderInfo maximizedFolder;
    QMap<QcwTabFolder*, QcwMinimizedFolderInfo> minimizedFolders;
    QcwTabFolder* centralFolder { nullptr };
    QPointer<QcwTabFolder> sharedFolder { nullptr };
    QWidget* sharedFolderSite { nullptr };

    // The folder this container was handed through addSharedFolder() or restoreState(). It is
    // not the container's to delete: its creator keeps it, another container may be showing it,
    // and clear() drops sharedFolder along with the layout while this one stays.
    QPointer<QcwTabFolder> borrowedFolder { nullptr };
    QcwDockAreaOverlay* overlay;
    QcwTabFolder* reference { nullptr };
    qcw::DockArea targetDockArea { qcw::NoDockArea };
    qcw::DockAreas allowedDockAreas { qcw::ContainerDockAreas };
    int lastFolderId { int(qcw::FolderStartingIds::Normal) }; // ids below belong to shared/floating folders
};

// -----------------------------------------------------------------------------

inline Qt::Orientation dockOrientation(qcw::DockArea area)
{
    return area == qcw::LeftDockArea || area == qcw::RightDockArea ? Qt::Horizontal : Qt::Vertical;
}

inline bool isDockingAtFront(qcw::DockArea area)
{
    return area == qcw::LeftDockArea || area == qcw::TopDockArea;
}

inline int visibleChildrenCount(QSplitter* splitter)
{
    int n = 0;
    for (int i = 0; i < splitter->count(); ++i)
    {
        auto widget = splitter->widget(i);
        n += (widget && widget->isVisible());
    }

    return n;
}

QcwTabFolderContainer::QcwTabFolderContainer(QWidget* parent) :
    QWidget { parent }, impl { new QcwTabFolderContainerPrivate { this } }
{
    impl->layout = new QVBoxLayout();
    impl->layout->setObjectName("folderContainerLayout");
    impl->layout->setContentsMargins(0, 0, 0, 0);
    setLayout(impl->layout);

    impl->overlay = new QcwDockAreaOverlay(this);
    impl->overlay->hide();

    impl->rootSplitter = new QSplitter(this);
    impl->rootSplitter->setChildrenCollapsible(false);
    impl->layout->addWidget(impl->rootSplitter);

    setMouseTracking(true);
    setAcceptDrops(true);
}

QcwTabFolderContainer::~QcwTabFolderContainer()
{
    for (auto& conn : impl->connections)
    {
        disconnect(conn);
    }
    impl->connections.clear();

    if (impl->sharedFolder && impl->sharedFolder->parentWidget() == impl->sharedFolderSite)
    {
        impl->sharedFolder->setParent(nullptr); // the window owns it: detach instead of delete
        impl->sharedFolder = nullptr;
    }
    delete impl;
}

QcwTabFolder* QcwTabFolderContainer::createFolder(QcwTabFolder* reference, qcw::DockArea dockArea)
{
    auto allowedAreas = reference ? allowedDockAreas(reference) : impl->allowedDockAreas;
    if (!allowedAreas.testFlag(dockArea))
    {
        return nullptr;
    }

    if (dockArea == qcw::CenterDockArea)
    {
        return reference;
    }

    auto folder = new QcwTabFolder;
    folder->setProperty("FolderId", ++impl->lastFolderId);
    impl->addFolder(folder, reference, dockArea);

    emit folderCreated(folder);

    return folder;
}

QcwTabFolder* QcwTabFolderContainer::sharedFolder() const
{
    return impl->sharedFolder;
}

bool QcwTabFolderContainer::addSharedFolder(QcwTabFolder* folder, QcwTabFolder* reference, qcw::DockArea dockArea)
{
    auto allowedAreas = reference ? allowedDockAreas(reference) : impl->allowedDockAreas;
    if (!folder || impl->sharedFolder || dockArea == qcw::CenterDockArea || !allowedAreas.testFlag(dockArea))
    {
        return false;
    }

    impl->addFolder(folder, reference, dockArea);
    impl->sharedFolder = folder;
    impl->borrowedFolder = folder;
    impl->sharedFolderSite = impl->sharedFolder->parentWidget();

    emit folderCreated(folder);

    return true;
}

QcwTabFolder* QcwTabFolderContainer::centralFolder() const
{
    return impl->centralFolder;
}

void QcwTabFolderContainer::setCentralFolder(QcwTabFolder* folder)
{
    if (folder != impl->centralFolder)
    {
        impl->centralFolder = folder;
    }
}

bool QcwTabFolderContainer::isCentralFolder(QcwTabFolder* folder) const
{
    return folder == impl->centralFolder;
}

bool QcwTabFolderContainer::isMinimizedFolder(QcwTabFolder* folder) const
{
    return impl->minimizedFolders.contains(folder) || impl->maximizedFolder.minimizedFolders.contains(folder);
}

QcwTabFolder* QcwTabFolderContainer::parentFolder(QWidget* child) const
{
    // QcwTabFolder::indexOf(nullptr) matches a tab without a pane, so a null
    // child must not reach it.
    if (!child)
    {
        return nullptr;
    }

    for (const auto& [folder, flags] : impl->folders.asKeyValueRange())
    {
        if (folder->indexOf(child) != -1)
        {
            return folder;
        }
    }
    return nullptr;
}

QcwTabFolder* QcwTabFolderContainer::currentFolder() const
{
    return impl->currentFolder;
}

QSet<QcwTabFolder*> QcwTabFolderContainer::folders() const
{
    auto keys = impl->folders.keys();
    return QSet<QcwTabFolder*> { keys.begin(), keys.end() };
}

qcw::EmptyFolderAction QcwTabFolderContainer::emptyFolderAction(QcwTabFolder* folder) const
{
    auto it = impl->folders.find(folder);
    return it != impl->folders.end() ? it->emptyFolderAction : qcw::EmptyFolderAction::Delete;
}

void QcwTabFolderContainer::setEmptyFolderAction(QcwTabFolder* folder, qcw::EmptyFolderAction action)
{
    if (folder != impl->centralFolder)
    {
        auto it = impl->folders.find(folder);
        if (it != impl->folders.end())
        {
            it->emptyFolderAction = action;
        }
    }
}

qcw::DockAreas QcwTabFolderContainer::allowedDockAreas(QcwTabFolder* folder) const
{
    auto it = impl->folders.find(folder);
    return it != impl->folders.end() ? it->allowedDockAreas : qcw::NoDockArea;
}

void QcwTabFolderContainer::setAllowedDockAreas(QcwTabFolder* folder, qcw::DockAreas areas)
{
    auto it = impl->folders.find(folder);
    if (it != impl->folders.end() && it->allowedDockAreas != areas)
    {
        it->allowedDockAreas = areas;
        it.key()->setFeature(qcw::AcceptDroppedTabs, areas.testFlag(qcw::CenterDockArea));
    }
}

qcw::DockAreas QcwTabFolderContainer::allowedDockAreas() const
{
    return impl->allowedDockAreas;
}

void QcwTabFolderContainer::setAllowedDockAreas(qcw::DockAreas areas)
{
    impl->allowedDockAreas = areas;
}

QSplitter* QcwTabFolderContainer::rootSplitter() const
{
    return impl->rootSplitter;
}

QSplitter* QcwTabFolderContainer::parentSplitter(QWidget* child) const
{
    if (!child)
    {
        return nullptr;
    }

    auto parent = child->parentWidget();
    while (parent)
    {
        if (QSplitter* splitter = qobject_cast<QSplitter*>(parent))
        {
            return splitter;
        }
        parent = parent->parentWidget();
    }
    return nullptr;
}

static void saveSplitterState(QXmlStreamWriter& stream,
                              QSplitter* splitter,
                              int index,
                              int& splitterId,
                              QMap<int, QList<int>>& splitterSizes,
                              const QMap<QSplitter*, QList<int>>& rememberedSizes)
{
    auto remembered = rememberedSizes.constFind(splitter);
    splitterSizes.insert(splitterId, remembered != rememberedSizes.cend() ? remembered.value() : splitter->sizes());

    stream.writeStartElement("splitter");
    stream.writeAttribute("id", QString::number(splitterId));
    stream.writeAttribute("orientation", QString::number(splitter->orientation()));
    stream.writeAttribute("count", QString::number(splitter->count()));
    ++splitterId;

    for (int i = 0; i < splitter->count(); ++i)
    {
        auto widget = splitter->widget(i);
        if (auto childSplitter = qobject_cast<QSplitter*>(widget))
        {
            saveSplitterState(stream, childSplitter, i, splitterId, splitterSizes, rememberedSizes);
        }
        else if (auto site = qobject_cast<QcwTabFolderSite*>(widget))
        {
            stream.writeStartElement("folderSite");
            stream.writeAttribute("id", QString::number(site->property("FolderId").toInt()));

            auto folder = site->folder();
            bool visible = folder && folder->state() != qcw::FolderState::Minimized;
            stream.writeAttribute("visible", QString::number(visible));
            stream.writeEndElement();
        }
    }

    stream.writeEndElement();
}

static QByteArray serializeSizes(const QList<int>& sizes)
{
    auto ba = QcwSerializeUtils::serialize(sizes);
    return ba.toHex(' ');
}

void QcwTabFolderContainer::saveState(QXmlStreamWriter& writer) const
{
    bool hasSharedFolder = impl->sharedFolder != nullptr;
    writer.writeStartElement("folderContainer"); // --> folderContainer begin
    writer.writeAttribute("hasSharedFolder", QString::number(hasSharedFolder));
    writer.writeAttribute(
        "centralFolder", QString::number(impl->centralFolder ? impl->centralFolder->property("FolderId").toInt() : -1));
    writer.writeAttribute(
        "currentFolder", QString::number(impl->currentFolder ? impl->currentFolder->property("FolderId").toInt() : -1));

    writer.writeStartElement("folders"); // --> folders begin
    writer.writeAttribute("count", QString::number(impl->folders.size()));
    for (const auto& [folder, flags] : impl->folders.asKeyValueRange())
    {
        if (folder == impl->sharedFolder)
        {
            continue;
        }

        writer.writeStartElement("folderInfo"); // --> folderInfo begin
        writer.writeAttribute("id", QString::number(folder->property("FolderId").toInt()));
        writer.writeAttribute("allowedDockAreas", QString::number(flags.allowedDockAreas));
        writer.writeAttribute("emptyFolderAction", QString::number(std::to_underlying(flags.emptyFolderAction)));

        folder->saveState(writer);

        writer.writeEndElement(); // <-- folderInfo end
    }
    writer.writeEndElement(); // <-- folders end

    // key: splitter id, values: splitter->sizes().
    QMap<int, QList<int>> splitterSizes;
    int splitterId = 0;

    // A hidden container no longer holds the shared folder, so its live splitters have already
    // collapsed the empty site. Write what the last visible layout was instead; the snapshot a
    // restore read from the document counts as one, which keeps repeated saves stable.
    const QMap<QSplitter*, QList<int>> rememberedSizes =
        isVisible() ? QMap<QSplitter*, QList<int>> {} : impl->rememberedSizes;

    writer.writeStartElement("splitters");
    saveSplitterState(writer, impl->rootSplitter, 0, splitterId, splitterSizes, rememberedSizes);
    writer.writeEndElement();

    writer.writeStartElement("splitterSizes");
    for (const auto& [id, sizes] : splitterSizes.asKeyValueRange())
    {
        writer.writeStartElement("splitterSize");
        writer.writeAttribute("id", QString::number(id));

        auto ba = QcwSerializeUtils::serialize(sizes);
        writer.writeAttribute("sizes", ba.toHex(' '));
        writer.writeEndElement();
    }
    writer.writeEndElement();

    if (impl->maximizedFolder.folder && !impl->maximizedFolder.minimizedFolders.empty())
    {
        writer.writeStartElement("maximizedFolder");
        writer.writeAttribute("id", QString::number(impl->maximizedFolder.folder->property("FolderId").toInt()));

        QList<int> minFolderIds;
        for (auto folder : std::as_const(impl->maximizedFolder.minimizedFolders))
        {
            minFolderIds.append(folder->property("FolderId").toInt());
        }

        auto ba = QcwSerializeUtils::serialize(minFolderIds);
        writer.writeAttribute("minimizedFolders", ba.toHex(' '));
        writer.writeEndElement();
    }

    if (!impl->minimizedFolders.empty())
    {
        writer.writeStartElement("minimizedFolders");
        for (const auto& [folder, info] : impl->minimizedFolders.asKeyValueRange())
        {
            writer.writeStartElement("folder");

            writer.writeAttribute("id", QString::number(folder->property("FolderId").toInt()));
            writer.writeAttribute("features", QString::number(info.features));
            writer.writeAttribute("fastViewToolBarArea", QString::number(info.fastViewToolBarArea));

            writer.writeEndElement();
        }
        writer.writeEndElement();
    }

    if (!impl->originalSizes.empty())
    {
        writer.writeStartElement("originalSizes");
        for (const auto& [folder, sizes] : impl->originalSizes.asKeyValueRange())
        {
            writer.writeStartElement("folderSizes");
            writer.writeAttribute("folderId", QString::number(folder->property("FolderId").toInt()));
            writer.writeAttribute("sizes", serializeSizes(sizes));
            writer.writeEndElement();
        }
        writer.writeEndElement();
    }

    writer.writeEndElement(); // <-- folderContainer end
}

static void restoreSplitterState(QXmlStreamReader& reader,
                                 QSplitter* splitter,
                                 QMap<int, QSplitter*>& splitters,
                                 QMap<int, QcwTabFolderSite*>& sites)
{
    QXmlStreamAttributes attrs = reader.attributes();
    splitter->setOrientation(Qt::Orientation(attrs.value("orientation").toInt()));
    splitters.insert(attrs.value("id").toInt(), splitter);

    while (reader.readNextStartElement())
    {
        if (reader.name() == "folderSite")
        {
            int siteId = reader.attributes().value("id").toInt();
            bool visible = reader.attributes().value("visible").toInt();

            auto site = new QcwTabFolderSite(nullptr, splitter);
            site->setProperty("FolderId", siteId);
            sites.insert(siteId, site);

            splitter->addWidget(site);
            site->setVisible(visible);

            reader.skipCurrentElement();
        }
        else if (reader.name() == "splitter")
        {
            auto childSplitter = new QSplitter(splitter);
            childSplitter->setChildrenCollapsible(false);
            restoreSplitterState(reader, childSplitter, splitters, sites);

            splitter->addWidget(childSplitter);
        }
        else
        {
            reader.skipCurrentElement();
        }
    }
}

static QList<int> deserializeSizes(QStringView sizesString)
{
    auto ba = QByteArray::fromHex(sizesString.toLocal8Bit());
    return QcwSerializeUtils::deserialize<QList<int>>(ba);
}

bool QcwTabFolderContainer::restoreState(QXmlStreamReader& reader,
                                         QcwTabFolder* folder,
                                         const QMap<int, QPair<QWidget*, int>>& tabPanes)
{
    QMap<int, QSplitter*> splitters;
    QMap<int, QcwTabFolderSite*> sites;
    QMap<int, QList<int>> splitterSizes;
    QMap<int, QcwFolderExtraInfo> extraInfos;
    QMap<int, QcwTabFolder*> folders;

    QPair<int, QList<int>> maximizedFolder { -1, {} };
    QMap<int, QcwMinimizedFolderInfo> minimizedFolders;
    QMap<int, QList<int>> originalSizes;

    if (reader.hasError())
    {
        return false;
    }

    bool restored = true;

    // A missing attribute must not read as id 0 - shared folders may own that id.
    auto readId = [&reader](const QString& name)
    {
        bool ok = false;
        const int id = reader.attributes().value(name).toInt(&ok);
        return ok ? id : -1;
    };
    int centralFolderId = readId("centralFolder");
    int currentFolderId = readId("currentFolder");
    bool hasSharedFolder = reader.attributes().value("hasSharedFolder").toString().toInt();

    int sharedFolderId = -1;
    if (hasSharedFolder && folder)
    {
        sharedFolderId = folder->property("FolderId").toInt();
        folders.insert(sharedFolderId, folder);
    }

    int maxFolderId = int(qcw::FolderStartingIds::Normal);
    while (reader.readNextStartElement())
    {
        const auto elemName = reader.name();

        if (elemName == "folders")
        {
            int count = reader.attributes().value("count").toString().toInt();
            if (count == 0 || (count == 1 && hasSharedFolder))
            {
                reader.skipCurrentElement();
            }
            else
            {
                while (reader.readNextStartElement())
                {
                    if (reader.name() == "folderInfo")
                    {
                        QcwFolderExtraInfo extraInfo;
                        int id = reader.attributes().value("id").toInt();
                        extraInfo.allowedDockAreas =
                            qcw::DockAreas::fromInt(reader.attributes().value("allowedDockAreas").toString().toInt());
                        extraInfo.emptyFolderAction =
                            qcw::EmptyFolderAction(reader.attributes().value("emptyFolderAction").toString().toInt());
                        extraInfos.insert(id, extraInfo);

                        if (id > maxFolderId)
                        {
                            maxFolderId = id;
                        }

                        while (reader.readNextStartElement())
                        {
                            if (reader.name() == "folder")
                            {
                                auto folder = new QcwTabFolder;
                                restored = folder->restoreState(reader, tabPanes) && restored;
                                folders.insert(id, folder);
                            }
                            else
                            {
                                reader.skipCurrentElement();
                            }
                        }
                    }
                    else
                    {
                        reader.skipCurrentElement();
                    }
                }
            }
        }
        else if (elemName == "splitters")
        {
            while (reader.readNextStartElement())
            {
                if (reader.name() == "splitter")
                {
                    restoreSplitterState(reader, impl->rootSplitter, splitters, sites);
                }
                else
                {
                    reader.skipCurrentElement();
                }
            }
        }
        else if (elemName == "splitterSizes")
        {
            while (reader.readNextStartElement())
            {
                if (reader.name() == "splitterSize")
                {
                    int id = reader.attributes().value("id").toInt();
                    splitterSizes.insert(id, deserializeSizes(reader.attributes().value("sizes")));
                }
                reader.skipCurrentElement();
            }
        }
        else if (elemName == "maximizedFolder")
        {
            maximizedFolder.first = reader.attributes().value("id").toInt();
            maximizedFolder.second = deserializeSizes(reader.attributes().value("minimizedFolders"));

            reader.skipCurrentElement();
        }
        else if (elemName == "minimizedFolders")
        {
            while (reader.readNextStartElement())
            {
                if (reader.name() == "folder")
                {
                    int id = reader.attributes().value("id").toInt();
                    auto features = qcw::FolderFeatures::fromInt(reader.attributes().value("features").toInt());
                    auto toolBarArea = Qt::ToolBarArea(reader.attributes().value("fastViewToolBarArea").toInt());

                    minimizedFolders.insert(id, { features, toolBarArea });
                }
                reader.skipCurrentElement();
            }
        }
        else if (elemName == "originalSizes")
        {
            while (reader.readNextStartElement())
            {
                if (reader.name() == "folderSizes")
                {
                    int id = reader.attributes().value("folderId").toInt();
                    auto sizes = deserializeSizes(reader.attributes().value("sizes"));
                    originalSizes.insert(id, sizes);
                }
                reader.skipCurrentElement();
            }
        }
        else
        {
            reader.skipCurrentElement();
        }
    }

    impl->rememberedSizes.clear();
    for (const auto& [id, splitter] : splitters.asKeyValueRange())
    {
        auto it = splitterSizes.find(id);
        if (it == splitterSizes.end() || it.value().size() != splitter->count())
        {
            continue;
        }

        // A site the document marks visible with no size at all is what the old restore path
        // wrote after the shared folder had already left for another perspective's container.
        // Applying such a snapshot would keep that folder collapsed forever, so leave the
        // splitter to its default layout instead.
        bool collapsedVisibleSite = false;
        for (int i = 0; i < splitter->count() && !collapsedVisibleSite; ++i)
        {
            auto site = qobject_cast<QcwTabFolderSite*>(splitter->widget(i));
            collapsedVisibleSite = site && !site->isHidden() && it.value().at(i) <= 0;
        }

        if (collapsedVisibleSite)
        {
            continue;
        }

        splitter->setSizes(it.value());

        // The shared folder is installed below and may already be held by the container
        // restored before this one. Keep the document's sizes as the layout to bring back
        // when this perspective is first shown.
        impl->rememberedSizes.insert(splitter, it.value());
    }

    for (const auto& [id, folder] : folders.asKeyValueRange())
    {
        auto site = sites.value(id);
        if (!site)
        {
            delete folder;
            continue;
        }

        site->setFolder(folder);
        folder->show();

        if (centralFolderId == id)
        {
            impl->centralFolder = folder;
        }

        impl->connectFolder(folder);
        impl->folders.insert(folder, extraInfos.value(id));

        emit folderCreated(folder);

        if (id == currentFolderId)
        {
            impl->currentFolder = folder;
            impl->emitCurrentFolderChanged();
        }
    }

    for (const auto& [folderId, info] : minimizedFolders.asKeyValueRange())
    {
        auto folder = folders.value(folderId);
        if (folder)
        {
            impl->minimizedFolders.insert(folder, info);
        }
    }

    if (maximizedFolder.first != -1)
    {
        auto folder = folders.value(maximizedFolder.first);
        if (folder && !maximizedFolder.second.empty())
        {
            QSet<QcwTabFolder*> minFolders;
            for (auto id : std::as_const(maximizedFolder.second))
            {
                auto minFolder = folders.value(id);
                if (minFolder)
                {
                    minFolders.insert(minFolder);
                }
            }

            if (!minFolders.empty())
            {
                impl->maximizedFolder.folder = folder;
                impl->maximizedFolder.minimizedFolders = minFolders;

                // Set the folder state to Normal first,
                // just to ensure that the folder displays the maximize button correctly.
                folder->setState(qcw::FolderState::Normal);
                folder->setState(qcw::FolderState::Maximized);
            }
        }
    }

    for (const auto& [id, sizes] : originalSizes.asKeyValueRange())
    {
        auto folder = folders.value(id);
        if (folder)
        {
            impl->originalSizes.insert(folder, sizes);
        }
    }

    impl->lastFolderId = maxFolderId;

    for (const auto& [folder, info] : impl->minimizedFolders.asKeyValueRange())
    {
        emit folderMinimized(folder, info.fastViewToolBarArea);
    }

    if (hasSharedFolder && folder)
    {
        impl->sharedFolder = folder;
        impl->borrowedFolder = folder;
        impl->sharedFolderSite = sites.value(sharedFolderId);
    }

    impl->updateStretchFactorsRecursive(impl->rootSplitter);

#ifdef QCW_DEBUG
    impl->dumpLayout("Restore");
#endif

    return restored && !reader.hasError();
}

static void removeSplitterChildren(QSplitter* splitter)
{
    for (int i = 0; i < splitter->count(); ++i)
    {
        auto widget = splitter->widget(i);
        if (widget)
        {
            if (auto childSplitter = qobject_cast<QSplitter*>(widget))
            {
                removeSplitterChildren(childSplitter);
                childSplitter->deleteLater();
            }
            else
            {
                widget->deleteLater();
            }
        }
    }
}

void QcwTabFolderContainer::clear()
{
    impl->folders.clear();
    impl->originalSizes.clear();
    impl->rememberedSizes.clear();
    impl->recentFolders.clear();
    impl->maximizedFolder = {};
    impl->minimizedFolders.clear();
    impl->currentFolder = nullptr;
    impl->centralFolder = nullptr;
    if (impl->sharedFolder && impl->sharedFolder->parentWidget() == impl->sharedFolderSite)
    {
        impl->sharedFolder->setParent(nullptr); // the window owns it: detach instead of delete
    }
    impl->sharedFolder = nullptr;
    impl->sharedFolderSite = nullptr;
    impl->reference = nullptr;
    impl->targetDockArea = qcw::NoDockArea;
    impl->allowedDockAreas = qcw::ContainerDockAreas;
    impl->lastFolderId = int(qcw::FolderStartingIds::Normal);

    removeSplitterChildren(impl->rootSplitter);

    auto old = impl->rootSplitter;
    impl->rootSplitter = new QSplitter(this);
    impl->rootSplitter->setOrientation(old->orientation());

    auto item = impl->layout->replaceWidget(old, impl->rootSplitter);
    old->deleteLater();
    delete item;

#ifdef QCW_DEBUG
    impl->dumpLayout("Clear");
#endif
}

void QcwTabFolderContainer::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(qcw::TabMimeType))
    {
        // Every drag starts without a target: a drop must act on a move of this
        // drag, not on the area a previous one left behind.
        impl->reference = nullptr;
        impl->targetDockArea = qcw::NoDockArea;

        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void QcwTabFolderContainer::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(qcw::TabMimeType))
    {
        QcwTabFolder* source = nullptr;
        if (!event->source() || !(source = qobject_cast<QcwTabFolder*>(event->source())))
        {
            event->ignore();
            return;
        }

        QByteArray itemData = event->mimeData()->data(qcw::TabMimeType);
        QDataStream dataStream(&itemData, QIODevice::ReadOnly);
        int index = -1;
        dataStream >> index;

        if (!source->isValidIndex(index) || !source->isTabDetachable(index))
        {
            event->ignore();
            return;
        }

        auto pos = event->position().toPoint();
        auto area = impl->determineDockArea(pos);
        if (area != qcw::NoDockArea)
        {
            impl->reference = nullptr;
            impl->targetDockArea = area;

            impl->overlay->setDockArea(qcw::DockTargetType::FolderContainer, area);
            impl->showOverlay(this, area, 0.25);
            event->accept();
        }
        else
        {
            pos = mapToGlobal(pos);
            QcwTabFolder* target = nullptr;

            const auto folders = impl->folders.keys();
            for (auto folder : folders)
            {
                if (!folder->isFloating() && folder->isVisible() && folder->rect().contains(folder->mapFromGlobal(pos)))
                {
                    target = folder;
                    break;
                }
            }

            if (target)
            {
                area = impl->determineDockArea(target, target->mapFromGlobal(pos));
                if (area != qcw::NoDockArea)
                {
                    impl->reference = target;
                    impl->targetDockArea = area;

                    impl->overlay->setDockArea(qcw::DockTargetType::Folder, area);
                    impl->showOverlay(target, area, 0.5);

                    event->accept();
                }
                else
                {
                    impl->reference = nullptr;
                    impl->targetDockArea = qcw::NoDockArea;

                    event->ignore();
                }
            }
            else
            {
                impl->reference = nullptr;
                impl->targetDockArea = qcw::NoDockArea;

                event->ignore();
            }
        }
    }
}

void QcwTabFolderContainer::dragLeaveEvent(QDragLeaveEvent* event)
{
    impl->overlay->hide();
    event->ignore();
}

void QcwTabFolderContainer::dropEvent(QDropEvent* event)
{
    impl->overlay->hide();

    do
    {
        auto source = qobject_cast<QcwTabFolder*>(event->source());
        if (!source || !event->mimeData()->hasFormat(qcw::TabMimeType) || impl->targetDockArea == qcw::NoDockArea)
        {
            event->ignore();
            break;
        }

        if (source == impl->reference)
        {
            if (impl->targetDockArea == qcw::CenterDockArea || source->count() == 1)
            {
                event->accept();
                break;
            }
        }

        QByteArray itemData = event->mimeData()->data(qcw::TabMimeType);
        QDataStream dataStream(&itemData, QIODevice::ReadOnly);
        int index = -1;
        dataStream >> index;

        if (!source->isValidIndex(index) || !source->isTabDetachable(index))
        {
            // The moves of this drag were refused for the same reason, so a drop
            // that still arrives here must not take the tab either.
            event->ignore();
            break;
        }

        // If the source and reference are in the same splitter, creating a new folder can be avoided
        // by simply placing the source in the position of the reference.
        auto srcSplitter = parentSplitter(source);
        auto refSplitter = parentSplitter(impl->reference);
        if (refSplitter && dockOrientation(impl->targetDockArea) == refSplitter->orientation() && srcSplitter &&
            refSplitter == srcSplitter && source->count() == 1)
        {
            if (impl->targetDockArea == qcw::CenterDockArea)
            {
                int targetIndex = -1;
                source->takeTab(impl->reference, 0, targetIndex);
                event->accept();
                break;
            }

            auto srcSite = source->parentWidget();
            auto refSite = impl->reference->parentWidget();

            int refIndex = refSplitter->indexOf(refSite);
            int srcIndex = refSplitter->indexOf(srcSite);
            int idxDiff = refIndex - srcIndex;

            bool moveToFront = isDockingAtFront(impl->targetDockArea);
            if (idxDiff != (moveToFront ? 1 : -1))
            {
                // QSplitter::insertWidget() takes the *final* index when the widget
                // is already a child. Landing spot: right before the reference when
                // docking at the front, right after it otherwise - and the reference
                // itself shifts one slot left once the source leaves its old place.
                int finalIndex;
                if (srcIndex < refIndex)
                {
                    finalIndex = moveToFront ? refIndex - 1 : refIndex;
                }
                else
                {
                    finalIndex = moveToFront ? refIndex : refIndex + 1;
                }

                auto sizes = refSplitter->sizes();
                auto sz = sizes.takeAt(srcIndex);
                refSplitter->insertWidget(finalIndex, srcSite);
                sizes.insert(finalIndex, sz);

                refSplitter->setSizes(sizes);
                impl->updateStretchFactors(refSplitter);
            }

            event->accept();
            break;
        }

        if (!impl->reference)
        {
            auto orientation = dockOrientation(impl->targetDockArea);
            if (orientation == impl->rootSplitter->orientation())
            {
                if (srcSplitter == impl->rootSplitter && source->count() == 1)
                {
                    // The splitter holds the site, not the folder itself.
                    int srcIndex = impl->rootSplitter->indexOf(source->parentWidget());
                    bool moveToFront =
                        impl->targetDockArea == qcw::LeftDockArea || impl->targetDockArea == qcw::TopDockArea;
                    if (moveToFront && srcIndex == 0)
                    {
                        event->accept();
                        break; // do nothing
                    }

                    if (!moveToFront && srcIndex == impl->rootSplitter->count() - 1)
                    {
                        event->accept();
                        break; // do nothing
                    }
                }
            }
        }

        auto target = createFolder(impl->reference, impl->targetDockArea);
        if (!target)
        {
            event->ignore();
            break;
        }

        int targetIndex = -1;
        source->takeTab(target, index, targetIndex);

        if (targetIndex == -1)
        {
            // The transfer was refused (the target already shows this pane). Only a
            // folder just created for it is empty and must be dropped again; the
            // reference folder keeps living when it rejected the drop.
            if (target->count() == 0)
            {
                emit target->tabsCleared();
            }
            event->ignore();
        }
        else
        {
            event->accept();
        }

    } while (false);
}

void QcwTabFolderContainer::showEvent(QShowEvent* event)
{
    impl->hasBeenShown = true;

    if (impl->sharedFolder && impl->sharedFolderSite)
    {
        impl->sharedFolder->setParent(impl->sharedFolderSite);
        impl->sharedFolder->show();
    }

    // The site that was empty while another perspective held the shared folder has been empty
    // long enough for its splitter to collapse it; put the proportions of the last visible
    // layout back now that the folder is here again.
    impl->applyRememberedSplitterSizes();

    for (auto folder : impl->folders.keys())
    {
        for (int i = 0; i < folder->count(); ++i)
        {
            if (auto widget = folder->tabPane(i))
            {
                widget->setParent(folder);
                if (i == folder->currentIndex())
                {
                    widget->show();
                }
            }
        }
    }

    QWidget::showEvent(event);
}

void QcwTabFolderContainer::hideEvent(QHideEvent* event)
{
    impl->rememberSplitterSizes();
    QWidget::hideEvent(event);
}

// -----------------------------------------------------------------------------

QcwTabFolderContainerPrivate::QcwTabFolderContainerPrivate(QcwTabFolderContainer* interface) :
    owner { interface }
{
}

qcw::DockArea QcwTabFolderContainerPrivate::determineDockArea(const QPoint& pos) const
{
    bool leftAllowed = allowedDockAreas.testFlag(qcw::LeftDockArea);
    bool rightAllowed = allowedDockAreas.testFlag(qcw::RightDockArea);
    bool topAllowed = allowedDockAreas.testFlag(qcw::TopDockArea);
    bool bottomAllowed = allowedDockAreas.testFlag(qcw::BottomDockArea);

    auto r = owner->rect();
    if (topAllowed && pos.y() - r.top() <= 50)
    {
        return qcw::TopDockArea;
    }

    if (bottomAllowed && r.bottom() - pos.y() <= 50)
    {
        return qcw::BottomDockArea;
    }

    if (leftAllowed && pos.x() - r.left() <= 50)
    {
        return qcw::LeftDockArea;
    }

    if (rightAllowed && r.right() - pos.x() <= 50)
    {
        return qcw::RightDockArea;
    }

    return qcw::NoDockArea;
}

qcw::DockArea QcwTabFolderContainerPrivate::determineDockArea(QcwTabFolder* target, const QPoint& pos) const
{
    auto r = target->rect();
    auto centerRect = r.adjusted(r.width() / 3, r.height() / 3, -r.width() / 3, -r.height() / 3);
    auto leftRect = r.adjusted(0, 0, -r.width() * 2 / 3, 0);
    auto rightRect = r.adjusted(r.width() * 2 / 3, 0, 0, 0);
    auto topRect = r.adjusted(0, 0, 0, -r.height() * 2 / 3);
    auto bottomRect = r.adjusted(0, r.height() * 2 / 3, 0, 0);

    auto allowedAreas = owner->allowedDockAreas(target);
    bool leftAllowed = allowedAreas.testFlag(qcw::LeftDockArea);
    bool rightAllowed = allowedAreas.testFlag(qcw::RightDockArea);
    bool topAllowed = allowedAreas.testFlag(qcw::TopDockArea);
    bool bottomAllowed = allowedAreas.testFlag(qcw::BottomDockArea);

    qcw::DockArea area { qcw::NoDockArea };
    do
    {
        if (leftAllowed && leftRect.contains(pos))
        {
            auto hx = pos.x() - r.left();
            if (topAllowed && hx > pos.y() - r.top()) // Closer to the top
            {
                area = qcw::TopDockArea;
                break;
            }

            if (bottomAllowed && hx > r.bottom() - pos.y()) // Closer to the bottom
            {
                area = qcw::BottomDockArea;
                break;
            }

            area = qcw::LeftDockArea;
            break;
        }

        if (rightAllowed && rightRect.contains(pos))
        {
            auto hx = r.right() - pos.x();
            if (topAllowed && hx > pos.y() - r.top()) // Closer to the top
            {
                area = qcw::TopDockArea;
                break;
            }

            if (bottomAllowed && hx > r.bottom() - pos.y()) // Closer to the bottom
            {
                area = qcw::BottomDockArea;
                break;
            }

            area = qcw::RightDockArea;
            break;
        }

        if (topAllowed && topRect.contains(pos))
        {
            auto vy = pos.y() - r.top();
            if (leftAllowed && vy > pos.x() - r.left()) // Closer to the left
            {
                area = qcw::LeftDockArea;
                break;
            }

            if (rightAllowed && vy > r.right() - pos.x()) // Closer to the right
            {
                area = qcw::RightDockArea;
                break;
            }

            area = qcw::TopDockArea;
            break;
        }

        if (bottomAllowed && bottomRect.contains(pos))
        {
            auto vy = r.bottom() - pos.y();
            if (leftAllowed && vy > pos.x() - r.left()) // Closer to the left
            {
                area = qcw::LeftDockArea;
                break;
            }

            if (rightAllowed && vy > r.right() - pos.x()) // Closer to the right
            {
                area = qcw::RightDockArea;
                break;
            }

            area = qcw::BottomDockArea;
            break;
        }

        if (allowedAreas.testFlag(qcw::CenterDockArea) && centerRect.contains(pos))
        {
            area = qcw::CenterDockArea;
            break;
        }
    } while (false);

    return area;
}

void QcwTabFolderContainerPrivate::showOverlay(QWidget* target, qcw::DockArea dockArea, qreal overlayRatio)
{
    auto r = target->rect();

    switch (dockArea)
    {
    case qcw::LeftDockArea:
        r.adjust(0, 0, -r.width() * (1.0 - overlayRatio), 0);
        break;
    case qcw::RightDockArea:
        r.adjust(r.width() * (1.0 - overlayRatio), 0, 0, 0);
        break;
    case qcw::TopDockArea:
        r.adjust(0, 0, 0, -r.height() * (1.0 - overlayRatio));
        break;
    case qcw::BottomDockArea:
        r.adjust(0, r.height() * (1.0 - overlayRatio), 0, 0);
        break;
    case qcw::CenterDockArea:
        break;
    default:
        r = QRect();
    }

    if (!r.isNull())
    {
        auto pos = target->mapToGlobal(r.topLeft());
        pos = owner->mapFromGlobal(pos);
        r.moveTo(pos);

        overlay->setGeometry(r);
        overlay->show();
        overlay->raise();
    }
}

void QcwTabFolderContainerPrivate::clearSplitters(QSplitter* parentSplitter, QSplitter* stopSplitter)
{
    if (!parentSplitter)
    {
        return;
    }

    if (parentSplitter->count() == 1)
    {
        auto splitter = qobject_cast<QSplitter*>(parentSplitter->widget(0));
        if (splitter)
        {
            auto origSizes = splitter->sizes();

            for (int j = splitter->count() - 1; j >= 0; --j)
            {
                auto w = splitter->widget(j);
                w->setParent(parentSplitter);
                parentSplitter->insertWidget(0, w);
            }

            parentSplitter->setOrientation(splitter->orientation());
            parentSplitter->setSizes(origSizes);
            updateStretchFactors(parentSplitter);

            bool isStopSplitter = splitter == stopSplitter;
            splitter->setParent(nullptr);
            splitter->deleteLater();

#ifdef QCW_DEBUG
            dumpLayout("Splitter Removed");
#endif

            if (!isStopSplitter)
            {
                clearSplitters(parentSplitter, stopSplitter);
            }
        }

        return;
    }

    auto parentSizes = parentSplitter->sizes();
    for (int i = parentSplitter->count() - 1; i >= 0; --i)
    {
        auto splitter = qobject_cast<QSplitter*>(parentSplitter->widget(i));
        if (!splitter)
        {
            continue;
        }

        if (splitter->count() == 0)
        {
            auto sz = parentSizes[i];
            if (i == parentSplitter->count() - 1)
            {
                parentSizes[i - 1] += sz;
            }
            else
            {
                parentSizes[i + 1] += sz;
            }

            parentSizes.removeAt(i);

            splitter->setParent(nullptr);
            splitter->deleteLater();

            parentSplitter->setSizes(parentSizes);
            updateStretchFactors(parentSplitter);

#ifdef QCW_DEBUG
            dumpLayout("Splitter Removed");
#endif
        }
        else if (splitter->count() == 1)
        {
            auto child = splitter->widget(0);
            child->setParent(parentSplitter);
            parentSplitter->insertWidget(i, child);

            splitter->setParent(nullptr);
            splitter->deleteLater();

            parentSplitter->setSizes(parentSizes);
            updateStretchFactors(parentSplitter);

#ifdef QCW_DEBUG
            dumpLayout("Splitter Removed");
#endif
        }
        else
        {
            if (splitter->orientation() == parentSplitter->orientation())
            {
                auto spSizes = splitter->sizes();
                for (int j = splitter->count() - 1; j >= 0; --j)
                {
                    auto w = splitter->widget(j);
                    parentSplitter->insertWidget(i, w);
                    parentSizes.insert(i, spSizes[j]);
                }

                splitter->setParent(nullptr);
                splitter->deleteLater();

                parentSplitter->setSizes(parentSizes);
                updateStretchFactors(parentSplitter);

#ifdef QCW_DEBUG
                dumpLayout("Splitter Removed");
#endif
            }
            else
            {
                clearSplitters(splitter, stopSplitter);
            }
        }

        if (splitter == stopSplitter)
        {
            break;
        }
    }
}

void QcwTabFolderContainerPrivate::saveSizes(QSplitter* splitter)
{
    // One snapshot per splitter serves every folder in it: restoring any of them has to
    // bring back the same proportions, and a later minimization must not replace it with
    // sizes that already have a hidden sibling collapsed to zero.
    const auto sites = splitter->findChildren<QcwTabFolderSite*>(Qt::FindDirectChildrenOnly);
    for (auto site : sites)
    {
        if (auto folder = site->folder(); folder && originalSizes.contains(folder))
        {
            return;
        }
    }

    const auto sizes = splitter->sizes();
    for (auto site : sites)
    {
        if (auto folder = site->folder())
        {
            originalSizes.insert(folder, sizes);
        }
    }
}

QList<int> QcwTabFolderContainerPrivate::savedSizes(QSplitter* splitter) const
{
    const auto sites = splitter->findChildren<QcwTabFolderSite*>(Qt::FindDirectChildrenOnly);
    for (auto site : sites)
    {
        auto folder = site->folder();
        auto it = folder ? originalSizes.constFind(folder) : originalSizes.cend();
        if (it != originalSizes.cend())
        {
            return it.value();
        }
    }

    return {};
}

static void collectSplitterSizes(QSplitter* splitter, QMap<QSplitter*, QList<int>>& sizes)
{
    sizes.insert(splitter, splitter->sizes());

    for (int i = 0; i < splitter->count(); ++i)
    {
        if (auto child = qobject_cast<QSplitter*>(splitter->widget(i)))
        {
            collectSplitterSizes(child, sizes);
        }
    }
}

void QcwTabFolderContainerPrivate::rememberSplitterSizes()
{
    // Before the first show there is no layout to remember: the splitter was never laid out,
    // so sizes() would report clamped minimums, not what the document described.
    if (!hasBeenShown)
    {
        return;
    }

    // The shared folder belongs to one container at a time. Once another perspective adopted
    // it, this container's site is empty and its splitter has collapsed what the user had
    // arranged; such sizes describe a layout no perspective ever showed, so keep the previous
    // snapshot instead of replacing it.
    if (sharedFolder && sharedFolderSite && sharedFolder->parentWidget() != sharedFolderSite)
    {
        return;
    }

    rememberedSizes.clear();
    collectSplitterSizes(rootSplitter, rememberedSizes);
}

void QcwTabFolderContainerPrivate::applyRememberedSplitterSizes()
{
    applyRememberedSplitterSizes(rootSplitter);
}

void QcwTabFolderContainerPrivate::applyRememberedSplitterSizes(QSplitter* splitter)
{
    auto it = rememberedSizes.constFind(splitter);
    if (it != rememberedSizes.cend() && it.value().size() == splitter->count())
    {
        splitter->setSizes(it.value());
    }

    for (int i = 0; i < splitter->count(); ++i)
    {
        if (auto child = qobject_cast<QSplitter*>(splitter->widget(i)))
        {
            applyRememberedSplitterSizes(child);
        }
    }
}

bool QcwTabFolderContainerPrivate::hasMinimizedDescendant(QSplitter* splitter) const
{
    for (int i = 0; i < splitter->count(); ++i)
    {
        auto widget = splitter->widget(i);
        if (auto site = qobject_cast<QcwTabFolderSite*>(widget))
        {
            auto folder = site->folder();
            if (folder && (minimizedFolders.contains(folder) || maximizedFolder.minimizedFolders.contains(folder)))
            {
                return true;
            }
        }
        else if (auto childSplitter = qobject_cast<QSplitter*>(widget))
        {
            if (hasMinimizedDescendant(childSplitter))
            {
                return true;
            }
        }
    }

    return false;
}

void QcwTabFolderContainerPrivate::pruneSizes(QSplitter* splitter)
{
    // Sizes are only kept while the subtree still has a folder waiting to be restored.
    for (int i = 0; i < splitter->count(); ++i)
    {
        if (auto childSplitter = qobject_cast<QSplitter*>(splitter->widget(i)))
        {
            pruneSizes(childSplitter);
        }
    }

    if (hasMinimizedDescendant(splitter))
    {
        return;
    }

    const auto sites = splitter->findChildren<QcwTabFolderSite*>(Qt::FindDirectChildrenOnly);
    for (auto site : sites)
    {
        auto folder = site->folder();
        // The maximized folder still needs its entry when the siblings are done.
        if (folder && folder != maximizedFolder.folder)
        {
            originalSizes.remove(folder);
        }
    }
}

#ifdef QCW_DEBUG

void QcwTabFolderContainerPrivate::dumpLayout(const QString& event)
{
    qDebug().noquote() << ">>>>" << event;
    int indentLevel = 0;
    dumpLayout(rootSplitter, indentLevel);
    qDebug().noquote() << "----------------------------------------------------------------";
}

void QcwTabFolderContainerPrivate::dumpLayout(QSplitter* splitter, int& indentLevel) const
{
    if (indentLevel > 0)
    {
        qDebug().noquote().nospace() << QString(indentLevel * 4, ' ') << splitter << ' ' << splitter->orientation();
    }
    else
    {
        qDebug().noquote() << splitter << splitter->orientation();
    }

    ++indentLevel;
    for (int i = 0; i < splitter->count(); ++i)
    {
        auto w = splitter->widget(i);
        if (auto c = qobject_cast<QSplitter*>(w))
        {
            dumpLayout(c, indentLevel);
        }
        else if (auto site = qobject_cast<QcwTabFolderSite*>(w))
        {
            auto folder = site->folder();
            qDebug().noquote().nospace() << QString(indentLevel * 4, ' ') << folder;
            if (folder)
            {
                ++indentLevel;
                for (int j = 0; j < folder->count(); ++j)
                {
                    qDebug().noquote().nospace()
                        << QString(indentLevel * 4, ' ') << "QcwTab(" << folder->tabText(j) << ")";
                }
                --indentLevel;
            }
        }
    }
    --indentLevel;
}

#endif // QCW_DEBUG

bool QcwTabFolderContainerPrivate::isResizingWithContainer(QWidget* widget)
{
    if (!centralFolder)
    {
        return true;
    }

    auto folder = qobject_cast<QcwTabFolder*>(widget);
    if (folder)
    {
        return folder == centralFolder;
    }

    // A splitter holds sites, not folders, so the folder check above never
    // matches the central folder's own slot; the site of the central folder
    // is the one child that must keep stretching.
    auto site = qobject_cast<QcwTabFolderSite*>(widget);
    if (site)
    {
        return site->folder() == centralFolder;
    }

    auto splitter = qobject_cast<QSplitter*>(widget);
    if (splitter)
    {
        const auto folders = splitter->findChildren<QcwTabFolder*>();
        for (auto f : folders)
        {
            if (f == centralFolder)
            {
                return true;
            }
        }
        return false;
    }

    return false;
}

void QcwTabFolderContainerPrivate::updateStretchFactors(QSplitter* splitter)
{
    if (!centralFolder || !splitter)
    {
        return;
    }

    for (int i = 0; i < splitter->count(); ++i)
    {
        splitter->setStretchFactor(i, isResizingWithContainer(splitter->widget(i)) ? 1 : 0);
    }
}

void QcwTabFolderContainerPrivate::updateStretchFactorsRecursive(QSplitter* splitter)
{
    if (!splitter)
    {
        return;
    }

    updateStretchFactors(splitter);

    const auto childSplitters = splitter->findChildren<QSplitter*>(Qt::FindDirectChildrenOnly);
    for (auto child : childSplitters)
    {
        updateStretchFactorsRecursive(child);
    }
}

void QcwTabFolderContainerPrivate::addFolder(QcwTabFolder* folder, QcwTabFolder* reference, qcw::DockArea dockArea)
{
    auto site = new QcwTabFolderSite(folder, owner);
    site->setProperty("FolderId", folder->property("FolderId"));

    if (rootSplitter->count() == 0)
    {
        rootSplitter->addWidget(site);
    }
    else
    {
        auto orientation = dockOrientation(dockArea);
        bool dockingAtFront = isDockingAtFront(dockArea);

        if (reference)
        {
            auto refSite = reference->parentWidget();
            auto refSplitter = owner->parentSplitter(refSite);

            if (refSplitter->count() == 1)
            {
                refSplitter->setOrientation(orientation);

                int refSize = orientation == Qt::Horizontal ? refSite->width() : refSite->height();
                if (dockingAtFront)
                {
                    refSplitter->insertWidget(0, site);
                }
                else
                {
                    refSplitter->addWidget(site);
                }

                refSplitter->setSizes({ refSize / 2, refSize / 2 });
                updateStretchFactors(refSplitter);
            }
            else
            {
                auto origSizes = refSplitter->sizes();
                int refIndex = refSplitter->indexOf(refSite);

                if (refSplitter->orientation() == orientation)
                {
                    int refSize = origSizes[refIndex];
                    origSizes[refIndex] /= 2;

                    if (dockingAtFront)
                    {
                        refSplitter->insertWidget(refIndex, site);
                        origSizes.insert(refIndex, refSize / 2);
                    }
                    else
                    {
                        refSplitter->insertWidget(refIndex + 1, site);
                        origSizes.insert(refIndex + 1, refSize / 2);
                    }

                    refSplitter->setSizes(origSizes);
                    updateStretchFactors(refSplitter);
                }
                else
                {
                    int refSize = orientation == Qt::Horizontal ? refSite->width() : refSite->height();

                    auto newSplitter = new QSplitter(owner);
                    newSplitter->setChildrenCollapsible(false);
                    newSplitter->setOrientation(orientation);

                    if (dockingAtFront)
                    {
                        newSplitter->addWidget(site);
                        newSplitter->addWidget(refSite);
                    }
                    else
                    {
                        newSplitter->addWidget(refSite);
                        newSplitter->addWidget(site);
                    }

                    newSplitter->setSizes({ refSize / 2, refSize / 2 });
                    updateStretchFactors(newSplitter);

                    refSplitter->insertWidget(refIndex, newSplitter);
                    refSplitter->setSizes(origSizes);
                    updateStretchFactors(refSplitter);
#ifdef QCW_DEBUG
                    dumpLayout("Splitter Added");
#endif
                }
            }
        }
        else
        {
            int contSize = orientation == Qt::Horizontal ? rootSplitter->width() : rootSplitter->height();
            if (rootSplitter->count() == 1)
            {
                rootSplitter->setOrientation(orientation);
                if (dockingAtFront)
                {
                    rootSplitter->insertWidget(0, site);
                }
                else
                {
                    rootSplitter->addWidget(site);
                }
                rootSplitter->setSizes({ contSize / 2, contSize / 2 });
            }
            else
            {
                if (rootSplitter->orientation() == orientation)
                {
                    auto origSizes = rootSplitter->sizes();
                    if (dockingAtFront)
                    {
                        rootSplitter->insertWidget(0, site);

                        auto refSize = origSizes.front();
                        origSizes.front() /= 2;
                        origSizes.insert(0, refSize / 2);
                    }
                    else
                    {
                        rootSplitter->addWidget(site);

                        auto refSize = origSizes.back();
                        origSizes.back() /= 2;
                        origSizes.append(refSize / 2);
                    }

                    rootSplitter->setSizes(origSizes);
                    updateStretchFactors(rootSplitter);
                }
                else
                {
                    auto newSplitter = new QSplitter(owner);
                    newSplitter->setChildrenCollapsible(false);
                    newSplitter->setOrientation(orientation);

                    int folderSize = contSize / 3;
                    QList<int> newSizes;
                    if (dockingAtFront)
                    {
                        newSplitter->addWidget(site);
                        newSizes.append(folderSize);

                        newSplitter->addWidget(rootSplitter);
                        newSizes.append(contSize - folderSize);
                    }
                    else
                    {
                        newSplitter->addWidget(rootSplitter);
                        newSizes.append(contSize - folderSize);

                        newSplitter->addWidget(site);
                        newSizes.append(folderSize);
                    }

                    layout->addWidget(newSplitter);
                    rootSplitter = newSplitter;
                    rootSplitter->setSizes(newSizes);
                    updateStretchFactors(rootSplitter);
#ifdef QCW_DEBUG
                    dumpLayout("Splitter Added");
#endif
                }
            }
        }
    }

    connectFolder(folder);

    auto stopSplitter = owner->parentSplitter(site);
    clearSplitters(rootSplitter, stopSplitter);
    updateStretchFactorsRecursive(rootSplitter);

    folders.insert(folder, {});

    currentFolder = folder;
    emitCurrentFolderChanged();
}

void QcwTabFolderContainerPrivate::connectFolder(QcwTabFolder* folder)
{
    // The members below key on raw folder pointers, so every folder has to be
    // dropped from them once it dies - e.g. its last tab is detached and the
    // emptied folder gets deleted, or a minimized/maximized folder is closed.
    // Otherwise saveState() dereferences a dangling pointer.
    auto conn = QObject::connect(folder, &QObject::destroyed, [this, folder](QObject*)
    {
        forgetFolder(folder);
    });
    connections.append(conn);

    conn = QObject::connect(folder, &QcwTabFolder::maximizeRequested, [this, folder]()
    {
        auto site = folder->parentWidget();
        if (folder == sharedFolder && site != sharedFolderSite) // sharedFolder owned by the other container
        {
            return;
        }

        maximizedFolder.minimizedFolders.clear();

        auto splitter = owner->parentSplitter(site);
        originalSizes.insert(folder, splitter->sizes());

        if (centralFolder && folder != centralFolder)
        {
            splitter->setStretchFactor(splitter->indexOf(site), 1);
        }

        for (const auto& [otherFolder, flags] : folders.asKeyValueRange())
        {
            if (otherFolder != folder && !maximizedFolder.minimizedFolders.contains(otherFolder) &&
                !minimizedFolders.contains(otherFolder))
            {
                auto otherSplitter = owner->parentSplitter(otherFolder);
                originalSizes.insert(otherFolder, otherSplitter->sizes());
            }
        }

        for (const auto& [otherFolder, flags] : folders.asKeyValueRange())
        {
            if (otherFolder != folder && !maximizedFolder.minimizedFolders.contains(otherFolder) &&
                !minimizedFolders.contains(otherFolder))
            {
                otherFolder->minimize();
                maximizedFolder.minimizedFolders.insert(otherFolder);
            }
        }

        if (!maximizedFolder.minimizedFolders.empty())
        {
            for (int i = 0; i < splitter->count(); ++i)
            {
                auto childSplitter = qobject_cast<QSplitter*>(splitter->widget(i));
                if (childSplitter)
                {
                    childSplitter->hide();
                }
            }

            folder->setState(qcw::FolderState::Maximized);
            maximizedFolder.folder = folder;
        }
        else
        {
            originalSizes.remove(folder);
        }
    });
    connections.append(conn);

    conn = QObject::connect(folder, &QcwTabFolder::restoreRequested, [this, folder]()
    {
        auto restoreMinimizedFolder = [this](QcwTabFolder* folder)
        {
            auto site = folder->parentWidget();
            if (folder == sharedFolder && site != sharedFolderSite)
            {
                return;
            }

            auto it = minimizedFolders.find(folder);
            if (it != minimizedFolders.end())
            {
                folder->setFeatures(it->features);
            }

            site->show();
            folder->setState(qcw::FolderState::Normal);

            minimizedFolders.remove(folder);
            maximizedFolder.minimizedFolders.remove(folder);
            if (maximizedFolder.minimizedFolders.empty() && maximizedFolder.folder)
            {
                if (maximizedFolder.folder->isVisible())
                {
                    maximizedFolder.folder->maximize();
                }
            }

            // Bring hidden ancestors back first and then apply the sizes from the outermost
            // splitter in: resizing a parent resizes the child splitter, so the child's
            // saved sizes have to be restored last.
            QList<QSplitter*> chain;
            for (auto s = owner->parentSplitter(site); s; s = owner->parentSplitter(s))
            {
                chain.prepend(s);
            }

            for (auto s : std::as_const(chain))
            {
                s->show();
            }

            for (auto s : std::as_const(chain))
            {
                const auto sizes = savedSizes(s);
                if (!sizes.isEmpty())
                {
                    s->setSizes(sizes);
                }
            }

            for (auto s : std::as_const(chain))
            {
                pruneSizes(s);
            }

            emit owner->folderRestored(folder);
        };

        // ---------------------------------------------------------------------

        emit folder->aboutToRestore();

        auto site = folder->parentWidget();
        if (folder == sharedFolder && site != sharedFolderSite)
        {
            return;
        }

        if (site->isVisible() && folder == maximizedFolder.folder)
        {
            const auto minFolders = maximizedFolder.minimizedFolders;
            for (auto hiddenFolder : minFolders)
            {
                emit hiddenFolder->aboutToRestore();
                restoreMinimizedFolder(hiddenFolder);
            }

            auto sizesIt = originalSizes.find(folder);
            if (sizesIt != originalSizes.end())
            {
                auto splitter = owner->parentSplitter(site);
                splitter->setSizes(sizesIt.value());
                originalSizes.erase(sizesIt);
            }

            folder->setState(qcw::FolderState::Normal);
            maximizedFolder.folder = nullptr;
            maximizedFolder.minimizedFolders.clear();
        }
        else
        {
            restoreMinimizedFolder(folder);
        }

        updateStretchFactorsRecursive(rootSplitter);
    });
    connections.append(conn);

    conn = QObject::connect(folder, &QcwTabFolder::minimizeRequested, [this, folder]()
    {
        auto site = folder->parentWidget();
        if (folder == sharedFolder && site != sharedFolderSite)
        {
            return;
        }

        auto folderRect = folder->rect();
        auto folderLeft = folder->mapTo(owner, folderRect.topLeft()).x();
        auto folderRight = folder->mapTo(owner, folderRect.topRight()).x();

        bool dockLeft = folderLeft - owner->rect().left() < owner->rect().right() - folderRight;
        auto menuDockArea = dockLeft ? Qt::LeftToolBarArea : Qt::RightToolBarArea;

        auto splitter = owner->parentSplitter(site);
        minimizedFolders.insert(folder, { folder->features(), menuDockArea });

        // The maximized folder is the only visible one, so minimizing it would
        // leave the container with every site hidden - and with a maximization
        // record naming a folder that is no longer maximized. End the
        // maximization first, the way deleting that folder does, and let its
        // hidden siblings come back below.
        const bool wasMaximized = folder == maximizedFolder.folder;
        const auto minimizedByMaximize = maximizedFolder.minimizedFolders;
        if (wasMaximized)
        {
            maximizedFolder = {};
        }

        // Remember the proportions at every level, not just in the folder's own splitter:
        // minimizing collapses whole subtrees, and restoring any given folder has to put
        // all the splitters above it back to the sizes they had while everything was shown.
        for (auto s = splitter; s; s = owner->parentSplitter(s))
        {
            saveSizes(s);
        }

        site->hide();
        folder->setState(qcw::FolderState::Minimized);

        // Drop this folder from the MRU list - not whatever happens to be last.
        recentFolders.removeOne(folder);

        if (visibleChildrenCount(splitter) == 0)
        {
            splitter->hide();
        }

        emit owner->folderMinimized(folder, menuDockArea);

        if (wasMaximized)
        {
            for (auto minimized : std::as_const(minimizedByMaximize))
            {
                emit minimized->restoreRequested();
            }
        }

        if (folder == currentFolder)
        {
            if (!recentFolders.empty())
            {
                currentFolder = recentFolders.back();
                recentFolders.removeOne(currentFolder);
                recentFolders.append(currentFolder);
            }
            else if (!folders.empty())
            {
                currentFolder = folders.firstKey();
                recentFolders.append(currentFolder);
            }
            else
            {
                currentFolder = nullptr;
            }

            emitCurrentFolderChanged();
        }
    });
    connections.append(conn);

    conn = QObject::connect(folder, &QcwTabFolder::tabsCleared, [this, folder]()
    {
        bool isCurrent = folder == currentFolder;

        if (folder != centralFolder)
        {
            auto site = folder->parentWidget();
            const auto action = owner->emptyFolderAction(folder);

            // A folder this container was handed belongs to its creator, so emptying it may
            // never delete it - another container may be showing it, and this one may have
            // been cleared since clear() forgets sharedFolder but not borrowedFolder. Hiding it
            // is the container's own business, but only while it is the one hosting the folder.
            const bool borrowed = folder == borrowedFolder;
            const bool deleteFolder = !borrowed && action == qcw::EmptyFolderAction::Delete;
            const bool hostedHere = site && site == sharedFolderSite;
            const bool hideFolder = action == qcw::EmptyFolderAction::Hide && (!borrowed || hostedHere);

            if (deleteFolder)
            {
                emit folder->aboutToDelete();

                auto splitter = owner->parentSplitter(site);

                folders.remove(folder);
                maximizedFolder.minimizedFolders.remove(folder);

                const bool wasMaximized = folder == maximizedFolder.folder;
                const auto minimizedByMaximize = maximizedFolder.minimizedFolders;
                if (wasMaximized)
                {
                    maximizedFolder = {};
                }

                folder->setParent(nullptr);
                folder->deleteLater();
                site->deleteLater();

#ifdef QCW_DEBUG
                dumpLayout("Folder Removed");
#endif
                clearSplitters(rootSplitter, splitter);
                updateStretchFactorsRecursive(rootSplitter);

                // Restore after the doomed site is collapsed, and through restoreRequested
                // so siblings also leave any fast-view site they were parked in.
                if (wasMaximized)
                {
                    for (auto minimized : std::as_const(minimizedByMaximize))
                    {
                        emit minimized->restoreRequested();
                    }
                }
            }
            else if (hideFolder)
            {
                site->hide();

                maximizedFolder.minimizedFolders.remove(folder);
            }
        }

        recentFolders.removeOne(folder);
        if (isCurrent)
        {
            if (!recentFolders.empty())
            {
                currentFolder = recentFolders.back();
                recentFolders.removeOne(currentFolder);
                recentFolders.append(currentFolder);
            }
            else if (!folders.empty())
            {
                for (const auto& [otherFolder, flags] : folders.asKeyValueRange())
                {
                    if (otherFolder != folder)
                    {
                        currentFolder = otherFolder;
                        break;
                    }
                }
                recentFolders.append(currentFolder);
            }
            else
            {
                currentFolder = nullptr;
            }

            emitCurrentFolderChanged();
        }
    });
    connections.append(conn);

    conn = QObject::connect(folder, &QcwTabFolder::tabBarClicked, [this, folder](int tabIndex)
    {
        if (folder != currentFolder || folder->currentIndex() != tabIndex)
        {
            currentFolder = folder;
            recentFolders.removeOne(folder);
            recentFolders.append(folder);
        }

        emitCurrentFolderChanged();
    });
    connections.append(conn);

#ifdef QCW_DEBUG
    conn = QObject::connect(folder, &QcwTabFolder::tabAdded, [this, folder](int)
    {
        dumpLayout("Tab Added");
    });
    connections.append(conn);

    conn = QObject::connect(folder, &QcwTabFolder::tabRemoved, [this, folder](int, QWidget*)
    {
        dumpLayout("Tab Removed");
    });
    connections.append(conn);

    conn =
        QObject::connect(folder, &QcwTabFolder::tabTransferred, [this, folder](QcwTabFolder*, QcwTabFolder*, QWidget*)
    {
        dumpLayout("Tab Transferred");
    });
    connections.append(conn);

    dumpLayout("Folder Added");
#endif
}

void QcwTabFolderContainerPrivate::forgetFolder(QcwTabFolder* folder)
{
    folders.remove(folder);
    originalSizes.remove(folder);
    minimizedFolders.remove(folder);
    maximizedFolder.minimizedFolders.remove(folder);
    recentFolders.removeOne(folder);

    if (maximizedFolder.folder == folder)
    {
        maximizedFolder.folder = nullptr;
    }

    if (currentFolder == folder)
    {
        currentFolder = nullptr;
    }

    if (centralFolder == folder)
    {
        centralFolder = nullptr;
    }

    if (sharedFolder == folder)
    {
        sharedFolder = nullptr;
        sharedFolderSite = nullptr;
    }

    if (reference == folder)
    {
        reference = nullptr;
    }

    // Emitted last, so receivers observe the container already freed of the folder.
    emit owner->folderDestroyed(folder);
}

void QcwTabFolderContainerPrivate::emitCurrentFolderChanged()
{
    // Active/highlight management is the consumer's responsibility: the
    // perspective (or the standalone host, e.g. the folder-container demo)
    // decides which folder is visually active, since it also knows about
    // floating folders that live outside this container.
    emit owner->currentFolderChanged(currentFolder);
}

// -----------------------------------------------------------------------------

QcwDockAreaOverlay::QcwDockAreaOverlay(QWidget* parent) :
    QWidget { parent }
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    borderColor = parent->palette().color(QPalette::Highlight);
}

void QcwDockAreaOverlay::setDockArea(qcw::DockTargetType type, qcw::DockArea area)
{
    if (type != targetType || area != dockArea)
    {
        targetType = type;
        dockArea = area;
        update();
    }
}

void QcwDockAreaOverlay::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    auto overlayColor = borderColor;
    overlayColor.setAlpha(64);

    painter.setPen(QPen(borderColor, 1.0));
    painter.setBrush(overlayColor);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    if (targetType == qcw::DockTargetType::FolderContainer)
    {
        // clang-format off
        // Built per paint, not cached: a static here would freeze the labels in whichever
        // language was installed the first time a drop was drawn.
        const QMap<qcw::DockArea, QString> dockAreaTexts {
            { qcw::LeftDockArea, QcwTabFolderContainer::tr("PAGE_LEFT") },
            { qcw::RightDockArea, QcwTabFolderContainer::tr("PAGE_RIGHT") },
            { qcw::TopDockArea, QcwTabFolderContainer::tr("PAGE_TOP") },
            { qcw::BottomDockArea, QcwTabFolderContainer::tr("PAGE_BOTTOM") }
        };
        // clang-format on

        const auto& text = dockAreaTexts[dockArea];
        QFontMetrics fm(font());

        auto textRect = fm.boundingRect(text);

        auto r = rect();
        r.setTopLeft(
            QPoint(r.left() + (r.width() - textRect.width()) / 2, r.top() + (r.height() - textRect.height()) / 2));
        r.setSize(textRect.size());

        auto textBgRect = r.adjusted(-4, -4, 4, 4);
        painter.setBrush(palette().color(QPalette::Base));
        painter.drawRect(textBgRect);

        painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, false);
        painter.setPen(QPen(palette().color(QPalette::WindowText)));
        painter.drawText(r, text);
    }
}

QT_END_NAMESPACE
