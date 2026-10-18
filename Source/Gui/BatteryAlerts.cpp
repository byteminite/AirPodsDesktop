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

#include "BatteryAlerts.h"

#include <algorithm>

namespace Gui {
namespace {

using ValueType = Core::AirPods::Battery::ValueType;

constexpr ValueType kFull = 100;
// Readings come in 10-point steps; re-arm two steps above a threshold so one-step flapping
// between, say, 10% and 20% never repeats a warning.
constexpr ValueType kReArmMargin = 20;

} // namespace

std::optional<ValueType>
BatteryAlertPolicy::TrackLow(PodTrack &track, const Core::AirPods::PodState &pod)
{
    if (!pod.battery.Available()) {
        return std::nullopt;
    }
    const auto value = pod.battery.Value();

    bool crossed = false;
    for (size_t i = 0; i < kLowThresholds.size(); ++i) {
        const auto threshold = kLowThresholds[i];
        if (pod.isCharging || value >= threshold + kReArmMargin) {
            track.announced[i] = false;
        }
        if (!pod.isCharging && value <= threshold && !track.announced[i]) {
            track.announced[i] = true;
            crossed = true;
        }
    }
    return crossed ? std::optional{value} : std::nullopt;
}

bool BatteryAlertPolicy::TrackCharged(
    ChargeTrack &track, std::optional<ValueType> level, bool charging)
{
    if (!level.has_value()) {
        return false;
    }
    if (*level < kFull) {
        track.belowFull = true;
        return false;
    }
    if (charging && track.belowFull) {
        track.belowFull = false;
        return true;
    }
    return false;
}

std::optional<BatteryAlert>
BatteryAlertPolicy::OnState(const Core::AirPods::State &state, bool lowEnabled, bool chargedEnabled)
{
    // Track every side on every state, whatever is enabled, so the bookkeeping never lags.
    const auto left = TrackLow(_left, state.pods.left);
    const auto right = TrackLow(_right, state.pods.right);

    std::optional<ValueType> podsLevel;
    bool podsCharging = true;
    for (const auto *pod : {&state.pods.left, &state.pods.right}) {
        if (pod->battery.Available()) {
            podsLevel = (std::min)(podsLevel.value_or(kFull), pod->battery.Value());
            podsCharging = podsCharging && pod->isCharging;
        }
    }
    const bool podsCharged = TrackCharged(_pods, podsLevel, podsCharging);

    const auto &caseBox = state.caseBox;
    const bool caseCharged = TrackCharged(
        _case,
        caseBox.battery.Available() ? std::optional{caseBox.battery.Value()} : std::nullopt,
        caseBox.isCharging);

    if (lowEnabled && (left.has_value() || right.has_value())) {
        if (left.has_value() && right.has_value()) {
            return BatteryAlert{
                BatteryAlert::Kind::Low, BatteryAlert::Side::BothPods, (std::min)(*left, *right)};
        }
        return left.has_value()
                   ? BatteryAlert{BatteryAlert::Kind::Low, BatteryAlert::Side::Left, *left}
                   : BatteryAlert{BatteryAlert::Kind::Low, BatteryAlert::Side::Right, *right};
    }
    if (chargedEnabled && podsCharged) {
        return BatteryAlert{BatteryAlert::Kind::Charged, BatteryAlert::Side::BothPods, kFull};
    }
    if (chargedEnabled && caseCharged) {
        return BatteryAlert{BatteryAlert::Kind::Charged, BatteryAlert::Side::Case, kFull};
    }
    return std::nullopt;
}

} // namespace Gui
