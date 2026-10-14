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

#include <QImage>

namespace Gui {

// Turns a snapshot of whatever sits behind the popup into its frosted-glass background: a heavy
// blur plus a saturation boost, the same recipe as CSS `blur() saturate(180%)` in the approved
// mockup. Done in software because none of the Windows blur APIs work on the popup's layered
// (per-pixel alpha) window, and the system backdrop materials only render for focused windows.
// Returns an opaque image of the same size, or a null image for a null input.
QImage FrostBackdrop(const QImage &source);

} // namespace Gui
