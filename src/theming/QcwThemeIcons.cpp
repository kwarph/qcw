// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwThemeIcons.h"

#include <QByteArray>
#include <QFile>
#include <QGlobalStatic>
#include <QGuiApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QPixmapCache>
#include <QSvgRenderer>

#include <array>
#include <optional>

QT_BEGIN_NAMESPACE

namespace
{

constexpr int kIconCount = 8;

// A pure WindowText stroke reads as harsh black on light palettes and harsh white on
// dark ones; pulling it this far towards the surface colour keeps icons visible without
// making them the loudest thing on a tool bar.
constexpr qreal kSurfaceBlend = 0.28;

constexpr std::array<const char*, kIconCount> kIconFiles {
    "perspective.svg",     "view.svg",           "new_perspective.svg", "new_view.svg",
    "folder_maximize.svg", "folder_restore.svg", "folder_minimize.svg", "tab_list.svg",
};

constexpr int iconIndex(qcw::Icon icon)
{
    return static_cast<int>(icon);
}

const char* iconFile(qcw::Icon icon)
{
    const int index = iconIndex(icon);
    return index >= 0 && index < kIconCount ? kIconFiles[index] : nullptr;
}

QByteArray iconPath(qcw::Icon icon)
{
    const char* file = iconFile(icon);
    return file ? QByteArrayLiteral(":/qcw/icons/") + file : QByteArray();
}

QColor iconColor(const QPalette& palette, QIcon::Mode mode)
{
    QColor color;

    switch (mode)
    {
    case QIcon::Disabled:
        color = palette.color(QPalette::Disabled, QPalette::WindowText);
        break;
    case QIcon::Selected:
        color = palette.color(QPalette::Active, QPalette::HighlightedText);
        break;
    case QIcon::Active:
    case QIcon::Normal:
        color = palette.color(QPalette::Active, QPalette::WindowText);
        break;
    }

    // A custom theme may omit a role; never fall back to black, which would disappear
    // on a dark palette.
    if (!color.isValid())
    {
        color = palette.color(QPalette::Active, QPalette::Text);
    }
    if (!color.isValid())
    {
        color = palette.color(QPalette::Active, QPalette::ButtonText);
    }

    if ((mode == QIcon::Normal || mode == QIcon::Active) && color.isValid())
    {
        const QColor surface = palette.color(QPalette::Active, QPalette::Window);
        if (surface.isValid())
        {
            color = QColor(qRound(color.red() + (surface.red() - color.red()) * kSurfaceBlend),
                           qRound(color.green() + (surface.green() - color.green()) * kSurfaceBlend),
                           qRound(color.blue() + (surface.blue() - color.blue()) * kSurfaceBlend), color.alpha());
        }
    }

    return color;
}

class ThemeIconEngine final : public QIconEngine
{
public:
    explicit ThemeIconEngine(qcw::Icon icon) :
        mIcon { icon }, mSvg { loadSvg(icon) }
    {
        QSvgRenderer probe(mSvg);
        mValid = probe.isValid();
    }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, dpr));
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        const QSize logicalSize = size.expandedTo(QSize(1, 1));
        return renderPixmap(logicalSize, scale > 0.0 ? scale : 1.0, mode, state);
    }

    QSize actualSize(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        Q_UNUSED(mode);
        Q_UNUSED(state);
        return size.expandedTo(QSize(1, 1));
    }

    QList<QSize> availableSizes(QIcon::Mode mode, QIcon::State state) override
    {
        Q_UNUSED(mode);
        Q_UNUSED(state);
        return { QSize(16, 16), QSize(22, 22), QSize(32, 32), QSize(48, 48) };
    }

    QIconEngine* clone() const override
    {
        return new ThemeIconEngine(mIcon);
    }

    QString key() const override
    {
        return QStringLiteral("qcw-theme-icon:") + QString::fromLatin1(iconFile(mIcon));
    }

    bool isNull() override
    {
        return !mValid;
    }

private:
    static QByteArray loadSvg(qcw::Icon icon)
    {
        const QByteArray path = iconPath(icon);
        if (path.isEmpty())
        {
            return {};
        }

        QFile file(QString::fromLatin1(path));
        if (!file.open(QIODevice::ReadOnly))
        {
            qWarning("QcwThemeIcons: cannot open %s", path.constData());
            return {};
        }

        return file.readAll();
    }

    QPixmap renderPixmap(const QSize& size, qreal dpr, QIcon::Mode mode, QIcon::State state) const
    {
        const QColor color = iconColor(QGuiApplication::palette(), mode);
        const int width = qMax(1, qRound(size.width() * dpr));
        const int height = qMax(1, qRound(size.height() * dpr));

        const QString cacheKey = QStringLiteral("qcw-icon/%1/%2/%3x%4/%5/%6/%7")
                                     .arg(QString::fromLatin1(iconFile(mIcon)))
                                     .arg(color.name(QColor::HexRgb))
                                     .arg(width)
                                     .arg(height)
                                     .arg(qRound(dpr * 1000))
                                     .arg(static_cast<int>(mode))
                                     .arg(static_cast<int>(state));

        QPixmap pixmap;
        if (QPixmapCache::find(cacheKey, &pixmap))
        {
            return pixmap;
        }

        pixmap = QPixmap(width, height);
        pixmap.fill(Qt::transparent);
        pixmap.setDevicePixelRatio(dpr);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (mValid)
        {
            QByteArray svg = mSvg;
            svg.replace("currentColor", color.name(QColor::HexRgb).toLatin1());

            QSvgRenderer renderer(svg);
            renderer.render(&painter, QRectF(0, 0, width, height));
        }
        else
        {
            // Keep a visible placeholder instead of silently returning an empty icon.
            const qreal inset = 3.0 * dpr;
            painter.setPen(QPen(color, 1.5 * dpr));
            painter.drawRect(QRectF(inset, inset, width - inset * 2, height - inset * 2));
        }

        painter.end();
        QPixmapCache::insert(cacheKey, pixmap);
        return pixmap;
    }

    qcw::Icon mIcon;
    QByteArray mSvg;
    bool mValid = false;
};

class ThemeIconCache
{
public:
    QIcon icon(qcw::Icon icon)
    {
        const int index = iconIndex(icon);
        if (index < 0 || index >= kIconCount)
        {
            return {};
        }

        auto& cached = mIcons[index];
        if (!cached)
        {
            cached = QIcon(new ThemeIconEngine(icon));
        }

        return *cached;
    }

private:
    std::array<std::optional<QIcon>, kIconCount> mIcons;
};

Q_GLOBAL_STATIC(ThemeIconCache, themeIconCache)

} // namespace

namespace qcw
{

QIcon themeIcon(Icon icon)
{
    return themeIconCache->icon(icon);
}

} // namespace qcw

QT_END_NAMESPACE
