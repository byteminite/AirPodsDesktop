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

#include <QLabel>
#include <QWidget>

#include "../../Core/AirPods.h"
#include "BatteryRow.h"

namespace Gui::Widget {

// The read-only battery summary at the top of the tray menu (F003): the device name plus
// Left / Right / Case rows, or a single dimmed status line when no device is connected.
class TrayBatteryHeader : public QWidget
{
    Q_OBJECT

public:
    explicit TrayBatteryHeader(QWidget *parent = nullptr);

    void SetState(const Core::AirPods::State &state);
    void SetStatus(const QString &status);
    void ApplyTheme();

    bool IsShowingStatus() const;

private:
    QLabel *_name;
    QWidget *_rows;
    BatteryRow *_left;
    BatteryRow *_right;
    BatteryRow *_case;
    QWidget *_status;
    QLabel *_statusIcon;
    QLabel *_statusText;
};

} // namespace Gui::Widget
