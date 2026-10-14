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

#include "FrostedBackdrop.h"

#include <algorithm>

#include <QColor>

namespace Gui {
namespace {

// Blurring a 1/10-size copy and scaling it back up gives a wide, smooth blur for almost no cost.
constexpr int kDownscale = 10;
constexpr int kBlurPasses = 4;
constexpr float kSaturation = 1.8f;

// One 3-tap box blur along a single axis, edges clamped.
void BoxBlurPass(const QImage &source, QImage &target, bool horizontal)
{
    const int width = source.width();
    const int height = source.height();
    for (int y = 0; y < height; ++y) {
        auto *out = reinterpret_cast<QRgb *>(target.scanLine(y));
        for (int x = 0; x < width; ++x) {
            int red = 0, green = 0, blue = 0;
            for (int offset = -1; offset <= 1; ++offset) {
                const int sx = horizontal ? std::clamp(x + offset, 0, width - 1) : x;
                const int sy = horizontal ? y : std::clamp(y + offset, 0, height - 1);
                const QRgb pixel = reinterpret_cast<const QRgb *>(source.constScanLine(sy))[sx];
                red += qRed(pixel);
                green += qGreen(pixel);
                blue += qBlue(pixel);
            }
            out[x] = qRgb(red / 3, green / 3, blue / 3);
        }
    }
}

void Saturate(QImage &image)
{
    for (int y = 0; y < image.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float hue{}, saturation{}, lightness{};
            QColor::fromRgb(line[x]).getHslF(&hue, &saturation, &lightness);
            if (saturation <= 0.0f) {
                continue; // achromatic: nothing to boost, and its hue is -1
            }
            saturation = std::min(1.0f, saturation * kSaturation);
            line[x] = QColor::fromHslF(hue, saturation, lightness).rgb();
        }
    }
}

} // namespace

QImage FrostBackdrop(const QImage &source)
{
    if (source.isNull()) {
        return {};
    }

    const QSize fullSize = source.size();
    const QSize smallSize{
        std::max(1, fullSize.width() / kDownscale), std::max(1, fullSize.height() / kDownscale)};

    QImage image = source.convertToFormat(QImage::Format_RGB32)
                       .scaled(smallSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage scratch{image.size(), QImage::Format_RGB32};
    for (int pass = 0; pass < kBlurPasses; ++pass) {
        BoxBlurPass(image, scratch, true);
        BoxBlurPass(scratch, image, false);
    }
    Saturate(image);

    return image.scaled(fullSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace Gui
