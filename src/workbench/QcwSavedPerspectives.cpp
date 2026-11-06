// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwSavedPerspectives.h"
#include "QcwPerspective.h"
#include "QcwView.h"
#include "QcwPathUtils.h"
#include "QcwSerializeUtils.h"
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QFile>
#include <QDir>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QcwSavedPerspectives, savedPerspectivesInstance);

QcwSavedPerspectives* QcwSavedPerspectives::instance()
{
    return savedPerspectivesInstance;
}

void QcwSavedPerspectives::initialize()
{
    auto path = QString("%1/perspectives").arg(QcwAppDirectories::configDirectory());
    QDir dir(path);
    if (!dir.exists())
    {
        dir.mkpath(path);
    }

    const auto fileNames = dir.entryList(QDir::Files);
    for (const auto& fileName : fileNames)
    {
        QFile file(dir.absoluteFilePath(fileName));
        if (file.open(QIODevice::ReadOnly))
        {
            QXmlStreamReader reader(&file);
            if (reader.readNextStartElement())
            {
                if (reader.name() == "savedPerspective")
                {
                    auto name = reader.attributes().value("name").toString();
                    auto type = reader.attributes().value("type").toString();

                    if (!name.isEmpty() && !type.isEmpty())
                    {
                        mPerspectives.insert(name, type);
                    }
                }
            }
        }
    }
}

bool QcwSavedPerspectives::saveState(QcwPerspective* perspective)
{
    auto fileName = QString("%1/perspectives/%2.xml").arg(QcwAppDirectories::configDirectory(), perspective->name());
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    QSet<QcwView*> views;
    perspective->collectViews(views);

    QXmlStreamWriter writer(&file);
    writer.writeDTD(R"(<?xml version="1.0" encoding="UTF-8"?>)");
    writer.writeStartElement("savedPerspective"); // savedPerspective being
    writer.writeAttribute("type", perspective->type());
    writer.writeAttribute("name", perspective->name());

    if (!views.empty())
    {
        writer.writeStartElement("views"); // views being
        writer.writeAttribute("count", QString::number(views.size()));
        for (auto view : std::as_const(views))
        {
            writer.writeStartElement("view");

            writer.writeAttribute("id", QString::number(view->id()));
            writer.writeAttribute("type", view->type());
            writer.writeAttribute("name", view->windowTitle());

            auto ba = QcwSerializeUtils::serialize(view->creatingArguments());
            writer.writeAttribute("creatingArguments", ba.toHex(' '));
            writer.writeAttribute("extraState", view->extraStateToHex());

            writer.writeEndElement();
        }
        writer.writeEndElement(); // views end
    }

    perspective->saveState(writer, "perspective");

    writer.writeEndElement(); // savedPerspective end
    writer.writeEndDocument();

    return true;
}

QByteArray QcwSavedPerspectives::loadState(const QString& name)
{
    auto fileName = QString("%1/perspectives/%2.xml").arg(QcwAppDirectories::configDirectory(), name);
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QByteArray {};
    }

    return file.readAll();
}

QT_END_NAMESPACE
