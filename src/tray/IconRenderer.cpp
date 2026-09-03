#include "IconRenderer.h"

#include "GaugePainter.h"

#include <QFont>
#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace claudometer::tray {
namespace {

/// Sizes a Linux panel actually asks for. Each is rendered directly so the mark
/// stays crisp instead of being downscaled from one large pixmap.
constexpr int kSizes[] = { 16, 22, 24, 32, 48, 64 };

/// Ring stroke and digit size, as fractions of the icon.
///
/// Settled by rendering the combinations at 16, 20, 22 and 24 px and reading
/// them in a real panel. The digits have to be comfortable to read when you look
/// at Claudometer, without being the loudest thing in the whole tray - bigger or
/// bolder than this and the icon starts reading as a notification badge. There
/// has to be visible air between the digits and the ring.
constexpr double kRingThickness = 0.08;
constexpr double kDigitSize = 0.58;

/// DemiBold rather than Bold: at 16-22 px Bold fills the counters and looks
/// blunt, while Medium goes thin enough to fade at 16 px, particularly in the
/// warning and critical colours.
constexpr QFont::Weight kDigitWeight = QFont::DemiBold;

/// Three digits do not fit legibly, and "at the limit" is better said than
/// counted.
constexpr auto kLimitGlyph = "!";

// Breeze-adjacent, so the icon does not look foreign on Plasma while staying
// legible on GNOME's and XFCE's panels.
//
// NOTE: the popup uses a longer ramp with an accent step (see Theme.qml). That
// is deliberate, not an oversight: a panel icon is on screen permanently and
// must not be a standing splash of colour, whereas the popup is only visible
// while the user is looking at it.
const QColor kWarning { 0xfd, 0xbc, 0x4b };
const QColor kCritical { 0xda, 0x44, 0x53 };

} // namespace

QColor IconRenderer::colorFor(double percentage, const Options& options)
{
    if (percentage >= options.criticalThreshold)
        return kCritical;
    if (percentage >= options.warningThreshold)
        return kWarning;
    return options.foreground; // monochrome until something needs attention
}

void IconRenderer::paintRingWithNumber(QPainter& painter, int size, double percentage,
                                       const Options& options)
{
    const QColor value = colorFor(percentage, options);
    const double stroke = std::max(1.0, size * kRingThickness);
    const double inset = stroke / 2.0 + size * 0.02;
    const QRectF ring = QRectF(0, 0, size, size).adjusted(inset, inset, -inset, -inset);

    QColor track = options.foreground;
    track.setAlphaF(0.22f);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(track, stroke, Qt::SolidLine, Qt::FlatCap));
    painter.drawEllipse(ring);

    painter.setPen(QPen(value, stroke, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(ring, 90 * 16, -static_cast<int>(percentage / 100.0 * 360 * 16));

    const QString text = percentage >= 100.0 ? QString::fromLatin1(kLimitGlyph)
                                             : QString::number(qRound(percentage));
    QFont font;
    font.setWeight(kDigitWeight);
    font.setPixelSize(static_cast<int>(size * kDigitSize));
    painter.setFont(font);
    painter.setPen(QPen(value));
    painter.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, text);
}

QPixmap IconRenderer::renderPixmap(int size, std::optional<double> percentage,
                                   const Options& options)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // With no data there is no number to show, so both modes fall back to the
    // empty dial - "unknown" looks the same however the icon is configured.
    if (percentage && options.showPercentage) {
        paintRingWithNumber(painter, size, std::clamp(*percentage, 0.0, 100.0), options);
        return pixmap;
    }

    const gauge::Colors colors {
        options.foreground,
        percentage ? colorFor(*percentage, options) : options.foreground,
    };
    gauge::paint(painter, QRectF(0, 0, size, size), percentage, colors);

    return pixmap;
}

QIcon IconRenderer::render(std::optional<double> percentage, const Options& options)
{
    QIcon icon;
    for (const int size : kSizes)
        icon.addPixmap(renderPixmap(size, percentage, options));
    return icon;
}

} // namespace claudometer::tray
