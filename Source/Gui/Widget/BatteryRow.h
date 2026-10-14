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

#include <QWidget>

namespace Gui::Widget {

// One Left/Right/Case row in the F002 "Apple popup" battery layout: a label, a slim pill-shaped
// level bar, and a right-aligned percentage. Distinct from `Battery` (the bordered bar with a
// head/nub that `TaskbarStatus` still uses), which keeps its current look unchanged.
class BatteryRow : public QWidget
{
    Q_OBJECT

public:
    using ValueType = quint32;

    explicit BatteryRow(QWidget *parent = nullptr);

    void SetLabel(const QString &text);
    void SetValue(std::optional<ValueType> percent); // nullopt renders the "unavailable" state
    void SetCharging(bool charging);

    void SetLabelColor(const QColor &color);
    void SetValueColor(const QColor &color);
    void SetTrackColor(const QColor &color);
    void SetNormalColor(const QColor &color);
    void SetLowColor(const QColor &color);
    void SetLowThreshold(ValueType percent);
    void SetUnavailableOpacity(qreal opacity);
    void SetColumns(int labelWidth, int valueWidth, int gap);

    QString Label() const
    {
        return _label;
    }
    std::optional<ValueType> Value() const
    {
        return _value;
    }
    bool IsCharging() const
    {
        return _charging;
    }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static constexpr int kBarHeight = 6;
    static constexpr int kChargingIconSize = 12;
    static constexpr int kRowHeight = 18;

    // Defaults are the popup's grid; the tray menu header uses a tighter one.
    int _labelWidth{48};
    int _valueWidth{56};
    int _columnGap{12};

    QString _label;
    std::optional<ValueType> _value;
    bool _charging{false};

    QColor _labelColor{Qt::black};
    QColor _valueColor{Qt::black};
    QColor _trackColor{200, 200, 200};
    QColor _normalColor{48, 209, 88};
    QColor _lowColor{255, 69, 58};
    ValueType _lowThreshold{20};
    qreal _unavailableOpacity{0.38};

    void DrawChargingBolt(QPainter &painter, const QRectF &rect) const;
};

} // namespace Gui::Widget
