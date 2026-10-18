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

#include "AudioSwitchPolicy.h"

namespace Core::AudioSwitch {

void Policy::OnConnected()
{
    _airPodsId.clear();
    _previousId.reset();
    _handled = false;
    _switched = false;
    _userOverrode = false;
}

std::optional<QString>
Policy::OnAirPodsReady(const QString &airPodsId, const QString &currentDefaultId, bool enabled)
{
    _airPodsId = airPodsId;
    // Only the first time the endpoint is ready in a connection; a flapping endpoint must not
    // undo an output the user picked in the meantime.
    if (_handled || airPodsId.isEmpty()) {
        return std::nullopt;
    }
    _handled = true;

    if (!enabled || currentDefaultId == airPodsId) {
        return std::nullopt;
    }
    _previousId = currentDefaultId.isEmpty() ? std::nullopt : std::optional{currentDefaultId};
    _switched = true;
    return airPodsId;
}

void Policy::OnDefaultChanged(const QString &newDefaultId, bool airPodsActive)
{
    if (_switched && airPodsActive && newDefaultId != _airPodsId) {
        _userOverrode = true;
    }
}

std::optional<QString>
Policy::OnDisconnected(const QString &currentDefaultId, bool previousActive)
{
    std::optional<QString> restore;
    if (_switched && !_userOverrode && _previousId.has_value() && previousActive &&
        currentDefaultId != *_previousId)
    {
        restore = _previousId;
    }
    OnConnected(); // forget this connection
    return restore;
}

const std::optional<QString> &Policy::Previous() const
{
    return _previousId;
}

} // namespace Core::AudioSwitch
