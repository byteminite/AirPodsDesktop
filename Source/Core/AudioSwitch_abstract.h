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

#include <QString>

namespace Core::AudioSwitch::Details {

// Moves the default sound output to the bound AirPods while they are connected (F008). All
// methods are called from the GUI thread and return immediately; the work happens elsewhere.
class SwitcherAbstract
{
public:
    virtual ~SwitcherAbstract() = default;

    virtual void SetEnabled(bool enabled) = 0;
    // The Bluetooth container id of the bound AirPods; empty when nothing is bound.
    virtual void SetBoundContainerId(const QString &containerId) = 0;
    virtual void SetDeviceConnected(bool connected) = 0;
};

} // namespace Core::AudioSwitch::Details
