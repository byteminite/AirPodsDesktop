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

#include <QFrame>
#include <QLabel>

namespace Gui::Widget {

// Paints the translucent Fluent card surface shared by setting cards and the device/About
// header cards (F004).
void PaintFluentCard(QWidget *widget, qreal radius);

// One Windows 11 Settings-style row: a 20px line icon, a title with an optional description, and
// a control on the right. A sub-card drops the icon and indents under the card above it.
class SettingsCard : public QFrame
{
    Q_OBJECT

public:
    explicit SettingsCard(const QString &iconResource, QWidget *parent = nullptr);

    QLabel *Title() const;
    QLabel *Description() const;
    void SetControl(QWidget *control);
    void SetSubCard(bool subCard);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString _iconResource;
    QLabel *_icon;
    QLabel *_title;
    QLabel *_description;
    QWidget *_control{nullptr};

    void ApplyTheme();
};

} // namespace Gui::Widget
