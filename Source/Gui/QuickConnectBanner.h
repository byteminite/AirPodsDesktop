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

#include <QImage>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

#include "../Core/AirPods.h"
#include "../Core/QuickConnect.h"
#include "Widget/BatteryRow.h"

namespace Gui {

// What a quick-connect banner says (F005). Only results and problems produce one.
struct QuickConnectBannerContent {
    enum class Kind { AirPods, Device, Problem };

    Kind kind{Kind::Problem};
    QString title;
    QString text;
    // Set for `Kind::AirPods`: the bound AirPods' state, for the picture and battery.
    std::optional<Core::AirPods::State> airPods;
    // Clicking opens Settings (to choose a device) instead of the popup.
    bool opensSettings{false};

    bool operator==(const QuickConnectBannerContent &rhs) const = default;
};

// Maps a quick-connect outcome to banner content, or nullopt when it should stay silent
// ("Connecting...", "already connected", "disabled"). `airPods` is the current state of the bound
// AirPods, if they are reporting; it is used only when it names the device that connected.
std::optional<QuickConnectBannerContent> QuickConnectBannerContentFor(
    Core::QuickConnect::Outcome outcome, const QString &deviceName,
    const std::optional<Core::AirPods::State> &airPods);

// A small frosted banner above the taskbar that reports a quick-connect result, in the popup's
// visual language. It never takes focus and hides by itself; hovering keeps it up.
class QuickConnectBanner : public QWidget
{
    Q_OBJECT

public:
    QuickConnectBanner();

    void Show(const QuickConnectBannerContent &content);
    // Live battery for an AirPods banner on screen; ignored otherwise.
    void UpdateAirPods(const Core::AirPods::State &state);
    void Dismiss();

    const std::optional<QuickConnectBannerContent> &Content() const;

Q_SIGNALS:
    void Clicked(bool opensSettings);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    static constexpr int kWidth = 340;
    static constexpr int kScreenGap = 12;
    static constexpr qreal kCornerRadius = 8.0;

    std::optional<QuickConnectBannerContent> _content;
    QImage _backdrop;
    // The frosted backdrop is much darker or lighter than the theme, e.g. a light theme over a
    // dark window: the glass is then tinted more heavily so the text stays readable.
    bool _backdropFightsTheme{false};
    QLabel *_leading;
    QLabel *_title;
    QLabel *_statusDot;
    QLabel *_text;
    QWidget *_rows;
    Widget::BatteryRow *_left, *_right, *_case;
    QTimer _hideTimer;
    QParallelAnimationGroup _showAnimation;
    QPropertyAnimation *_slide;
    QPropertyAnimation *_fadeIn;
    QPropertyAnimation _fadeOut;

    void Render();
    void ApplyBattery(const Core::AirPods::State &state);
    int HideDelay() const;
};

} // namespace Gui
