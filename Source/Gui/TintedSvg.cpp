//
// AirPodsDesktop - AirPods Desktop User Experience Enhancement Program.
// Copyright (C) 2021-2022 SpriteOvO
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "TintedSvg.h"

#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace Gui {

QImage TintedSvgImage(const QString &resource, const QColor &color, int size, qreal devicePixelRatio)
{
    const int pixels = qRound(size * devicePixelRatio);
    QImage image{pixels, pixels, QImage::Format_ARGB32_Premultiplied};
    image.fill(Qt::transparent);

    QPainter painter{&image};
    painter.setRenderHint(QPainter::Antialiasing);
    QSvgRenderer{resource}.render(&painter);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), color);
    painter.end();

    image.setDevicePixelRatio(devicePixelRatio);
    return image;
}

QIcon TintedSvgIcon(const QString &resource, const QColor &color, int size)
{
    QIcon icon;
    for (const qreal ratio : {1.0, 2.0}) {
        icon.addPixmap(QPixmap::fromImage(TintedSvgImage(resource, color, size, ratio)));
    }
    return icon;
}

} // namespace Gui
