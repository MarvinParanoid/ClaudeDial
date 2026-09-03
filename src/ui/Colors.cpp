#include "Colors.h"

#include "Brand.h"

#include <QGuiApplication>
#include <QPalette>

namespace claudometer::ui {

Colors::Colors(QObject* parent)
    : QObject(parent)
{
    refresh();
}

void Colors::setDark(bool dark)
{
    if (m_dark == dark)
        return;
    m_dark = dark;
    Q_EMIT changed();
}

void Colors::refresh()
{
    const QPalette palette = QGuiApplication::palette();
    // Accent is the role Qt added for exactly this; Highlight is where the
    // information lived before, and still does on some platform themes.
    QColor accent = palette.color(QPalette::Accent);
    if (!accent.isValid())
        accent = palette.color(QPalette::Highlight);
    if (accent != m_accent) {
        m_accent = accent;
        Q_EMIT changed();
    }
}

QColor Colors::brand() const
{
    return brand::kIdentity;
}

QColor Colors::accent() const
{
    return m_accent;
}

QColor Colors::usageNormal() const
{
    // Readable, and clearly not an alert. The percentage stays the largest thing
    // on its row by size; below the warning threshold it carries no colour.
    return m_dark ? QColor(0xc3, 0xca, 0xd3) : QColor(0x4a, 0x50, 0x58);
}

QColor Colors::usageWarning() const
{
    return brand::kUsageWarning;
}

QColor Colors::usageCritical() const
{
    return brand::kUsageCritical;
}

QColor Colors::usageSevere() const
{
    return brand::kUsageSevere;
}

} // namespace claudometer::ui
