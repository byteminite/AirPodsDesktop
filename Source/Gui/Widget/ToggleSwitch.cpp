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

#include "ToggleSwitch.h"

#include <algorithm>

#include <QEvent>
#include <QPainter>

#include "../Theme.h"

namespace Gui::Widget {

ToggleSwitch::ToggleSwitch(QWidget *parent) : QCheckBox{parent}
{
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(minimumSizeHint());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

int ToggleSwitch::LabelWidth() const
{
    const QFontMetrics metrics{font()};
    return std::max(metrics.horizontalAdvance(tr("On")), metrics.horizontalAdvance(tr("Off")));
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize{LabelWidth() + kLabelGap + kTrackWidth + 4, kMinimumHeight};
}

QSize ToggleSwitch::minimumSizeHint() const
{
    return sizeHint();
}

bool ToggleSwitch::hitButton(const QPoint &position) const
{
    return rect().contains(position);
}

void ToggleSwitch::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange || event->type() == QEvent::FontChange) {
        setMinimumSize(minimumSizeHint());
        updateGeometry();
    }
    QCheckBox::changeEvent(event);
}

void ToggleSwitch::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const auto &colors = Theme::Manager::Instance().Colors();
    const bool on = isChecked();
    const bool enabled = isEnabled();

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF track{
        width() - kTrackWidth - 2.0, (height() - kTrackHeight) / 2.0, qreal{kTrackWidth},
        qreal{kTrackHeight}};
    const qreal radius = kTrackHeight / 2.0;

    QColor offStroke = colors.text;
    offStroke.setAlphaF(enabled ? (colors.dark ? 0.78 : 0.6) : 0.3);
    const QColor accent = enabled ? colors.accent : colors.accentDisabled;

    painter.setPen(Qt::NoPen);
    if (on) {
        painter.setBrush(accent);
        painter.drawRoundedRect(track, radius, radius);
    }
    else {
        painter.setPen(QPen{offStroke, 1.0});
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    }

    // Fluent thumb: 12px off, 14px on; it stretches while pressed.
    const qreal thumb = on ? 14.0 : 12.0;
    const qreal stretch = isDown() ? 3.0 : 0.0;
    const qreal thumbY = track.center().y() - thumb / 2.0;
    const qreal thumbX = on ? track.right() - 3.0 - thumb - stretch : track.left() + 4.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(on ? colors.accentText : offStroke);
    painter.drawRoundedRect(
        QRectF{thumbX, thumbY, thumb + stretch, thumb}, thumb / 2.0, thumb / 2.0);

    QColor label = colors.text;
    if (!enabled) {
        label = colors.textDisabled;
    }
    painter.setPen(label);
    const QRectF labelRect{0, 0, track.left() - kLabelGap, qreal(height())};
    painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, on ? tr("On") : tr("Off"));

    if (hasFocus()) {
        painter.setPen(QPen{colors.text, 1.5});
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(track.adjusted(-3, -3, 3, 3), radius + 3, radius + 3);
    }
}

} // namespace Gui::Widget
