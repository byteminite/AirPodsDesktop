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

#include <QImage>
#include <QElapsedTimer>
#include <QMenu>

namespace Gui {

// The tray icon's context menu (F003): the popup's frosted glass instead of the solid menu
// surface every other menu uses, line icons on the left, and toggle checkmarks on the right.
class TrayMenu : public QMenu
{
    Q_OBJECT

public:
    explicit TrayMenu(QWidget *parent = nullptr);

    void ApplyTheme();

    // Mirrors the inset of the shared menu surface so the corners look the same as before.
    static constexpr int kSurfaceInset = 3;
    static constexpr qreal kCornerRadius = 8.0;

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QImage _backdrop;
    QElapsedTimer _shownFor;
};

} // namespace Gui
