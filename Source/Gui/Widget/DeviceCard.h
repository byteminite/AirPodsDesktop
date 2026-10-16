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

#include <QBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>

#include "../../Core/AirPods.h"
#include "../Base.h"
#include "BatteryRow.h"

namespace Gui::Widget {

// What the Settings window knows about the bound AirPods, fed from the same signals as the popup.
struct DeviceSnapshot {
    Status status{Status::Unavailable};
    std::optional<Core::AirPods::State> state;
};

// The AirPods hero card at the top of the Settings "AirPods" page (F004): picture, name, status,
// Left / Right / Case battery, and the Unbind / Bind to AirPods button.
class DeviceCard : public QFrame
{
    Q_OBJECT

public:
    explicit DeviceCard(QWidget *parent = nullptr);

    void SetSnapshot(const DeviceSnapshot &snapshot);
    QPushButton *Button() const;

Q_SIGNALS:
    void UnbindRequested();
    void BindRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    static constexpr int kCompactWidth = 560;

    DeviceSnapshot _snapshot;
    bool _compact{false};
    QHBoxLayout *_layout{};
    QVBoxLayout *_textLayout{};
    QLabel *_picture;
    QLabel *_name;
    QLabel *_statusDot;
    QLabel *_status;
    QLabel *_hint;
    QWidget *_rows;
    BatteryRow *_left;
    BatteryRow *_right;
    BatteryRow *_case;
    QPushButton *_button;

    void Render();
    void ApplyTheme();
};

// The compact device block at the top of the Settings sidebar: a round picture, name and status.
class DeviceBadge : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceBadge(QWidget *parent = nullptr);

    void SetSnapshot(const DeviceSnapshot &snapshot);

protected:
    void changeEvent(QEvent *event) override;

private:
    DeviceSnapshot _snapshot;
    QLabel *_picture;
    QLabel *_name;
    QLabel *_status;

    void Render();
};

// Shared by the card and the badge so they always agree.
QString DeviceDisplayName(const DeviceSnapshot &snapshot);
QString DeviceStatusText(const DeviceSnapshot &snapshot);
QImage DevicePicture(const DeviceSnapshot &snapshot);

} // namespace Gui::Widget
