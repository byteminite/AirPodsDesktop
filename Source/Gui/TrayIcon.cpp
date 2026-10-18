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

#include "TrayIcon.h"

#include "Theme.h"
#include "TintedSvg.h"

#include <QFont>
#include <QApplication>
#include <QCursor>
#include <QPainter>
#include <QSvgRenderer>

#include <algorithm>

#include <Config.h>

#if defined APD_OS_WIN
    #include <Windows.h>
#endif

namespace Gui {
namespace {

constexpr auto kTrayGlyph = ":/Resource/Image/TrayAirPods.svg";
constexpr int kMenuIconSize = 16;

} // namespace

TrayIcon::TrayIcon(
    std::function<int()> getCurrentLocaleIndex, Core::QuickConnect::Controller &quickConnect)
    : _quickConnect{quickConnect}, _settingsWindow{std::move(getCurrentLocaleIndex), quickConnect}
{
    _singleClickTimer.setSingleShot(true);
    connect(&_singleClickTimer, &QTimer::timeout, this, [this] {
        ExecuteTrayActivation(_trayActivationState.OnSingleClickTimeout());
    });

    connect(_actionNewVersion, &QAction::triggered, this, &TrayIcon::OnNewVersionClicked);
    connect(_actionLowAudioLatency, &QAction::triggered, this, [](bool enabled) {
        Core::Settings::ModifiableAccess()->low_audio_latency = enabled;
    });
    connect(_actionSettings, &QAction::triggered, this, &TrayIcon::OnSettingsClicked);
    connect(_actionAbout, &QAction::triggered, this, &TrayIcon::OnAboutClicked);
    connect(_actionQuit, &QAction::triggered, qApp, &QApplication::quit, Qt::QueuedConnection);
    connect(_tray, &QSystemTrayIcon::activated, this, &TrayIcon::OnIconClicked);
    connect(_tray, &QSystemTrayIcon::messageClicked, this, &TrayIcon::ShowMainWindowRequested);
    // "Bind to AirPods" on the Settings device card opens the popup, which lists the devices.
    connect(
        &_settingsWindow, &SettingsWindow::BindRequested, this, &TrayIcon::ShowMainWindowRequested);
    connect(_banner.get(), &QuickConnectBanner::Clicked, this, [this](bool opensSettings) {
        if (opensSettings) {
            _settingsWindow.SetTabIndex(0);
            _settingsWindow.show();
            _settingsWindow.raise();
            _settingsWindow.activateWindow();
        }
        else {
            emit ShowMainWindowRequested();
        }
    });
    connect(
        &_quickConnect, &Core::QuickConnect::Controller::OutcomeChanged, this,
        &TrayIcon::OnQuickConnectOutcome);

    connect(
        this, &TrayIcon::OnTrayIconBatteryChangedSafely, this, &TrayIcon::OnTrayIconBatteryChanged);
    connect(
        this, &TrayIcon::OnLowAudioLatencyChangedSafely, this, &TrayIcon::OnLowAudioLatencyChanged);

    _actionNewVersion->setVisible(false);
    _actionLowAudioLatency->setCheckable(true);

    _headerAction->setDefaultWidget(_header);
    _menu->addAction(_headerAction);
    _menu->addSeparator();
    _menu->addAction(_actionNewVersion);
    _menu->addAction(_actionLowAudioLatency);
    _menu->addAction(_actionSettings);
    _menu->addSeparator();
    _menu->addAction(_actionAbout);
    _menu->addAction(_actionQuit);

    _tray->setContextMenu(_menu);
    _tray->setIcon(qApp->windowIcon());
    _tray->show();

    ApplyTheme();
    connect(&Theme::Manager::Instance(), &Theme::Manager::Changed, this, &TrayIcon::ApplyTheme);
}

void TrayIcon::ApplyTheme()
{
    const auto &colors = Theme::Manager::Instance().Colors();
    const std::pair<QAction *, const char *> icons[] = {
        {_actionNewVersion, ":/Resource/Image/Menu/Download.svg"},
        {_actionLowAudioLatency, ":/Resource/Image/Menu/AudioLines.svg"},
        {_actionSettings, ":/Resource/Image/Menu/Settings.svg"},
        {_actionAbout, ":/Resource/Image/Menu/Info.svg"},
        {_actionQuit, ":/Resource/Image/Menu/Power.svg"},
    };
    for (const auto &[action, resource] : icons) {
        action->setIcon(TintedSvgIcon(
            resource, action == _actionNewVersion ? colors.accent : colors.text, kMenuIconSize));
    }
    _header->ApplyTheme();

    // The tray glyph follows the system theme, which may have just changed too.
    Repaint();
}

void TrayIcon::UpdateState(const Core::AirPods::State &state)
{
    _status = Status::Updating;
    _airPodsState = state;
    _banner->UpdateAirPods(state);

    // F006: low battery warning and "fully charged" notice. The policy sees every state so its
    // bookkeeping stays current even while a banner can't be shown.
    const auto fields = Core::Settings::GetCurrent();
    const auto alert =
        _batteryAlerts.OnState(state, fields.battery_low_alert, fields.battery_charged_alert);
    if (alert.has_value() && !_popupVisible && !UserIsBusy()) {
        _banner->Show(BatteryAlertBannerContent(*alert, state));
    }
    Repaint();
}

void TrayIcon::Unavailable()
{
    _status = Status::Unavailable;
    _airPodsState.reset();
    Repaint();
}

void TrayIcon::Disconnect()
{
    // No device is bound yet, so "Waiting for Binding" outranks "Disconnected". The scanner
    // reports itself available on every (re)start, and that must not overwrite this state.
    if (_status == Status::Unbind) {
        return;
    }

    _status = Status::Disconnected;
    _airPodsState.reset();
    Repaint();
}

void TrayIcon::Unbind()
{
    _status = Status::Unbind;
    _airPodsState.reset();
    Repaint();
}

void TrayIcon::VersionUpdateAvailable(const Core::Update::ReleaseInfo &releaseInfo)
{
    _updateReleaseInfo = releaseInfo;
    _actionNewVersion->setVisible(true);
    Repaint();
}

void TrayIcon::ShowMainWindow()
{
    emit ShowMainWindowRequested();
}

void TrayIcon::ShowContextMenu(const QPoint &position)
{
    _menu->popup(position);
#if defined APD_OS_WIN
    // A menu that is not the foreground window neither closes on a click elsewhere nor stays above
    // the taskbar. The app already holds the foreground (see `OnIconClicked`), so this is allowed.
    SetForegroundWindow(reinterpret_cast<HWND>(_menu->winId()));
#endif
}

void TrayIcon::Repaint()
{
    _settingsWindow.SetDevice({_status, _airPodsState});

    QString toolTipContent;
    Core::AirPods::Battery minBattery;

    switch (_status) {
    case Status::Unavailable:
    case Status::Disconnected:
    case Status::Unbind:
        toolTipContent = DisplayableStatus(_status);
        _header->SetStatus(toolTipContent);
        break;
    case Status::Updating: {
        if (!_airPodsState.has_value()) {
            break;
        }
        const auto &state = _airPodsState.value();
        _header->SetState(state);

        toolTipContent += state.displayName;

        const auto strLeft{tr("Left")}, strRight{tr("Right")}, strCase{tr("Case")},
            strCharging{tr("charging")};

        const auto textCharging = QString{" (%1)"}.arg(strCharging),
                   textPlaceHolder = QString{"\n%1: %2%%3"};

        // clang-format off
        if (state.pods.left.battery.Available()) {
            const auto batteryValue = state.pods.left.battery.Value();

            toolTipContent += textPlaceHolder
                .arg(strLeft)
                .arg(batteryValue)
                .arg(state.pods.left.isCharging ? textCharging : QString{});

            minBattery = batteryValue;
        }

        if (state.pods.right.battery.Available()) {
            const auto batteryValue = state.pods.right.battery.Value();

            toolTipContent += textPlaceHolder
                .arg(strRight)
                .arg(batteryValue)
                .arg(state.pods.right.isCharging ? textCharging : QString{});

            if (minBattery.Available() && batteryValue < minBattery.Value() ||
                !minBattery.Available()) {
                minBattery = batteryValue;
            }
        }

        if (state.caseBox.battery.Available()) {
            toolTipContent += textPlaceHolder
                .arg(strCase)
                .arg(state.caseBox.battery.Value())
                .arg(state.caseBox.isCharging ? textCharging : QString{});
        }
        // clang-format on
        break;
    }
    default:
        APD_ASSERT(false);
    }

    if (_updateReleaseInfo.has_value()) {
        toolTipContent += '\n' + _actionNewVersion->text();
    }

    const auto toolTip = "AirPodsDesktop\n" + toolTipContent.trimmed();
    _tray->setToolTip(toolTip);
    emit ToolTipChanged(toolTip);

    // RepaintIcon

    std::optional<Core::AirPods::Battery::ValueType> iconBattery;
    if (minBattery.Available()) {
        const bool drawBattery = [&] {
            switch (_trayIconBatteryBehavior) {
            case Core::Settings::TrayIconBatteryBehavior::WhenLowBattery:
                return minBattery.IsLowBattery();
            case Core::Settings::TrayIconBatteryBehavior::Always:
                return true;
            case Core::Settings::TrayIconBatteryBehavior::Disable:
            default:
                return false;
            }
        }();
        if (drawBattery) {
            iconBattery = minBattery.Value();
        }
    }

    static const QColor kNewVersionAvailableDot = Qt::yellow;
    const IconRenderState nextIconRenderState{
        .battery = iconBattery,
        .showUpdateDot = _updateReleaseInfo.has_value(),
        .systemDark = Theme::Manager::Instance().IsSystemDark(),
    };

    if (_lastIconRenderState == nextIconRenderState) {
        return;
    }

    // One rendition per common tray size (100%-300% scaling): a thin line glyph only stays crisp
    // when drawn at the size Windows shows it, not shrunk from a large image.
    QIcon icon;
    for (const int size : {16, 20, 24, 32, 40, 48, 64}) {
        const auto optIcon = GenerateIcon(
            size, nextIconRenderState.systemDark ? QColor{Qt::white} : QColor{Qt::black},
            nextIconRenderState.battery,
            nextIconRenderState.showUpdateDot ? std::optional<QColor>{kNewVersionAvailableDot}
                                              : std::nullopt);
        if (optIcon.has_value()) {
            icon.addPixmap(QPixmap::fromImage(optIcon.value()));
        }
    }
    if (!icon.isNull()) {
        _tray->setIcon(icon);
        _lastIconRenderState = nextIconRenderState;
    }
}

std::optional<QImage> TrayIcon::GenerateIcon(
    int size, const QColor &glyph,
    const std::optional<Core::AirPods::Battery::ValueType> &battery,
    const std::optional<QColor> &dot)
{
    QImage result{size, size, QImage::Format_ARGB32_Premultiplied};
    result.fill(Qt::transparent);
    QPainter painter{&result};
    painter.setRenderHint(QPainter::Antialiasing);

    if (!battery.has_value()) {
        painter.drawImage(0, 0, TintedSvgImage(kTrayGlyph, glyph, size, 1.0));
    }
    else {
        // A tray icon is ~16px: digits on a badge were unreadable there, so show the lowest pod's
        // level as a small battery bar under a slightly smaller glyph (F003). The exact
        // percentage is in the tooltip and the tray menu.
        const int barHeight = std::max(3, qRound(size * 0.19));
        const int glyphSize = size - barHeight - std::max(1, qRound(size * 0.08));
        painter.drawImage(
            (size - glyphSize) / 2, 0, TintedSvgImage(kTrayGlyph, glyph, glyphSize, 1.0));

        const QRectF track{1.0, qreal(size - barHeight), qreal(size - 2), qreal(barHeight)};
        const qreal radius = barHeight / 2.0;
        QColor trackColor = glyph;
        trackColor.setAlpha(90);
        painter.setPen(Qt::NoPen);
        painter.setBrush(trackColor);
        painter.drawRoundedRect(track, radius, radius);

        const auto level = std::min<Core::AirPods::Battery::ValueType>(battery.value(), 100);
        const qreal fillWidth = std::max(track.height(), track.width() * level / 100.0);
        // Same thresholds and colours as the battery rows in the popup and menu.
        painter.setBrush(level <= 20 ? QColor{255, 59, 48} : QColor{52, 199, 89});
        painter.drawRoundedRect(QRectF{track.topLeft(), QSizeF{fillWidth, track.height()}}, radius, radius);
    }

    painter.save();
    do {
        if (!dot.has_value()) {
            break;
        }

        const double dotDiameter = size * 0.32;

        painter.setBrush(dot.value());
        painter.drawEllipse(QRectF{size - dotDiameter, 0, dotDiameter, dotDiameter});
    } while (false);
    painter.restore();

    return result;
}

void TrayIcon::OnNewVersionClicked()
{
    APD_ASSERT(_updateReleaseInfo.has_value());

    _actionNewVersion->setVisible(false);

    auto releaseInfo = std::move(_updateReleaseInfo.value());
    _updateReleaseInfo.reset();
    Repaint();

    emit UserUpdateRequested(releaseInfo);
}

void TrayIcon::OnSettingsClicked()
{
    if (!_settingsWindow.isVisible() ||
        _settingsWindow.GetTabCurrentIndex() == _settingsWindow.GetTabLastVisibleIndex())
    {
        _settingsWindow.SetTabIndex(0);
    }

    _settingsWindow.show();
    _settingsWindow.raise();
}

void TrayIcon::OnAboutClicked()
{
    _settingsWindow.SetTabIndex(_settingsWindow.GetTabLastVisibleIndex());
    _settingsWindow.show();
    _settingsWindow.raise();
}

void TrayIcon::OnIconClicked(QSystemTrayIcon::ActivationReason reason)
{
    const auto result = _trayActivationState.OnActivation(reason, _quickConnect.IsEnabled());

    switch (result.timer) {
    case SingleClickTimer::Start:
        // Read the interval each time; the user can change it in Windows while we are running.
        _singleClickPosition = QCursor::pos();
#if defined APD_OS_WIN
        // Windows lets an app come to the foreground only while handling the click itself; any
        // mouse movement during the double-click wait takes that right away again. Take the
        // foreground now (on this never-shown window, as Qt does for a right click) so the menu
        // opened after the wait can be activated: only then does it close on a click elsewhere
        // and stay above the taskbar.
        SetForegroundWindow(reinterpret_cast<HWND>(winId()));
#endif
        _singleClickTimer.start(QApplication::doubleClickInterval());
        break;
    case SingleClickTimer::Stop:
        _singleClickTimer.stop();
        break;
    case SingleClickTimer::Unchanged:
        break;
    }

    ExecuteTrayActivation(result.action);
}

void TrayIcon::ExecuteTrayActivation(TrayActivationAction action)
{
    if (action == TrayActivationAction::QuickConnect) {
        _quickConnect.Request();
        return;
    }

    if (action == TrayActivationAction::ShowMainWindow) {
        ShowMainWindow();
    }
    else if (action == TrayActivationAction::ShowMenu) {
        ShowContextMenu(_singleClickPosition);
    }
}

void TrayIcon::OnQuickConnectOutcome(Core::QuickConnect::Outcome outcome, const QString &deviceName)
{
    // F005: results and problems show as the app's own banner instead of a Windows notification.
    // "Connecting..." stays silent; "already connected" shows the battery, if known.
    const auto content = QuickConnectBannerContentFor(
        outcome, deviceName,
        _status == Status::Updating ? _airPodsState : std::optional<Core::AirPods::State>{});
    if (!content.has_value() || _popupVisible) {
        return;
    }
    _banner->Show(*content);
}

void TrayIcon::SetPopupVisible(bool visible)
{
    _popupVisible = visible;
    if (visible) {
        _banner->Dismiss();
    }
}

void TrayIcon::OnTrayIconBatteryChanged(Core::Settings::TrayIconBatteryBehavior value)
{
    _trayIconBatteryBehavior = value;
    Repaint();
}

void TrayIcon::OnLowAudioLatencyChanged(bool enabled)
{
    _actionLowAudioLatency->setChecked(enabled);
    _settingsWindow.SetLowAudioLatencyChecked(enabled);
}

} // namespace Gui
