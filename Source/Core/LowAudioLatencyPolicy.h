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

namespace Core::LowAudioLatency {

// Decides *when* the keep-alive stream should open or close. Kept free of Qt Multimedia so it can
// be driven deterministically in tests without a real audio backend; `Controller` owns the actual
// `QMediaPlayer` and the timers, and just executes whatever `Action` each event returns.
//
// The point of this split: the Bluetooth "connected" notification arrives before Windows has
// finished rebuilding the AirPods' A2DP endpoint, so opening the stream immediately on connect
// races the endpoint's own setup (and anyone else's). A short settle delay avoids that race. The
// same reasoning applies in reverse to brief disconnects - tearing the stream down and reopening it
// on every blip adds more churn right when the endpoint is already unstable, and throws away the
// low-latency state for no reason - so a short grace period rides out blips instead.
class Policy
{
public:
    enum class Action {
        None,          // nothing changed; leave any pending timer running as is
        CancelPending, // a settle/grace/retry wait was abandoned; no stream change
        Start,
        Release,
        ScheduleSettle,
        ScheduleGrace,
        ScheduleFastRetry,
        ScheduleSlowRetry,
    };

    Action SetEnabled(bool enabled);
    Action SetDeviceConnected(bool connected);
    Action OnSettleElapsed();
    Action OnGraceElapsed();
    Action OnPlaybackError();
    Action OnRetryElapsed();

private:
    enum class State {
        Idle,
        Settling,
        Running,
        Grace,
        RetryPending,
    };

    bool _enabled{false};
    bool _connected{false};
    bool _fastRetryUsed{false};
    State _state{State::Idle};
};

} // namespace Core::LowAudioLatency
