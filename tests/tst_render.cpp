// The tray mark, asserted rather than looked at.
//
// Everything here was checked by eye during design, and then by a throwaway
// program each time it changed - which is not a test, because nobody reruns it.
// The properties below are the ones those programs measured, turned into
// something that fails on its own.
//
// Separate from tst_core because it needs QtGui: core deliberately links only
// Qt Core and Network, which is what keeps it testable headlessly.

#include "Brand.h"
#include "core/UsageLevel.h"
#include "tray/IconRenderer.h"

#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QTest>

using namespace claudedial;
using Style = core::Config::TrayStyle;

namespace {

QImage render(int size, std::optional<double> percentage, Style style, bool stale = false)
{
    tray::IconRenderer::Options options;
    options.style = style;
    options.foreground = QColor(0xdc, 0xdc, 0xdc);
    options.stale = stale;
    return tray::IconRenderer::render(percentage, options).pixmap(size, size).toImage();
}

/// How much of the bottom rows is inked. The small-size percentage mark puts a
/// usage bar there; the dial has its opening there and leaves it empty.
double bottomFill(const QImage& image, int rows)
{
    int inked = 0;
    int total = 0;
    for (int y = image.height() - rows; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x, ++total)
            if (qAlpha(image.pixel(x, y)) > 40)
                ++inked;
    return total > 0 ? double(inked) / total : 0.0;
}

/// The width of whatever is drawn inside the dial - the number, in practice.
int centreInkWidth(const QImage& image)
{
    const int from = image.width() / 4;
    const int to = image.width() - from;
    int left = image.width();
    int right = -1;
    for (int y = from; y < to; ++y)
        for (int x = from; x < to; ++x)
            if (qAlpha(image.pixel(x, y)) > 40) {
                left = qMin(left, x);
                right = qMax(right, x);
            }
    return right < 0 ? 0 : right - left + 1;
}

QRgb dominantColour(const QImage& image)
{
    QHash<QRgb, int> counts;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qAlpha(image.pixel(x, y)) > 200)
                ++counts[image.pixel(x, y) | 0xff000000u];

    QRgb best = 0;
    int most = 0;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        if (it.value() > most) {
            most = it.value();
            best = it.key();
        }
    return best;
}

double meanAlpha(const QImage& image)
{
    double sum = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            sum += qAlpha(image.pixel(x, y));
    return sum / (image.width() * image.height());
}

/// Whether this machine can draw text at all. A container with no fonts
/// installed renders nothing, and the digit assertions would then fail for a
/// reason that has nothing to do with the code.
bool digitsRender()
{
    return centreInkWidth(render(22, 63, Style::Percentage)) > 0;
}

} // namespace

class RenderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bakesTheSizesPanelsAskFor();
    void swapsTheMarkBelowSeventeenPixels();
    void showsOneGlyphRatherThanThreeDigitsAtTheLimit();
    void paintsTheUsageRamp();
    void fadesWhenStale();
    void drawsNoReadingWithoutData();
};

void RenderTest::bakesTheSizesPanelsAskFor()
{
    // A StatusNotifierItem host is never asked what size it wants: it receives
    // this set and picks. 15 is here because i3bar asks for exactly that, and
    // without it Qt scales 16 down and the result is soft.
    QList<int> widths;
    for (const QSize& size : tray::IconRenderer::render(63, {}).availableSizes())
        widths << size.width();
    std::sort(widths.begin(), widths.end());
    QCOMPARE(widths, QList<int>({ 15, 16, 22, 24, 32, 48, 64 }));
}

