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

#include "EarDetection.h"

namespace Core::EarDetection {

Action Policy::OnState(
    std::optional<bool> leftInEar, std::optional<bool> rightInEar, PauseMode mode,
    Clock::time_point now)
{
    if (leftInEar.has_value()) {
        _left = leftInEar;
    }
    if (rightInEar.has_value()) {
        _right = rightInEar;
    }
    if (!_left.has_value() && !_right.has_value()) {
        return Action::None;
    }

    const int count = (_left.value_or(false) ? 1 : 0) + (_right.value_or(false) ? 1 : 0);
    const auto previous = _count;
    _count = count;
    if (!previous.has_value() || count == *previous) {
        return Action::None;
    }

    if (_pausedFromCount.has_value()) {
        const bool back =
            mode == PauseMode::BothEarbuds ? count >= 1 : count >= *_pausedFromCount;
        if (!back) {
            return Action::None;
        }
        _pausedFromCount.reset();
        return now - _pausedAt > kResumeWindow ? Action::Forget : Action::Resume;
    }

    const bool removed = mode == PauseMode::BothEarbuds ? count == 0 && *previous > 0
                                                        : count < *previous;
    if (!removed) {
        return Action::None;
    }
    _pausedFromCount = *previous;
    _pausedAt = now;
    return Action::Pause;
}

Action Policy::Reset()
{
    const bool waiting = _pausedFromCount.has_value();
    _left.reset();
    _right.reset();
    _count.reset();
    _pausedFromCount.reset();
    return waiting ? Action::Forget : Action::None;
}

} // namespace Core::EarDetection
