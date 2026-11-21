// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/svgpixmap.h"

#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

#include <map>
#include <memory>

QString picturePath(const QString &picture)
{
    return QStringLiteral(":/svg/devices/") + picture;
}

QPixmap svgPixmap(const QString &path, int height, qreal devicePixelRatio)
{
    // QSvgRenderer warns about a missing file; callers only need the null result.
    if (!QFile::exists(path))
        return {};
    QSvgRenderer renderer(path);
    if (!renderer.isValid())
        return {};

    const QRectF box = renderer.viewBoxF();
    const QSize size(qRound(height * box.width() / box.height()), height);
    QPixmap pixmap(size * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(QPointF(0, 0), size));
    return pixmap;
}

QIcon svgIcon(const QString &path, int size)
{
    QIcon icon;
    icon.addPixmap(svgPixmap(path, size));
    icon.addPixmap(svgPixmap(path, size, 2.0));
    return icon;
}

QSvgRenderer &svgRenderer(const QString &path)
{
    static std::map<QString, std::unique_ptr<QSvgRenderer>> renderers;
    std::unique_ptr<QSvgRenderer> &renderer = renderers[path];
    if (!renderer)
        renderer = std::make_unique<QSvgRenderer>(path);
    return *renderer;
}
