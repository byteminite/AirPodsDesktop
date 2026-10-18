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

#if !defined APD_OS_WIN
    #error "This file shouldn't be compiled."
#endif

#include <QObject>
#include <QThread>

#include "AudioSwitch_abstract.h"

namespace Core::AudioSwitch {

namespace Details {
class Worker;
} // namespace Details

// Windows implementation: MMDevice endpoints matched by container id, and the default output set
// through `IPolicyConfig` (no public API exists for it). Everything runs on a dedicated COM thread.
class Switcher final : public QObject, public Details::SwitcherAbstract
{
    Q_OBJECT

public:
    explicit Switcher(QObject *parent = nullptr);
    ~Switcher() override;

    void SetEnabled(bool enabled) override;
    void SetBoundContainerId(const QString &containerId) override;
    void SetDeviceConnected(bool connected) override;

Q_SIGNALS:
    // The default output was just moved (to the AirPods or back); emitted on the GUI thread.
    void DefaultOutputSwitched();

private:
    QThread _thread;
    Details::Worker *_worker;
};

} // namespace Core::AudioSwitch