void RenderTest::swapsTheMarkBelowSeventeenPixels()
{
    if (!digitsRender())
        QSKIP("no font on this machine, so the small mark cannot be drawn");

    // Below the threshold the percentage style gives up the dial: the number
    // takes the box and a bar carries the reading. The bar is the difference
    // that can be measured - it fills the bottom rows, where the dial's own
    // opening leaves them empty.
    // Stated as a ratio rather than two absolute thresholds. The bar is a solid
    // block and the dial's opening is empty, so the difference is large on any
    // machine - but a font with a deep descender can tint the bottom rows, and
    // an absolute "< 0.05" would then fail for a reason that has nothing to do
    // with the mark.
    const double small = bottomFill(render(16, 63, Style::Percentage), 2);
    const double large = bottomFill(render(22, 63, Style::Percentage), 2);
    const auto measured = [&](const char* what) {
        return qPrintable(QStringLiteral("%1: small=%2 large=%3")
                              .arg(QLatin1String(what))
                              .arg(small, 0, 'f', 3)
                              .arg(large, 0, 'f', 3));
    };
    QVERIFY2(small > 0.5, measured("the small mark should carry a usage bar"));
    QVERIFY2(small > large * 4, measured("the designed mark leaves its bottom to the dial"));

    // The gauge style keeps its dial at every size, so it never grows a bar.
    // No text is involved here, so this one can be absolute.
    QVERIFY2(bottomFill(render(16, 63, Style::Gauge), 2) < 0.05,
             "the gauge style keeps the dial below the threshold, not a bar");

    // The boundary itself, since it is a documented number.
    const double justAbove = bottomFill(render(17, 63, Style::Percentage), 2);
    QVERIFY2(justAbove < small / 4,
             qPrintable(QStringLiteral("16px=%1 should carry a bar and 17px=%2 should not")
                            .arg(small, 0, 'f', 3)
                            .arg(justAbove, 0, 'f', 3)));
}

void RenderTest::showsOneGlyphRatherThanThreeDigitsAtTheLimit()
{
    if (!digitsRender())
        QSKIP("no font on this machine");

    // Three digits do not fit inside the dial at tray sizes, so 100% is drawn
    // as a single glyph. Measured inside the arc, where the number lives.
    const int two = centreInkWidth(render(22, 99, Style::Percentage));
    const int limit = centreInkWidth(render(22, 100, Style::Percentage));
    QVERIFY2(limit > 0, "something should be drawn at the limit");
    QVERIFY2(limit < two / 2,
             qPrintable(QStringLiteral("one narrow glyph expected, not three digits: "
                                       "99%% is %1px wide, 100%% is %2px")
                            .arg(two)
                            .arg(limit)));
}

void RenderTest::paintsTheUsageRamp()
{
    // Two assertions, because one of them alone proves nothing. Comparing the
    // pixels against brand::* shows the renderer uses the ramp - but if the ramp
    // itself changed, both sides would move together and say nothing. Verified
    // by mutation: altering kUsageWarning left this test green until the literal
    // values below were added.
    QCOMPARE(brand::kUsageWarning.name(), QStringLiteral("#fdbc4b"));
    QCOMPARE(brand::kUsageCritical.name(), QStringLiteral("#f0842c"));
    QCOMPARE(brand::kUsageSevere.name(), QStringLiteral("#da4453"));

    // One ramp, shared with the popup and with --json. These are the steps, at
    // the default thresholds, read off the pixels rather than from the header
    // that defines them.
    QCOMPARE(dominantColour(render(22, 50, Style::Gauge)), QColor(0xdc, 0xdc, 0xdc).rgb());
    QCOMPARE(dominantColour(render(22, 80, Style::Gauge)), brand::kUsageWarning.rgb());
    QCOMPARE(dominantColour(render(22, 93, Style::Gauge)), brand::kUsageCritical.rgb());
    QCOMPARE(dominantColour(render(22, 99, Style::Gauge)), brand::kUsageSevere.rgb());
}

void RenderTest::fadesWhenStale()
{
    // Stale is drawn as opacity over whatever the colour is, never as a colour
    // of its own - so it reads the same on any panel and at any usage level.
    const double fresh = meanAlpha(render(22, 63, Style::Gauge));
    const double stale = meanAlpha(render(22, 63, Style::Gauge, true));
    QVERIFY(fresh > 0);
    const double ratio = stale / fresh;
    QVERIFY2(ratio > 0.35 && ratio < 0.6, qPrintable(QStringLiteral("ratio %1").arg(ratio)));
}

void RenderTest::drawsNoReadingWithoutData()
{
    // No data is not zero. An empty dial says "unknown"; a full-looking mark at
    // 0% would say "nothing used", which is a different and wrong claim.
    const QImage empty = render(22, std::nullopt, Style::Gauge);
    const QImage zero = render(22, 0, Style::Gauge);
    QVERIFY(meanAlpha(empty) > 0);
    QVERIFY2(meanAlpha(empty) < meanAlpha(zero) + 1.0,
             "the no-data mark must not carry more ink than a real reading");
}

QTEST_MAIN(RenderTest)
#include "tst_render.moc"
