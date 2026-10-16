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

#include "SettingsCard.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QVBoxLayout>

#include "../Theme.h"
#include "../TintedSvg.h"

namespace Gui::Widget {
namespace {

constexpr int kIconSize = 20;
constexpr int kSubCardIndent = 52;

} // namespace

void PaintFluentCard(QWidget *widget, qreal radius)
{
    const auto &colors = Theme::Manager::Instance().Colors();
    QPainter painter{widget};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen{colors.dark ? QColor{0, 0, 0, 64} : QColor{0, 0, 0, 15}, 1.0});
    painter.setBrush(colors.dark ? QColor{255, 255, 255, 13} : QColor{255, 255, 255, 184});
    painter.drawRoundedRect(QRectF{widget->rect()}.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
}

SettingsCard::SettingsCard(const QString &iconResource, QWidget *parent)
    : QFrame{parent}, _iconResource{iconResource}, _icon{new QLabel{this}},
      _title{new QLabel{this}}, _description{new QLabel{this}}
{
    setMinimumHeight(68);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    _icon->setFixedSize(kIconSize, kIconSize);
    _title->setWordWrap(true);
    _description->setWordWrap(true);
    // A word-wrapped label otherwise asks for most of its unwrapped width and pushes the control
    // past the card's edge on narrow windows; let it wrap instead.
    _title->setMinimumWidth(48);
    _description->setMinimumWidth(48);
    _description->hide();

    auto descriptionFont = _description->font();
    descriptionFont.setPixelSize(12);
    _description->setFont(descriptionFont);
    auto titleFont = _title->font();
    titleFont.setPixelSize(14);
    _title->setFont(titleFont);

    auto *text = new QVBoxLayout;
    text->setContentsMargins(0, 0, 0, 0);
    text->setSpacing(2);
    text->addWidget(_title);
    text->addWidget(_description);

    auto *layout = new QHBoxLayout{this};
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(16);
    layout->addWidget(_icon, 0, Qt::AlignVCenter);
    layout->addLayout(text, 1);

    ApplyTheme();
    connect(&Theme::Manager::Instance(), &Theme::Manager::Changed, this, &SettingsCard::ApplyTheme);
}

QLabel *SettingsCard::Title() const
{
    return _title;
}

QLabel *SettingsCard::Description() const
{
    return _description;
}

void SettingsCard::SetControl(QWidget *control)
{
    _control = control;
    static_cast<QHBoxLayout *>(layout())->addWidget(control, 0, Qt::AlignVCenter);
}

void SettingsCard::SetSubCard(bool subCard)
{
    _icon->setVisible(!subCard && !_iconResource.isEmpty());
    setMinimumHeight(subCard ? 52 : 68);
    layout()->setContentsMargins(subCard ? kSubCardIndent : 16, 12, 16, 12);
}

void SettingsCard::ApplyTheme()
{
    const auto &colors = Theme::Manager::Instance().Colors();
    if (_iconResource.isEmpty()) {
        _icon->hide();
    }
    else {
        _icon->setPixmap(QPixmap::fromImage(
            TintedSvgImage(_iconResource, colors.text, kIconSize, devicePixelRatioF())));
    }

    auto palette = _title->palette();
    palette.setColor(QPalette::WindowText, colors.text);
    _title->setPalette(palette);
    palette.setColor(QPalette::WindowText, colors.textSecondary);
    _description->setPalette(palette);
    update();
}

void SettingsCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    PaintFluentCard(this, 6.0);
}

} // namespace Gui::Widget
