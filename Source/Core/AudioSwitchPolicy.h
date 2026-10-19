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

#include <optional>

#include <QString>

namespace Core::AudioSwitch {

// Decides when to make the AirPods the Windows default output, and when to put the previous
// output back (F008). Pure logic over endpoint ids; the Windows side reports what happens and
// carries out the returned `SetDefault` ids.
//
// - Once per connection, when the AirPods' stereo endpoint is ready: if it isn't the default
//   already, switch to it and remember what was the default.
// - While the AirPods endpoint is still active, any other default is the user's own choice; it is
//   never fought, and it cancels the restore. A change after the endpoint went away is Windows
//   falling back and doesn't count.
// - On disconnect: restore the remembered output if we switched, the user didn't override, it is
//   still active, and it isn't the default already.
class Policy
{
public:
    void OnConnected();
    std::optional<QString>
    OnAirPodsReady(const QString &airPodsId, const QString &currentDefaultId, bool enabled);
    void OnDefaultChanged(const QString &newDefaultId, bool airPodsActive);
    std::optional<QString> OnDisconnected(const QString &currentDefaultId, bool previousActive);

    const std::optional<QString> &Previous() const;

private:
    QString _airPodsId;
    std::optional<QString> _previousId;
    bool _handled{false};
    bool _switched{false};
    bool _userOverrode{false};
};

} // namespace Core::AudioSwitch
