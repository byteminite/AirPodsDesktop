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

#include "QuickConnectBanner.h"

#include <QAccessible>
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QVBoxLayout>

#include "FrostedBackdrop.h"
#include "MainWindowPresentation.h"
#include "Theme.h"
#include "TintedSvg.h"
#include "Widget/DeviceCard.h"

namespace Gui {
namespace {

using Core::QuickConnect::Outcome;
using Kind = QuickConnectBannerContent::Kind;

constexpr int kPictureWidth = 96;
constexpr int kPictureHeight = 48;
constexpr int kPlateSize = 40;
constexpr int kPlateIconSize = 20;
constexpr auto kWarningColor = "#FF9F0A";

QString QuickConnectTitle()
{
    return QCoreApplication::translate("Gui::TrayIcon", "Quick connect");
}

} // namespace

std::optional<QuickConnectBannerContent> QuickConnectBannerContentFor(
    Outcome outcome, const QString &deviceName, const std::optional<Core::AirPods::State> &airPods)
{
    QuickConnectBannerContent content;

    // The strings are the ones the Windows notification used, so their translations carry over.
    switch (outcome) {
    case Outcome::Connected:
    case Outcome::AlreadyConnected:
        // Double-clicking an already connected device is how to check its battery.
        content.title = deviceName;
        content.text = QCoreApplication::translate("Gui::Widget::DeviceCard", "Connected");
        // Quick connect can target any paired headset; show AirPods battery only for them.
        if (airPods.has_value() && !deviceName.isEmpty() && airPods->displayName == deviceName) {
            content.kind = Kind::AirPods;
            content.airPods = airPods;
        }
        else {
            content.kind = Kind::Device;
        }
        return content;
    case Outcome::NoDevice:
        content.title = QuickConnectTitle();
        content.text = QCoreApplication::translate(
            "Gui::TrayIcon", "Choose a Bluetooth audio device in Settings first.");
        content.opensSettings = true;
        return content;
    case Outcome::DeviceUnavailable:
        content.title = QuickConnectTitle();
        content.text = QCoreApplication::translate(
            "Gui::TrayIcon",
            "The selected device is not available. Turn it on or bring it into range.");
        return content;
    case Outcome::TimedOut:
        content.title = QuickConnectTitle();
        content.text =
            QCoreApplication::translate("Gui::TrayIcon", "Connection to %1 timed out.")
                .arg(deviceName);
        return content;
    case Outcome::Failed:
        content.title = QuickConnectTitle();
        content.text =
            QCoreApplication::translate("Gui::TrayIcon", "Windows could not connect to %1.")
                .arg(deviceName);
        return content;
    case Outcome::Disabled:
    case Outcome::RequestStarted:
        return std::nullopt;
    }
    return std::nullopt;
}

//////////////////////////////////////////////////

QuickConnectBanner::QuickConnectBanner()
    : QWidget{nullptr}, _leading{new QLabel{this}}, _title{new QLabel{this}},
      _statusDot{new QLabel{this}}, _text{new QLabel{this}}, _rows{new QWidget{this}},
      _left{new Widget::BatteryRow{_rows}}, _right{new Widget::BatteryRow{_rows}},
      _case{new Widget::BatteryRow{_rows}}, _slide{new QPropertyAnimation{this, "pos", this}},
      _fadeIn{new QPropertyAnimation{this, "windowOpacity", this}},
      _fadeOut{this, "windowOpacity"}
{
    setObjectName("quickConnectBanner");
    setWindowFlags(
        Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
        Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setProperty(Theme::Manager::kSkipDwmProperty, true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setFixedWidth(kWidth);

    auto titleFont = _title->font();
    titleFont.setPixelSize(13);
    titleFont.setWeight(QFont::DemiBold);
    _title->setFont(titleFont);
    _title->setObjectName("quickConnectBannerTitle");
    _title->setTextFormat(Qt::PlainText);

    auto textFont = _text->font();
    textFont.setPixelSize(12);
    _text->setFont(textFont);
    _text->setObjectName("quickConnectBannerText");
    _text->setTextFormat(Qt::PlainText);
    _text->setWordWrap(true);
    _statusDot->setFixedSize(6, 6);

    auto rowFont = textFont;
    rowFont.setWeight(QFont::Medium);
    auto *rowsLayout = new QVBoxLayout{_rows};
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(4);
    const std::pair<Widget::BatteryRow *, const char *> rows[] = {
        {_left, "bannerLeftBatteryRow"},
        {_right, "bannerRightBatteryRow"},
        {_case, "bannerCaseBatteryRow"}};
    for (const auto &[row, name] : rows) {
        row->setObjectName(name);
        row->setFont(rowFont);
        row->SetColumns(44, 56, 8);
        rowsLayout->addWidget(row);
    }

    auto *statusLine = new QHBoxLayout;
    statusLine->setSpacing(6);
    statusLine->addWidget(_statusDot, 0, Qt::AlignVCenter);
    statusLine->addWidget(_text, 1);

    auto *text = new QVBoxLayout;
    text->setSpacing(2);
    text->addWidget(_title);
    text->addLayout(statusLine);

    auto *head = new QHBoxLayout;
    head->setSpacing(12);
    head->addWidget(_leading, 0, Qt::AlignVCenter);
    head->addLayout(text, 1);

    auto *layout = new QVBoxLayout{this};
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    layout->addLayout(head);
    layout->addWidget(_rows);

    _hideTimer.setSingleShot(true);
    connect(&_hideTimer, &QTimer::timeout, this, &QuickConnectBanner::Dismiss);

    _slide->setDuration(260);
    _slide->setEasingCurve(QEasingCurve::OutCubic);
    _fadeIn->setDuration(180);
    _fadeIn->setStartValue(0.0);
    _fadeIn->setEndValue(1.0);
    _showAnimation.addAnimation(_slide);
    _showAnimation.addAnimation(_fadeIn);

    _fadeOut.setDuration(220);
    _fadeOut.setEndValue(0.0);
    connect(&_fadeOut, &QPropertyAnimation::finished, this, &QWidget::hide);

    connect(&Theme::Manager::Instance(), &Theme::Manager::Changed, this, [this] {
        if (_content.has_value()) {
            Render();
        }
    });
    connect(qApp, &QCoreApplication::aboutToQuit, this, &QWidget::hide);
}

const std::optional<QuickConnectBannerContent> &QuickConnectBanner::Content() const
{
    return _content;
}

int QuickConnectBanner::HideDelay() const
{
    // Problems stay up longer: unlike the Windows toast, there is no notification centre to
    // look them up in afterwards.
    return _content.has_value() && _content->kind == Kind::Problem ? 6000 : 4000;
}

void QuickConnectBanner::Show(const QuickConnectBannerContent &content)
{
    // A new outcome replaces the banner on screen. It is hidden first so the backdrop grab below
    // never contains the old banner.
    _showAnimation.stop();
    _fadeOut.stop();
    if (isVisible()) {
        hide();
    }

    _content = content;
    Render();
    // `adjustSize` over-estimates word-wrapped labels; size for the fixed width instead. The
    // battery rows may just have been shown or hidden, so drop any cached layout sizes first.
    layout()->invalidate();
    layout()->activate();
    resize(kWidth, layout()->hasHeightForWidth() ? layout()->heightForWidth(kWidth)
                                                 : sizeHint().height());

    auto *screen = QGuiApplication::screenAt(QCursor::pos());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    const auto available = screen->availableGeometry();
    const auto target = PopupPosition(available, size(), QSize{kScreenGap, kScreenGap});

    const auto local = QRect{target, size()}.translated(-screen->geometry().topLeft());
    const auto shot = screen->grabWindow(0, local.x(), local.y(), local.width(), local.height());
    _backdrop = shot.isNull() ? QImage{} : FrostBackdrop(shot.toImage());
    _backdropFightsTheme = false;
    if (!_backdrop.isNull()) {
        const auto mean = _backdrop.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                              .pixelColor(0, 0);
        const bool backdropDark = mean.lightnessF() < 0.45;
        _backdropFightsTheme = backdropDark != Theme::Manager::Instance().Colors().dark;
    }

    setWindowOpacity(0.0);
    move(target);
    show();
    // Resolve the native screen first, then start the slide a little below the target.
    _slide->setStartValue(target + QPoint{0, 16});
    _slide->setEndValue(target);
    _showAnimation.start();

    QAccessibleEvent alert{this, QAccessible::Alert};
    QAccessible::updateAccessibility(&alert);

    if (!underMouse()) {
        _hideTimer.start(HideDelay());
    }
}

void QuickConnectBanner::UpdateAirPods(const Core::AirPods::State &state)
{
    if (!isVisible() || !_content.has_value() || _content->kind != Kind::AirPods) {
        return;
    }
    _content->airPods = state;
    ApplyBattery(state);
}

void QuickConnectBanner::Dismiss()
{
    _hideTimer.stop();
    if (!isVisible() || _fadeOut.state() == QAbstractAnimation::Running) {
        return;
    }
    _showAnimation.stop();
    _fadeOut.setStartValue(windowOpacity());
    _fadeOut.start();
}

void QuickConnectBanner::ApplyBattery(const Core::AirPods::State &state)
{
    const auto apply = [](Widget::BatteryRow *row, const Core::AirPods::Details::BasicState &side) {
        row->SetValue(
            side.battery.Available()
                ? std::optional<Widget::BatteryRow::ValueType>{side.battery.Value()}
                : std::nullopt);
        row->SetCharging(side.battery.Available() && side.isCharging);
    };
    apply(_left, state.pods.left);
    apply(_right, state.pods.right);
    apply(_case, state.caseBox);
}

void QuickConnectBanner::Render()
{
    const auto &content = *_content;
    const auto &colors = Theme::Manager::Instance().Colors();
    const qreal ratio = devicePixelRatioF();

    // Leading visual: the AirPods picture, or a round plate with a line icon.
    QPixmap leading;
    if (content.kind == Kind::AirPods) {
        const auto image =
            Widget::DevicePicture(Widget::DeviceSnapshot{Status::Updating, content.airPods});
        auto scaled = image.scaled(
            QSize{kPictureWidth, kPictureHeight} * ratio, Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(ratio);
        leading = QPixmap::fromImage(scaled);
        _leading->setFixedSize(kPictureWidth, kPictureHeight);
    }
    else {
        const bool problem = content.kind == Kind::Problem;
        leading = QPixmap{QSize{kPlateSize, kPlateSize} * ratio};
        leading.setDevicePixelRatio(ratio);
        leading.fill(Qt::transparent);
        QPainter painter{&leading};
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        painter.setPen(QPen{colors.dark ? QColor{0, 0, 0, 64} : QColor{0, 0, 0, 15}, 1.0});
        painter.setBrush(colors.dark ? QColor{255, 255, 255, 20} : QColor{255, 255, 255, 180});
        painter.drawEllipse(QRectF{0.5, 0.5, kPlateSize - 1.0, kPlateSize - 1.0});
        const auto icon = TintedSvgImage(
            problem ? ":/Resource/Image/Menu/AlertTriangle.svg"
                    : ":/Resource/Image/Menu/Headphones.svg",
            problem ? QColor{kWarningColor} : colors.mainText, kPlateIconSize, ratio);
        const qreal offset = (kPlateSize - kPlateIconSize) / 2.0;
        painter.drawImage(QRectF{offset, offset, kPlateIconSize, kPlateIconSize}, icon);
        painter.end();
        _leading->setFixedSize(kPlateSize, kPlateSize);
    }
    _leading->setPixmap(leading);

    // The title is a device name of any length: elide it to the room beside the leading visual.
    const int titleWidth = kWidth - 32 - _leading->width() - 12;
    _title->setText(
        _title->fontMetrics().elidedText(content.title, Qt::ElideRight, titleWidth));
    _title->setToolTip(content.title);
    _text->setText(content.text);

    const bool connected = content.kind != Kind::Problem;
    _statusDot->setVisible(connected);
    if (connected) {
        QPixmap dot{_statusDot->size() * ratio};
        dot.setDevicePixelRatio(ratio);
        dot.fill(Qt::transparent);
        QPainter painter{&dot};
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.batteryNormal);
        painter.drawEllipse(QRectF{0, 0, 6, 6});
        painter.end();
        _statusDot->setPixmap(dot);
    }

    auto palette = this->palette();
    palette.setColor(QPalette::WindowText, colors.mainText);
    _title->setPalette(palette);
    palette.setColor(QPalette::WindowText, colors.mainTextSecondary);
    _text->setPalette(palette);

    _left->SetLabel(QCoreApplication::translate("Gui::TrayIcon", "Left"));
    _right->SetLabel(QCoreApplication::translate("Gui::TrayIcon", "Right"));
    _case->SetLabel(QCoreApplication::translate("Gui::TrayIcon", "Case"));
    for (auto *row : {_left, _right, _case}) {
        row->SetLabelColor(colors.mainText);
        row->SetValueColor(colors.mainText);
        row->SetTrackColor(colors.batteryTrack);
        row->SetNormalColor(colors.batteryNormal);
        row->SetLowColor(colors.batteryAlarm);
    }
    // The rows keep their space even before the battery is known, so the banner doesn't jump
    // when the reading arrives.
    _rows->setVisible(content.kind == Kind::AirPods);
    if (content.kind == Kind::AirPods && content.airPods.has_value()) {
        ApplyBattery(*content.airPods);
    }

    setAccessibleName(content.title);
    setAccessibleDescription(content.text);
    update();
}

void QuickConnectBanner::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    const auto &colors = Theme::Manager::Instance().Colors();

    QPainter painter{this};
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    painter.setPen(Qt::NoPen);

    const QRectF surface = QRectF{rect()}.adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath shape;
    shape.addRoundedRect(surface, kCornerRadius, kCornerRadius);

    if (_backdrop.isNull()) {
        painter.fillPath(shape, colors.mainSurface);
    }
    else {
        // Brush, not clip: Qt's clipping is binary and would step the corners.
        QBrush backdropBrush{_backdrop};
        backdropBrush.setTransform(QTransform::fromScale(
            qreal(width()) / _backdrop.width(), qreal(height()) / _backdrop.height()));
        painter.fillPath(shape, backdropBrush);
        auto tint = colors.mainGlassTint;
        if (_backdropFightsTheme) {
            tint.setAlpha(200);
        }
        painter.fillPath(shape, tint);
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(
        QPen{colors.dark ? QColor{255, 255, 255, 26} : QColor{0, 0, 0, 20}, 1.0});
    painter.drawPath(shape);
}

void QuickConnectBanner::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        const bool opensSettings = _content.has_value() && _content->opensSettings;
        _hideTimer.stop();
        hide();
        emit Clicked(opensSettings);
    }
    QWidget::mouseReleaseEvent(event);
}

void QuickConnectBanner::enterEvent(QEnterEvent *event)
{
    // Hovering keeps the banner up, and brings it back if it was fading out.
    _hideTimer.stop();
    if (_fadeOut.state() == QAbstractAnimation::Running) {
        _fadeOut.stop();
        setWindowOpacity(1.0);
    }
    QWidget::enterEvent(event);
}

void QuickConnectBanner::leaveEvent(QEvent *event)
{
    if (isVisible()) {
        _hideTimer.start(HideDelay() / 2);
    }
    QWidget::leaveEvent(event);
}

void QuickConnectBanner::hideEvent(QHideEvent *event)
{
    _hideTimer.stop();
    _showAnimation.stop();
    _backdrop = {};
    QWidget::hideEvent(event);
}

} // namespace Gui
