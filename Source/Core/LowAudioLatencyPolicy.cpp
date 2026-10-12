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

#include "LowAudioLatencyPolicy.h"

namespace Core::LowAudioLatency {

Policy::Action Policy::SetEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return Action::None;
    }
    _enabled = enabled;

    if (!enabled) {
        const bool wasActive = _state != State::Idle;
        _state = State::Idle;
        _fastRetryUsed = false;
        return wasActive ? Action::Release : Action::None;
    }

    if (!_connected) {
        return Action::None;
    }

    // Already connected while the feature was off, so the endpoint is already stable - no need to
    // wait out a settle delay that exists only to cover the moment right after a fresh connect.
    _state = State::Running;
    _fastRetryUsed = false;
    return Action::Start;
}

Policy::Action Policy::SetDeviceConnected(bool connected)
{
    if (_connected == connected) {
        return Action::None;
    }
    _connected = connected;

    if (!_enabled) {
        _state = State::Idle;
        return Action::None;
    }

    if (connected) {
        if (_state == State::Grace) {
            // Reconnected within the grace window - the stream was never released, so just cancel
            // the pending release and keep it running.
            _state = State::Running;
            return Action::CancelPending;
        }
        _state = State::Settling;
        return Action::ScheduleSettle;
    }

    switch (_state) {
    case State::Settling:
        // Disconnected before the settle delay elapsed; nothing was ever started.
        _state = State::Idle;
        return Action::CancelPending;
    case State::RetryPending:
        // Dropped again before the retry could run; abandon it rather than retry a device that
        // is no longer there.
        _state = State::Idle;
        return Action::CancelPending;
    case State::Running:
        _state = State::Grace;
        return Action::ScheduleGrace;
    case State::Idle:
    case State::Grace:
        _state = State::Idle;
        return Action::None;
    }
    return Action::None;
}

Policy::Action Policy::OnSettleElapsed()
{
    if (_state != State::Settling) {
        return Action::None;
    }
    _state = State::Running;
    _fastRetryUsed = false;
    return Action::Start;
}

Policy::Action Policy::OnGraceElapsed()
{
    if (_state != State::Grace) {
        return Action::None;
    }
    _state = State::Idle;
    return Action::Release;
}

Policy::Action Policy::OnPlaybackError()
{
    if (_state != State::Running) {
        return Action::None;
    }
    _state = State::RetryPending;

    if (!_fastRetryUsed) {
        _fastRetryUsed = true;
        return Action::ScheduleFastRetry;
    }
    return Action::ScheduleSlowRetry;
}

Policy::Action Policy::OnRetryElapsed()
{
    if (_state != State::RetryPending) {
        return Action::None;
    }
    if (!_enabled || !_connected) {
        _state = State::Idle;
        return Action::None;
    }
    _state = State::Running;
    _fastRetryUsed = false;
    return Action::Start;
}

} // namespace Core::LowAudioLatency
