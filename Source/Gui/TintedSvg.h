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

#pragma once

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QString>

namespace Gui {

// Renders a single-colour line icon (drawn in black) at `size` x `size` logical pixels and recolours
// every opaque pixel to `color`, so one SVG serves both light and dark themes.
QImage TintedSvgImage(const QString &resource, const QColor &color, int size, qreal devicePixelRatio);

// The same, wrapped as an icon with 1x and 2x renditions.
QIcon TintedSvgIcon(const QString &resource, const QColor &color, int size);

} // namespace Gui
