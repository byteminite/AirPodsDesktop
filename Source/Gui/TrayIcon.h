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

#include <memory>

#include <functional>

#include <QSystemTrayIcon>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QWidgetAction>

#include "../Core/AirPods.h"
#include "../Core/QuickConnect.h"
#include "../Core/Update.h"
#include "Base.h"
#include "TrayActivation.h"
#include "SettingsWindow.h"
#include "TrayMenu.h"
#include "QuickConnectBanner.h"
#include "Widget/TrayBatteryHeader.h"

namespace Gui {

class TrayIcon : public QWidget
{
    Q_OBJECT

public:
    explicit TrayIcon(
        std::function<int()> getCurrentLocaleIndex, Core::QuickConnect::Controller &quickConnect);

    template <class... ArgsT>
    inline void ShowMessage(ArgsT &&...args)
    {
        _tray->showMessage(std::forward<ArgsT>(args)...);
    }

    void UpdateState(const Core::AirPods::State &state);
    void Unavailable();
    void Disconnect();
    void Unbind();
    void VersionUpdateAvailable(const Core::Update::ReleaseInfo &releaseInfo);
    // The popup already shows the AirPods; no quick-connect banner appears beside it.
    void SetPopupVisible(bool visible);
    void ShowContextMenu(const QPoint &position);

Q_SIGNALS:
    void OnTrayIconBatteryChangedSafely(Core::Settings::TrayIconBatteryBehavior value);
    void OnLowAudioLatencyChangedSafely(bool enabled);
    void ShowMainWindowRequested();
    void UserUpdateRequested(const Core::Update::ReleaseInfo &releaseInfo);
    void ToolTipChanged(const QString &text);

private:
    struct IconRenderState {
        std::optional<Core::AirPods::Battery::ValueType> battery;
        bool showUpdateDot{false};
        // The glyph follows the taskbar, i.e. the Windows *system* theme, not the app's mode.
        bool systemDark{true};

        bool operator==(const IconRenderState &rhs) const = default;
    };

    QSystemTrayIcon *_tray = new QSystemTrayIcon{this};
    TrayMenu *_menu = new TrayMenu{this};
    Widget::TrayBatteryHeader *_header = new Widget::TrayBatteryHeader;
    QWidgetAction *_headerAction = new QWidgetAction{this};
    QAction *_actionNewVersion = new QAction{tr("New version available!"), this};
    QAction *_actionLowAudioLatency = new QAction{tr("Low audio latency (may cause hiss)"), this};
    QAction *_actionSettings = new QAction{tr("Settings"), this};
    QAction *_actionAbout = new QAction{tr("About"), this};
    QAction *_actionQuit = new QAction{tr("Quit"), this};
    Core::Settings::TrayIconBatteryBehavior _trayIconBatteryBehavior{
        Core::Settings::TrayIconBatteryBehavior::Disable};
    Status _status{Status::Unavailable};
    std::optional<Core::AirPods::State> _airPodsState;
    std::optional<QString> _displayName;
    std::optional<Core::Update::ReleaseInfo> _updateReleaseInfo;
    std::optional<IconRenderState> _lastIconRenderState;
    std::unique_ptr<QuickConnectBanner> _banner = std::make_unique<QuickConnectBanner>();
    bool _popupVisible{false};
    Core::QuickConnect::Controller &_quickConnect;
    TrayActivationState _trayActivationState;
    QTimer _singleClickTimer{this};
    // Where the pending single click happened; the menu opens there, not wherever the mouse has
    // moved to by the time the double-click interval runs out.
    QPoint _singleClickPosition;

    void ShowMainWindow();
    void Repaint();
    void ExecuteTrayActivation(TrayActivationAction action);
    void ApplyTheme();

    static std::optional<QImage> GenerateIcon(
        int size, const QColor &glyph,
        const std::optional<Core::AirPods::Battery::ValueType> &battery,
        const std::optional<QColor> &dot);

    void OnNewVersionClicked();
    void OnSettingsClicked();
    void OnAboutClicked();
    void OnIconClicked(QSystemTrayIcon::ActivationReason reason);
    void OnQuickConnectOutcome(Core::QuickConnect::Outcome outcome, const QString &deviceName);
    void OnTrayIconBatteryChanged(Core::Settings::TrayIconBatteryBehavior value);
    void OnLowAudioLatencyChanged(bool enabled);

protected:
    SettingsWindow _settingsWindow;

    UTILS_QT_REGISTER_LANGUAGECHANGE(QWidget, [this] { Repaint(); });
};
} // namespace Gui
