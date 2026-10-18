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

#include <functional>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QTextBrowser>
#include <QTextEdit>

#include "../Core/QuickConnect.h"
#include "../Core/Settings.h"
#include "Utils.h"
#include "Widget/DeviceCard.h"

namespace Gui {

namespace Widget {
class SettingsCard;
class ToggleSwitch;
} // namespace Widget

using namespace Core::Settings;

// Windows 11 Settings-style window (F004): a Mica backdrop, a nav pane with the AirPods at the top,
// and pages of Fluent setting cards. Every control edits the same `Core::Settings` field as before.
class SettingsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsWindow(
        std::function<int()> getCurrentLocaleIndex, Core::QuickConnect::Controller &quickConnect,
        QWidget *parent = nullptr);
    void SetLowAudioLatencyChecked(bool checked);
    void SetDevice(const Widget::DeviceSnapshot &snapshot);

    int GetTabCount() const;
    int GetTabCurrentIndex() const;
    int GetTabLastVisibleIndex() const;
    void SetTabIndex(int index);

Q_SIGNALS:
    void BindRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // Named like the widgets of the former `.ui` form; tests and the handlers look them up so.
    struct Widgets {
        Widget::DeviceBadge *deviceBadge{};
        QListWidget *navList{};
        QLabel *pageTitle{};
        QScrollArea *pagesScrollArea{};
        // Pages stack in one column and only the current one is visible. A QStackedWidget would
        // make every page as tall as the tallest one.
        std::vector<QWidget *> pages;
        int currentPage{-1};

        // AirPods
        Widget::DeviceCard *deviceCard{};
        QLabel *lbFeatures{};
        Widget::SettingsCard *cardAutoEarDetection{}, *cardLowAudioLatency{},
            *cardTrayQuickConnect{}, *cardTrayQuickConnectDevice{};
        Widget::ToggleSwitch *cbAutoEarDetection{}, *cbLowAudioLatency{},
            *cbTrayQuickConnectEnabled{};
        Widget::SettingsCard *cardBatteryLowAlert{}, *cardBatteryChargedAlert{};
        Widget::ToggleSwitch *cbBatteryLowAlert{}, *cbBatteryChargedAlert{};
        QComboBox *cbTrayQuickConnectDevice{};

        // Appearance
        Widget::SettingsCard *cardAppearance{}, *cardTrayIconBattery{}, *cardTaskbarBattery{};
        QComboBox *cbAppearanceMode{}, *cbTrayIconBattery{}, *cbTaskbarBattery{};

        // General
        Widget::SettingsCard *cardLanguage{}, *cardAutoRun{}, *cardMaxReceivingRange{},
            *cardDiagnostics{}, *cardRestoreDefaults{};
        QComboBox *cbLanguages{};
        Widget::ToggleSwitch *cbAutoRun{};
        QSlider *hsMaxReceivingRange{};
        QPushButton *pbOpenLogsDirectory{}, *pbRestoreDefaults{};

        // About
        QLabel *lbAppName{}, *lbVersion{}, *lbCopyright{}, *lbRelatedLinks{};
        Widget::SettingsCard *cardSourceCode{}, *cardIssues{}, *cardLicense{};
        QTextBrowser *tbCredits{};

        // Debug
        QCheckBox *cbAdvOverride{};
        QTextEdit *teAdvOverride{};
    };

    Widgets _ui;
    bool _trigger{true};
    bool _mica{false};
    int _lastLanguageIndex{0};
    std::function<int()> _getCurrentLocaleIndex;
    Core::QuickConnect::Controller &_quickConnect;

    void BuildUi();
    QWidget *BuildAirPodsPage();
    QWidget *BuildAppearancePage();
    QWidget *BuildGeneralPage();
    QWidget *BuildAboutPage();
    QWidget *BuildDebugPage();
    void OnPageChanged(int index);
    void Retranslate();

    void InitCreditsText();
    void RestoreDefaults();
    void Update(const Fields &fields, bool trigger);
    void UpdateQuickConnectDevices(const Fields &fields);
    void UpdateDescriptions();
    void UpdateAdvOverride();

    void showEvent(QShowEvent *event) override;

    // General
    void On_cbLanguages_currentIndexChanged(int index);
    void On_cbAutoRun_toggled(bool checked);
    void On_pbUnbind_clicked();

    // Appearance
    void On_cbAppearanceMode_currentIndexChanged(int index);
    void On_cbDisplayBatteryOnTrayIcon_toggled(TrayIconBatteryBehavior behavior);
    void On_cbDisplayBatteryOnTaskbar_toggled(TaskbarStatusBehavior behavior);

    // AirPods
    void On_cbLowAudioLatency_toggled(bool checked);
    void On_cbTrayQuickConnectEnabled_toggled(bool checked);
    void On_cbTrayQuickConnectDevice_currentIndexChanged(int index);
    void On_cbAutoEarDetection_toggled(bool checked);
    void On_hsMaxReceivingRange_valueChanged(int value);

    // About
    void On_pbOpenLogsDirectory_clicked();

    // Debug
    void On_cbAdvOverride_toggled(bool checked);
    void On_teAdvOverride_textChanged();

    UTILS_QT_DISABLE_ESC_QUIT(QDialog);
    UTILS_QT_REGISTER_LANGUAGECHANGE(QDialog, [this] { Retranslate(); });
};
} // namespace Gui
