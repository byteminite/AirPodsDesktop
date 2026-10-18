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

#include <chrono>
#include <cstdint>
#include <optional>

namespace Core::EarDetection {

// When taking earbuds out pauses media (F007).
enum class PauseMode : uint32_t {
    OneEarbud,  // any earbud coming out pauses; putting it back resumes (like iPhone)
    BothEarbuds // only the last earbud coming out pauses; any earbud back in resumes
};

enum class Action { None, Pause, Resume, Forget };

// Decides what automatic ear detection does from successive in-ear readings. Pure logic: the
// caller passes the time, so the resume window is tested without waiting.
//
// It counts the earbuds that are in, so it works for one-earbud listening too:
// - OneEarbud: the count dropping pauses, and climbing back to the count at the pause resumes.
// - BothEarbuds: the count reaching 0 pauses, and any earbud back in resumes.
// Going back in later than `kResumeWindow` after the pause gives `Forget` instead of `Resume`:
// nothing restarts, and the paused players are dropped. A side without a reading keeps its last
// known value, so reporting gaps never look like a removal. The first reading after `Reset` only
// records the starting point.
class Policy
{
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto kResumeWindow = std::chrono::minutes{10};

    Action OnState(
        std::optional<bool> leftInEar, std::optional<bool> rightInEar, PauseMode mode,
        Clock::time_point now);

    // A fresh connection: start over. Returns `Forget` if a pause was still waiting for the
    // earbuds, so the paused players aren't resumed by a much later insertion.
    Action Reset();

private:
    std::optional<bool> _left, _right;
    std::optional<int> _count;
    // The earbud count just before the pause; set while a pause waits for the earbuds.
    std::optional<int> _pausedFromCount;
    Clock::time_point _pausedAt{};
};

} // namespace Core::EarDetection
