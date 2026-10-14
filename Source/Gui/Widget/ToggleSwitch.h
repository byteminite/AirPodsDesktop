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

#include <QCheckBox>

namespace Gui::Widget {

// A Windows 11 (Fluent) toggle switch with its "On"/"Off" state label on the left (F004).
// Still a QCheckBox, so `toggled`, `isChecked`, keyboard handling and accessibility are unchanged
// for everything that used the plain checkboxes before.
class ToggleSwitch : public QCheckBox
{
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool hitButton(const QPoint &position) const override;
    void changeEvent(QEvent *event) override;

private:
    static constexpr int kTrackWidth = 40;
    static constexpr int kTrackHeight = 20;
    static constexpr int kLabelGap = 12;
    static constexpr int kMinimumHeight = 32;

    int LabelWidth() const;
};

} // namespace Gui::Widget
