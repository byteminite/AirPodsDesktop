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

#include "BatteryRow.h"

#include <algorithm>

#include <QPainter>
#include <QPainterPath>

namespace Gui::Widget {

BatteryRow::BatteryRow(QWidget *parent) : QWidget{parent}
{
    setFixedHeight(kRowHeight);
}

void BatteryRow::SetLabel(const QString &text)
{
    if (_label == text) {
        return;
    }
    _label = text;
    update();
}

void BatteryRow::SetValue(std::optional<ValueType> percent)
{
    if (percent.has_value()) {
        percent = std::min(*percent, ValueType{100});
    }
    if (_value == percent) {
        return;
    }
    _value = percent;
    update();
}

void BatteryRow::SetCharging(bool charging)
{
    if (_charging == charging) {
        return;
    }
    _charging = charging;
    update();
}

void BatteryRow::SetLabelColor(const QColor &color)
{
    if (_labelColor == color) {
        return;
    }
    _labelColor = color;
    update();
}

void BatteryRow::SetValueColor(const QColor &color)
{
    if (_valueColor == color) {
        return;
    }
    _valueColor = color;
    update();
}

void BatteryRow::SetTrackColor(const QColor &color)
{
    if (_trackColor == color) {
        return;
    }
    _trackColor = color;
    update();
}

void BatteryRow::SetNormalColor(const QColor &color)
{
    if (_normalColor == color) {
        return;
    }
    _normalColor = color;
    update();
}

void BatteryRow::SetLowColor(const QColor &color)
{
    if (_lowColor == color) {
        return;
    }
    _lowColor = color;
    update();
}

void BatteryRow::SetLowThreshold(ValueType percent)
{
    if (_lowThreshold == percent) {
        return;
    }
    _lowThreshold = percent;
    update();
}

void BatteryRow::SetUnavailableOpacity(qreal opacity)
{
    if (qFuzzyCompare(_unavailableOpacity, opacity)) {
        return;
    }
    _unavailableOpacity = opacity;
    update();
}

QSize BatteryRow::sizeHint() const
{
    return QSize{200, kRowHeight};
}

void BatteryRow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter{this};
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    if (!_value.has_value()) {
        painter.setOpacity(_unavailableOpacity);
    }

    const qreal rowHeight = height();
    const QRectF labelRect{0, 0, kLabelColumnWidth, rowHeight};
    const qreal barLeft = kLabelColumnWidth + kColumnGap;
    const qreal barRight = width() - kValueColumnWidth - kColumnGap;
    const QRectF barRect{
        barLeft, (rowHeight - kBarHeight) / 2.0, qMax(barRight - barLeft, 0.0),
        qreal{kBarHeight}};
    const QRectF valueRect{
        static_cast<qreal>(width() - kValueColumnWidth), 0.0, qreal{kValueColumnWidth}, rowHeight};

    QTextOption leftAlign;
    leftAlign.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QTextOption rightAlign;
    rightAlign.setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    painter.setPen(_labelColor);
    painter.drawText(labelRect, _label, leftAlign);

    painter.setPen(Qt::NoPen);
    painter.setBrush(_trackColor);
    painter.drawRoundedRect(barRect, kBarHeight / 2.0, kBarHeight / 2.0);

    if (_value.has_value() && barRect.width() > 0) {
        const qreal fillWidth = barRect.width() * (*_value / 100.0);
        const QRectF fillRect{barRect.left(), barRect.top(), fillWidth, barRect.height()};
        const bool isLow = *_value <= _lowThreshold;
        painter.setBrush(isLow ? _lowColor : _normalColor);
        painter.drawRoundedRect(fillRect, kBarHeight / 2.0, kBarHeight / 2.0);
    }

    const QString valueText = _value.has_value() ? QString{"%1%"}.arg(*_value) : QStringLiteral("--");
    painter.setPen(_valueColor);
    if (_charging && _value.has_value()) {
        const QRectF boltRect{
            valueRect.right() - kValueColumnWidth, (rowHeight - kChargingIconSize) / 2.0,
            qreal{kChargingIconSize}, qreal{kChargingIconSize}};
        DrawChargingBolt(painter, boltRect);
        const QRectF textRect{
            boltRect.right() + 4, 0, valueRect.width() - kChargingIconSize - 4, rowHeight};
        painter.drawText(textRect, valueText, rightAlign);
    }
    else {
        painter.drawText(valueRect, valueText, rightAlign);
    }
}

void BatteryRow::DrawChargingBolt(QPainter &painter, const QRectF &rect) const
{
    painter.save();
    {
        QPainterPath path;
        path.moveTo(rect.left() + rect.width() * 0.55, rect.top());
        path.lineTo(rect.left(), rect.top() + rect.height() * 0.58);
        path.lineTo(rect.left() + rect.width() * 0.4, rect.top() + rect.height() * 0.58);
        path.lineTo(rect.left() + rect.width() * 0.38, rect.bottom());
        path.lineTo(rect.right(), rect.top() + rect.height() * 0.42);
        path.lineTo(rect.left() + rect.width() * 0.6, rect.top() + rect.height() * 0.42);
        path.closeSubpath();

        painter.setPen(Qt::NoPen);
        painter.setBrush(_normalColor);
        painter.drawPath(path);
    }
    painter.restore();
}

} // namespace Gui::Widget
