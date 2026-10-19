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

#include "TrayMenu.h"

#include <algorithm>

#include <QGuiApplication>
#include <QMouseEvent>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

#include "../Logger.h"
#include "FrostedBackdrop.h"
#include "Theme.h"
#include "TintedSvg.h"

namespace Gui {
namespace {

constexpr auto kCheckIcon = ":/Resource/Image/Menu/Check.svg";
constexpr int kCheckSize = 14;
constexpr int kItemSidePadding = 10;
// Gap between the menu and the screen's work-area edges (the taskbar), as Windows 11 menus keep.
constexpr int kScreenGap = 12;

QString Rgba(const QColor &color)
{
    return QString{"rgba(%1, %2, %3, %4)"}
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

} // namespace

TrayMenu::TrayMenu(QWidget *parent) : QMenu{parent}
{
    setObjectName("trayMenu");
    // Paints its own surface below, so opt out of the shared solid one.
    setProperty(Theme::kCustomSurfaceProperty, true);
    Theme::ConfigurePopupSurface(this);
    ApplyTheme();
    connect(&Theme::Manager::Instance(), &Theme::Manager::Changed, this, &TrayMenu::ApplyTheme);
}

void TrayMenu::ApplyTheme()
{
    const auto &colors = Theme::Manager::Instance().Colors();
    const QColor hover = colors.dark ? QColor{255, 255, 255, 26} : QColor{0, 0, 0, 15};

    // The shared indicator column is hidden: checkmarks are drawn on the right in `paintEvent`.
    setStyleSheet(QString{
        "QMenu#trayMenu { padding: %1px; }"
        "QMenu#trayMenu::item { padding: 8px 36px 8px %2px; border-radius: 5px; color: %3; }"
        "QMenu#trayMenu::item:selected { background: %4; }"
        "QMenu#trayMenu::icon { padding-left: %2px; }"
        "QMenu#trayMenu::indicator { width: 0px; height: 0px; margin: 0px; image: none; }"
        "QMenu#trayMenu::separator { height: 1px; background: %5; margin: 4px 8px; }"}
                      .arg(kSurfaceInset + 6)
                      .arg(kItemSidePadding)
                      .arg(colors.text.name())
                      .arg(Rgba(hover))
                      .arg(Rgba(colors.dark ? QColor{255, 255, 255, 26} : QColor{0, 0, 0, 20})));
    update();
}

void TrayMenu::showEvent(QShowEvent *event)
{
    // QShowEvent arrives before the native window is mapped, and `popup()` has already placed
    // the menu, so the grab sees exactly what the menu is about to cover.
    auto area = geometry();
    auto *screen = QGuiApplication::screenAt(area.center());
    if (screen == nullptr) {
        screen = this->screen();
    }
    if (screen != nullptr) {
        // Qt places the menu flush against the taskbar; lift it off the edges a little. The
        // painted surface is already inset by `kSurfaceInset`, so subtract that from the gap.
        const auto bounds = screen->availableGeometry().adjusted(
            kScreenGap - kSurfaceInset, kScreenGap - kSurfaceInset, kSurfaceInset - kScreenGap,
            kSurfaceInset - kScreenGap);
        auto moved = area;
        moved.moveRight((std::min)(moved.right(), bounds.right()));
        moved.moveBottom((std::min)(moved.bottom(), bounds.bottom()));
        moved.moveLeft((std::max)(moved.left(), bounds.left()));
        moved.moveTop((std::max)(moved.top(), bounds.top()));
        if (moved.topLeft() != area.topLeft()) {
            move(moved.topLeft());
            area = moved;
        }
    }
    _backdrop = {};
    if (screen != nullptr) {
        const auto local = area.translated(-screen->geometry().topLeft());
        const auto shot =
            screen->grabWindow(0, local.x(), local.y(), local.width(), local.height());
        if (!shot.isNull()) {
            _backdrop = FrostBackdrop(shot.toImage());
        }
    }
    _shownFor.start();
    QMenu::showEvent(event);
}

void TrayMenu::mouseReleaseEvent(QMouseEvent *event)
{
    // A tray single click opens this menu right where the user clicked. If they meant to double
    // click but were a little slow, the second click arrives just after the menu opened and would
    // land on the item under the cursor, the bottom one: Quit. Ignore clicks for that long.
    if (_shownFor.isValid() && _shownFor.elapsed() < QApplication::doubleClickInterval()) {
        LOG(Info, "TrayMenu: Ignored a click {} ms after opening.", _shownFor.elapsed());
        event->accept();
        return;
    }
    QMenu::mouseReleaseEvent(event);
}

void TrayMenu::hideEvent(QHideEvent *event)
{
    _backdrop = {};
    QMenu::hideEvent(event);
}

void TrayMenu::paintEvent(QPaintEvent *event)
{
    const auto &colors = Theme::Manager::Instance().Colors();
    {
        QPainter painter{this};
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

        const QRectF surface =
            QRectF{rect()}.adjusted(kSurfaceInset, kSurfaceInset, -kSurfaceInset, -kSurfaceInset);
        QPainterPath shape;
        shape.addRoundedRect(surface, kCornerRadius, kCornerRadius);

        if (_backdrop.isNull()) {
            painter.fillPath(shape, colors.popupSurface);
        }
        else {
            // Brush, not clip: Qt's clipping is binary and would step the corners.
            QBrush backdropBrush{_backdrop};
            backdropBrush.setTransform(QTransform::fromScale(
                qreal(width()) / _backdrop.width(), qreal(height()) / _backdrop.height()));
            painter.fillPath(shape, backdropBrush);
            painter.fillPath(shape, colors.mainGlassTint);
        }

        painter.setPen(QPen{
            colors.dark ? QColor{255, 255, 255, 26} : QColor{0, 0, 0, 20}, 1.0});
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(shape);
    }

    QMenu::paintEvent(event);

    QPainter painter{this};
    const auto check = TintedSvgImage(kCheckIcon, colors.text, kCheckSize, devicePixelRatioF());
    for (auto *action : actions()) {
        if (!action->isVisible() || !action->isCheckable() || !action->isChecked()) {
            continue;
        }
        const auto item = actionGeometry(action);
        painter.drawImage(
            QPointF{
                qreal(item.right() - kItemSidePadding - kCheckSize + 1),
                item.center().y() - kCheckSize / 2.0 + 0.5},
            check);
    }
}

} // namespace Gui
