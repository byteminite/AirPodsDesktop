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

#include "DeviceCard.h"

#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QStyle>
#include <QVBoxLayout>

#include "../MainWindowPresentation.h"
#include "../Theme.h"
#include "SettingsCard.h"

namespace Gui::Widget {
namespace {

void ApplyBattery(BatteryRow *row, const Core::AirPods::Details::BasicState &state)
{
    row->SetValue(
        state.battery.Available() ? std::optional<BatteryRow::ValueType>{state.battery.Value()}
                                  : std::nullopt);
    row->SetCharging(state.battery.Available() && state.isCharging);
}

bool IsConnected(const DeviceSnapshot &snapshot)
{
    return snapshot.status == Status::Updating && snapshot.state.has_value();
}

QPixmap ScaledPicture(const QImage &image, const QSize &box, qreal devicePixelRatio)
{
    if (image.isNull()) {
        return {};
    }
    auto scaled = image.scaled(
        box * devicePixelRatio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(devicePixelRatio);
    return QPixmap::fromImage(scaled);
}

QColor StatusDotColor(const DeviceSnapshot &snapshot)
{
    return IsConnected(snapshot) ? QColor{108, 203, 95} : QColor{138, 138, 138};
}

} // namespace

QString DeviceDisplayName(const DeviceSnapshot &snapshot)
{
    if (snapshot.status == Status::Unbind) {
        return DeviceCard::tr("No AirPods bound");
    }
    if (snapshot.state.has_value() && !snapshot.state->displayName.isEmpty()) {
        return snapshot.state->displayName;
    }
    return QStringLiteral("AirPods");
}

QString DeviceStatusText(const DeviceSnapshot &snapshot)
{
    if (IsConnected(snapshot)) {
        return DeviceCard::tr("Connected");
    }
    switch (snapshot.status) {
    case Status::Unavailable:
    case Status::Disconnected:
    case Status::Unbind:
        return DisplayableStatus(snapshot.status);
    default:
        return DisplayableStatus(Status::Disconnected);
    }
}

QImage DevicePicture(const DeviceSnapshot &snapshot)
{
    const auto model = snapshot.state.has_value() &&
                               snapshot.state->model != Core::AirPods::Model::Unknown
                           ? snapshot.state->model
                           : Core::AirPods::Model::AirPods_Pro_2;
    return QImage{GetAnimationPresentation(model).FallbackResource()};
}

//////////////////////////////////////////////////

DeviceCard::DeviceCard(QWidget *parent)
    : QFrame{parent}, _picture{new QLabel{this}}, _name{new QLabel{this}},
      _statusDot{new QLabel{this}}, _status{new QLabel{this}}, _hint{new QLabel{this}},
      _rows{new QWidget{this}}, _left{new BatteryRow{_rows}}, _right{new BatteryRow{_rows}},
      _case{new BatteryRow{_rows}}, _button{new QPushButton{this}}
{
    setObjectName("deviceCard");
    _picture->setFixedSize(150, 100);
    _picture->setAlignment(Qt::AlignCenter);

    auto nameFont = _name->font();
    nameFont.setPixelSize(20);
    nameFont.setWeight(QFont::DemiBold);
    _name->setFont(nameFont);
    _name->setTextFormat(Qt::PlainText);
    _name->setObjectName("deviceCardName");

    auto smallFont = _status->font();
    smallFont.setPixelSize(12);
    _status->setFont(smallFont);
    _status->setObjectName("deviceCardStatus");
    _statusDot->setFixedSize(8, 8);
    _hint->setFont(smallFont);
    _hint->setWordWrap(true);

    auto rowFont = smallFont;
    rowFont.setWeight(QFont::Medium);
    auto *rowsLayout = new QVBoxLayout{_rows};
    rowsLayout->setContentsMargins(0, 4, 0, 0);
    rowsLayout->setSpacing(6);
    const std::pair<BatteryRow *, const char *> rows[] = {
        {_left, "deviceLeftBatteryRow"},
        {_right, "deviceRightBatteryRow"},
        {_case, "deviceCaseBatteryRow"}};
    for (const auto &[row, name] : rows) {
        row->setObjectName(name);
        row->setFont(rowFont);
        row->SetColumns(44, 52, 10);
        row->setMaximumWidth(340);
        rowsLayout->addWidget(row);
    }

    _button->setObjectName("pbUnbind");
    _button->setAutoDefault(false);
    connect(_button, &QPushButton::clicked, this, [this] {
        if (_snapshot.status == Status::Unbind) {
            emit BindRequested();
        }
        else {
            emit UnbindRequested();
        }
    });

    auto *statusLine = new QHBoxLayout;
    statusLine->setSpacing(6);
    statusLine->addWidget(_statusDot);
    statusLine->addWidget(_status, 1);

    _textLayout = new QVBoxLayout;
    _textLayout->setSpacing(2);
    _textLayout->addWidget(_name);
    _textLayout->addLayout(statusLine);
    _textLayout->addWidget(_hint);
    _textLayout->addWidget(_rows);
    _textLayout->addStretch();

    _layout = new QHBoxLayout{this};
    _layout->setContentsMargins(22, 20, 22, 20);
    _layout->setSpacing(22);
    _layout->addWidget(_picture, 0, Qt::AlignVCenter);
    _layout->addLayout(_textLayout, 1);
    _layout->addWidget(_button, 0, Qt::AlignTop);

    _left->SetLabel(QCoreApplication::translate("Gui::TrayIcon", "Left"));
    _right->SetLabel(QCoreApplication::translate("Gui::TrayIcon", "Right"));
    _case->SetLabel(QCoreApplication::translate("Gui::TrayIcon", "Case"));

    connect(&Theme::Manager::Instance(), &Theme::Manager::Changed, this, &DeviceCard::ApplyTheme);
    ApplyTheme();
}

QPushButton *DeviceCard::Button() const
{
    return _button;
}

void DeviceCard::SetSnapshot(const DeviceSnapshot &snapshot)
{
    _snapshot = snapshot;
    Render();
}

void DeviceCard::Render()
{
    const bool unbound = _snapshot.status == Status::Unbind;
    _name->setText(DeviceDisplayName(_snapshot));
    _status->setText(DeviceStatusText(_snapshot));
    _picture->setPixmap(
        ScaledPicture(DevicePicture(_snapshot), _picture->size(), devicePixelRatioF()));

    _statusDot->setVisible(!unbound);
    _status->setVisible(!unbound);
    _rows->setVisible(!unbound);
    _hint->setVisible(unbound);
    _hint->setText(tr("Choose your paired AirPods to see their battery here."));

    if (_snapshot.state.has_value() && _snapshot.status == Status::Updating) {
        ApplyBattery(_left, _snapshot.state->pods.left);
        ApplyBattery(_right, _snapshot.state->pods.right);
        ApplyBattery(_case, _snapshot.state->caseBox);
    }
    else {
        for (auto *row : {_left, _right, _case}) {
            row->SetValue(std::nullopt);
            row->SetCharging(false);
        }
    }

    // "Unbind AirPods" is the translated label from the old Settings layout.
    _button->setText(
        unbound ? QCoreApplication::translate("Gui::MainWindow", "Bind to AirPods")
                : QCoreApplication::translate("SettingsWindow", "Unbind AirPods"));
    _button->setToolTip(
        unbound ? QString{}
                : QCoreApplication::translate(
                      "SettingsWindow",
                      "Forget the bound AirPods so that another pair can be bound."));
    _button->setProperty("cssClass", unbound ? "accent" : "");
    _button->style()->unpolish(_button);
    _button->style()->polish(_button);

    QPixmap dot{_statusDot->size() * devicePixelRatioF()};
    dot.setDevicePixelRatio(devicePixelRatioF());
    dot.fill(Qt::transparent);
    QPainter painter{&dot};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(StatusDotColor(_snapshot));
    painter.drawEllipse(QRectF{0, 0, 8, 8});
    painter.end();
    _statusDot->setPixmap(dot);
}

void DeviceCard::ApplyTheme()
{
    const auto &colors = Theme::Manager::Instance().Colors();
    auto palette = this->palette();
    palette.setColor(QPalette::WindowText, colors.text);
    _name->setPalette(palette);
    palette.setColor(QPalette::WindowText, colors.textSecondary);
    _status->setPalette(palette);
    _hint->setPalette(palette);
    for (auto *row : {_left, _right, _case}) {
        row->SetLabelColor(colors.text);
        row->SetValueColor(colors.text);
        row->SetTrackColor(colors.batteryTrack);
        row->SetNormalColor(colors.batteryNormal);
        row->SetLowColor(colors.batteryAlarm);
    }
    Render();
}

void DeviceCard::resizeEvent(QResizeEvent *event)
{
    // On a narrow window the picture shrinks and the button moves under the battery bars, so
    // nothing is pushed past the card's right edge.
    const bool compact = width() < kCompactWidth;
    if (compact != _compact) {
        _compact = compact;
        _picture->setFixedSize(compact ? QSize{96, 64} : QSize{150, 100});
        _layout->removeWidget(_button);
        if (compact) {
            _textLayout->insertWidget(_textLayout->count() - 1, _button, 0, Qt::AlignLeft);
        }
        else {
            _layout->addWidget(_button, 0, Qt::AlignTop);
        }
        _layout->setSpacing(compact ? 16 : 22);
        Render();
    }
    QFrame::resizeEvent(event);
}

void DeviceCard::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        _left->SetLabel(QCoreApplication::translate("Gui::TrayIcon", "Left"));
        _right->SetLabel(QCoreApplication::translate("Gui::TrayIcon", "Right"));
        _case->SetLabel(QCoreApplication::translate("Gui::TrayIcon", "Case"));
        Render();
    }
    QFrame::changeEvent(event);
}

void DeviceCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    PaintFluentCard(this, 8.0);
}

//////////////////////////////////////////////////

DeviceBadge::DeviceBadge(QWidget *parent)
    : QWidget{parent}, _picture{new QLabel{this}}, _name{new QLabel{this}},
      _status{new QLabel{this}}
{
    setObjectName("deviceBadge");
    _picture->setFixedSize(56, 56);
    _picture->setAlignment(Qt::AlignCenter);

    auto nameFont = _name->font();
    nameFont.setPixelSize(14);
    nameFont.setWeight(QFont::DemiBold);
    _name->setFont(nameFont);
    _name->setTextFormat(Qt::PlainText);
    auto statusFont = _status->font();
    statusFont.setPixelSize(12);
    _status->setFont(statusFont);

    auto *text = new QVBoxLayout;
    text->setSpacing(2);
    text->addStretch();
    text->addWidget(_name);
    text->addWidget(_status);
    text->addStretch();

    auto *layout = new QHBoxLayout{this};
    layout->setContentsMargins(8, 4, 8, 14);
    layout->setSpacing(12);
    layout->addWidget(_picture);
    layout->addLayout(text, 1);

    connect(&Theme::Manager::Instance(), &Theme::Manager::Changed, this, &DeviceBadge::Render);
    Render();
}

