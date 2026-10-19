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

#include "TrayBatteryHeader.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include "../Theme.h"
#include "../TintedSvg.h"

namespace Gui::Widget {
namespace {

constexpr auto kStatusIcon = ":/Resource/Image/Menu/BluetoothOff.svg";

void ApplyBattery(BatteryRow *row, const Core::AirPods::Details::BasicState &state)
{
    row->SetValue(
        state.battery.Available() ? std::optional<BatteryRow::ValueType>{state.battery.Value()}
                                  : std::nullopt);
    row->SetCharging(state.battery.Available() && state.isCharging);
}

} // namespace

TrayBatteryHeader::TrayBatteryHeader(QWidget *parent)
    : QWidget{parent}, _name{new QLabel{this}}, _rows{new QWidget{this}},
      _left{new BatteryRow{_rows}}, _right{new BatteryRow{_rows}}, _case{new BatteryRow{_rows}},
      _status{new QWidget{this}}, _statusIcon{new QLabel{_status}}, _statusText{new QLabel{_status}}
{
    setObjectName("trayBatteryHeader");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);

    auto nameFont = _name->font();
    nameFont.setPixelSize(13);
    nameFont.setWeight(QFont::DemiBold);
    _name->setFont(nameFont);
    _name->setTextFormat(Qt::PlainText);
    _name->setObjectName("trayBatteryHeaderName");

    auto rowFont = font();
    rowFont.setPixelSize(12);
    rowFont.setWeight(QFont::Medium);

    auto *rowsLayout = new QVBoxLayout{_rows};
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(8);
    const std::pair<BatteryRow *, const char *> rows[] = {
        {_left, "trayLeftBatteryRow"}, {_right, "trayRightBatteryRow"}, {_case, "trayCaseBatteryRow"}};
    for (const auto &[row, name] : rows) {
        row->setObjectName(name);
        row->setFont(rowFont);
        row->SetColumns(40, 48, 10);
        rowsLayout->addWidget(row);
    }

    _statusText->setFont(rowFont);
    _statusText->setObjectName("trayBatteryHeaderStatus");
    auto *statusLayout = new QHBoxLayout{_status};
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(8);
    statusLayout->addWidget(_statusIcon);
    statusLayout->addWidget(_statusText, 1);

    auto *layout = new QVBoxLayout{this};
    layout->setContentsMargins(10, 10, 10, 12);
    layout->setSpacing(10);
    layout->addWidget(_name);
    layout->addWidget(_rows);
    layout->addWidget(_status);

    _left->SetLabel(tr("Left"));
    _right->SetLabel(tr("Right"));
    _case->SetLabel(tr("Case"));

    ApplyTheme();
    SetStatus({});
}

void TrayBatteryHeader::SetState(const Core::AirPods::State &state)
{
    _name->setText(state.displayName);
    _name->show();
    ApplyBattery(_left, state.pods.left);
    ApplyBattery(_right, state.pods.right);
    ApplyBattery(_case, state.caseBox);
    _rows->show();
    _status->hide();
}

void TrayBatteryHeader::SetStatus(const QString &status)
{
    _name->hide();
    _rows->hide();
    _statusText->setText(status);
    _status->show();
}

bool TrayBatteryHeader::IsShowingStatus() const
{
    return !_status->isHidden();
}

void TrayBatteryHeader::ApplyTheme()
{
    const auto &colors = Theme::Manager::Instance().Colors();

    auto palette = this->palette();
    palette.setColor(QPalette::WindowText, colors.text);
    _name->setPalette(palette);
    palette.setColor(QPalette::WindowText, colors.textSecondary);
    _statusText->setPalette(palette);

    _statusIcon->setPixmap(QPixmap::fromImage(
        TintedSvgImage(kStatusIcon, colors.textSecondary, 14, devicePixelRatioF())));

    for (auto *row : {_left, _right, _case}) {
        row->SetLabelColor(colors.text);
        row->SetValueColor(colors.text);
        row->SetTrackColor(colors.batteryTrack);
        row->SetNormalColor(colors.batteryNormal);
        row->SetLowColor(colors.batteryAlarm);
    }
}

} // namespace Gui::Widget
