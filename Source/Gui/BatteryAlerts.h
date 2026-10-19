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

#include <array>
#include <optional>

#include "../Core/AirPods.h"

namespace Gui {

// A low battery warning or a "fully charged" notice (F006).
struct BatteryAlert {
    enum class Kind { Low, Charged };
    enum class Side { Left, Right, BothPods, Case };

    Kind kind{Kind::Low};
    Side side{Side::Left};
    // The reading that triggered it, 0-100.
    Core::AirPods::Battery::ValueType percent{};

    bool operator==(const BatteryAlert &rhs) const = default;
};

// Decides, from each new AirPods state, whether a battery alert is due. Pure logic: no timers, no
// widgets, so every rule is unit-tested.
//
// - Low: an earbud reading at or below 20%, and again at or below 10%. Each threshold fires once
//   per discharge and re-arms when the earbud charges, or reads two steps (20 points) above it, so
//   a reading flapping between 10% and 20% doesn't repeat. Both earbuds crossing in one reading
//   give one `BothPods` alert. The first reading counts: connecting at 10% warns at once. The case
//   never gets low warnings.
// - Charged: the earbuds (all reporting ones) or the case reach 100% while charging, after having
//   read below 100%. A first reading that is already full stays silent.
// - A side that isn't reporting changes nothing, so a reconnect doesn't replay alerts.
// - A disabled kind still tracks state, so enabling it later doesn't replay old crossings.
// - At most one alert per state: low before charged, earbuds before the case.
class BatteryAlertPolicy
{
public:
    static constexpr std::array<Core::AirPods::Battery::ValueType, 2> kLowThresholds{20, 10};

    std::optional<BatteryAlert>
    OnState(const Core::AirPods::State &state, bool lowEnabled, bool chargedEnabled);

private:
    struct PodTrack {
        std::array<bool, kLowThresholds.size()> announced{};
    };
    struct ChargeTrack {
        bool belowFull{false};
    };

    PodTrack _left, _right;
    ChargeTrack _pods, _case;

    // Returns the reading if this pod crossed a not-yet-announced threshold, and records it.
    static std::optional<Core::AirPods::Battery::ValueType>
    TrackLow(PodTrack &track, const Core::AirPods::PodState &pod);
    // True when the side just became full while charging.
    static bool TrackCharged(
        ChargeTrack &track, std::optional<Core::AirPods::Battery::ValueType> level, bool charging);
};

} // namespace Gui