void DeviceBadge::SetSnapshot(const DeviceSnapshot &snapshot)
{
    _snapshot = snapshot;
    Render();
}

void DeviceBadge::Render()
{
    const auto &colors = Theme::Manager::Instance().Colors();
    auto palette = this->palette();
    palette.setColor(QPalette::WindowText, colors.text);
    _name->setPalette(palette);
    palette.setColor(QPalette::WindowText, colors.textSecondary);
    _status->setPalette(palette);

    _name->setText(DeviceDisplayName(_snapshot));
    _status->setText(DeviceStatusText(_snapshot));

    // A round, card-coloured plate with the AirPods picture inside, like the account picture in
    // Windows 11 Settings.
    const qreal ratio = devicePixelRatioF();
    QPixmap plate{_picture->size() * ratio};
    plate.setDevicePixelRatio(ratio);
    plate.fill(Qt::transparent);
    QPainter painter{&plate};
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    painter.setPen(QPen{colors.dark ? QColor{0, 0, 0, 64} : QColor{0, 0, 0, 15}, 1.0});
    painter.setBrush(colors.dark ? QColor{255, 255, 255, 13} : QColor{255, 255, 255, 184});
    painter.drawEllipse(QRectF{0.5, 0.5, 55, 55});
    const auto picture = ScaledPicture(DevicePicture(_snapshot), QSize{42, 42}, ratio);
    if (!picture.isNull()) {
        const QSizeF logical = picture.deviceIndependentSize();
        painter.drawPixmap(
            QPointF{(56 - logical.width()) / 2.0, (56 - logical.height()) / 2.0}, picture);
    }
    painter.end();
    _picture->setPixmap(plate);
}

void DeviceBadge::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        Render();
    }
    QWidget::changeEvent(event);
}

} // namespace Gui::Widget
