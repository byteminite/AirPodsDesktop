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

#include "LowAudioLatency.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QUrl>

#include "../Logger.h"

namespace Core::LowAudioLatency {

Controller::Controller(QObject *parent) : QObject{parent}
{
    connect(this, &Controller::ControlSafely, this, &Controller::Control);
    connect(this, &Controller::SetDeviceConnectedSafely, this, &Controller::SetDeviceConnected);

    // Retries creating the media backend while no audio output is enumerated yet (issue #20). This
    // is independent of the settle/grace/retry timing below, which governs *when* to reach for an
    // endpoint that Qt already knows about.
    _initTimer.setInterval(kRetryInterval);
    _initTimer.callOnTimeout([this] { StartWhenReady(); });

    _settleTimer.setSingleShot(true);
    _settleTimer.setInterval(kSettleDelay);
    connect(&_settleTimer, &QTimer::timeout, this, [this] { Execute(_policy.OnSettleElapsed()); });

    _graceTimer.setSingleShot(true);
    _graceTimer.setInterval(kGraceDelay);
    connect(&_graceTimer, &QTimer::timeout, this, [this] { Execute(_policy.OnGraceElapsed()); });

    _retryTimer.setSingleShot(true);
    connect(&_retryTimer, &QTimer::timeout, this, [this] { Execute(_policy.OnRetryElapsed()); });
}

Controller::~Controller()
{
    _settleTimer.stop();
    _graceTimer.stop();
    _retryTimer.stop();
    ResetAudio();
}

bool Controller::Initialize()
{
    // issue #20
    //
    // Constructing `QMediaPlayer` when no audio output device is enabled will cause `play` to
    // continually raise errors and is unrecoverable until the device list changes.
    if (QMediaDevices::audioOutputs().empty()) {
        LOG(Warn, "LowAudioLatency: Try to init, but no audio output device is enabled.");
        return false;
    }

    _mediaPlayer = std::make_unique<QMediaPlayer>();
    _audioOutput = std::make_unique<QAudioOutput>();

    connect(
        _mediaPlayer.get(), &QMediaPlayer::errorOccurred, this,
        [this](QMediaPlayer::Error error, const QString &) { OnError(error); });

    _mediaPlayer->setAudioOutput(_audioOutput.get());
    _mediaPlayer->setSource(QUrl{"qrc:/Resource/Audio/Silence.mp3"});
    _mediaPlayer->setLoops(QMediaPlayer::Infinite);

    _inited = true;
    LOG(Info, "LowAudioLatency: Compatible media backend initialized.");
    return true;
}

bool Controller::StartWhenReady()
{
    if (!_inited && !Initialize()) {
        if (!_initTimer.isActive()) {
            _initTimer.start();
        }
        return false;
    }

    _initTimer.stop();
    Start();
    return true;
}

void Controller::ResetAudio()
{
    // The only teardown path, so it also disarms the "no audio output enumerated yet" retry -
    // nothing should keep polling once the feature no longer wants a stream.
    _initTimer.stop();
    Stop();
    _mediaPlayer.reset();
    _audioOutput.reset();
    _inited = false;
}

void Controller::Control(bool enable)
{
    LOG(Info, "LowAudioLatency::Controller Control: {}, _inited: {}", enable, _inited);

    const auto action = _policy.SetEnabled(enable);
    if (enable && action == Policy::Action::None) {
        LOG(Info, "LowAudioLatency: Waiting for the bound device to connect.");
    }
    Execute(action);
}

void Controller::SetDeviceConnected(bool connected)
{
    Execute(_policy.SetDeviceConnected(connected));
}

void Controller::Execute(Policy::Action action)
{
    if (action == Policy::Action::None) {
        return;
    }

    // Only one of settle/grace/retry is ever meaningful at a time; stop whichever is now stale
    // before acting on the new decision.
    _settleTimer.stop();
    _graceTimer.stop();
    _retryTimer.stop();

    switch (action) {
    case Policy::Action::None:
        break;
    case Policy::Action::CancelPending:
        LOG(Info, "LowAudioLatency: Reconnected in time; keeping the stream as is.");
        break;
    case Policy::Action::Start:
        StartWhenReady();
        break;
    case Policy::Action::Release:
        LOG(Info, "LowAudioLatency: Releasing the audio stream.");
        ResetAudio();
        break;
    case Policy::Action::ScheduleSettle:
        LOG(Info, "LowAudioLatency: Waiting for the endpoint to settle before starting.");
        _settleTimer.start();
        break;
    case Policy::Action::ScheduleGrace:
        LOG(Info, "LowAudioLatency: Riding out a disconnect before releasing the stream.");
        _graceTimer.start();
        break;
    case Policy::Action::ScheduleFastRetry:
        LOG(Info, "LowAudioLatency: Retrying shortly after a playback error.");
        _retryTimer.setInterval(kSettleDelay);
        _retryTimer.start();
        break;
    case Policy::Action::ScheduleSlowRetry:
        LOG(Info, "LowAudioLatency: Retrying later after a playback error.");
        _retryTimer.setInterval(kRetryInterval);
        _retryTimer.start();
        break;
    }
}

void Controller::Start()
{
    if (!_inited || !_mediaPlayer) {
        return;
    }

    if (_mediaPlayer->playbackState() != QMediaPlayer::PlayingState) {
        _mediaPlayer->play();
    }
}

void Controller::Stop()
{
    if (_mediaPlayer) {
        _mediaPlayer->stop();
    }
}

void Controller::OnError(QMediaPlayer::Error error)
{
    LOG(Warn, "LowAudioLatency::Controller error: {}. Reinit later.", error);

    // Do not destroy QMediaPlayer while it is emitting its error signal.
    const auto *failedPlayer = _mediaPlayer.get();
    QTimer::singleShot(0, this, [this, failedPlayer] {
        if (_mediaPlayer.get() != failedPlayer) {
            return;
        }

        const auto action = _policy.OnPlaybackError();
        if (action == Policy::Action::None) {
            // The policy no longer considers the stream running (e.g. a disconnect already moved
            // it into the grace period) - leave the player alone rather than tearing it down.
            return;
        }

        ResetAudio();
        Execute(action);
    });
}

} // namespace Core::LowAudioLatency
