#include "PanelTheme.h"

#include <cstring>
#include <QStringList>

namespace claudedial::core {
namespace {

/// The value of `key=` inside `[group]`, or empty. Plain INI, one group deep.
QString valueInGroup(const QString& text, const QString& group, const QString& key)
{
    bool inGroup = false;
    for (const QString& raw : text.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inGroup = line.startsWith(group);
            continue;
        }
        if (!inGroup || !line.startsWith(key + QLatin1Char('=')))
            continue;
        return line.mid(key.size() + 1).trimmed();
    }
    return {};
}

} // namespace

double Rgb::luminance() const
{
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

QString plasmaThemeName(const QString& plasmarc, const QString& lookAndFeelDefaults)
{
    const QString explicitName = valueInGroup(plasmarc, QStringLiteral("[Theme]"),
                                              QStringLiteral("name"));
    if (!explicitName.isEmpty())
        return explicitName;

    return valueInGroup(lookAndFeelDefaults, QStringLiteral("[plasmarc][Theme]"),
                        QStringLiteral("name"));
}

QString lookAndFeelPackage(const QString& kdeglobals)
{
    for (const QString& raw : kdeglobals.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1String("LookAndFeelPackage=")))
            return line.mid(int(strlen("LookAndFeelPackage="))).trimmed();
    }
    return {};
}

std::optional<Rgb> plasmaPanelBackground(const QString& themeColors)
{
    const QString value = valueInGroup(themeColors, QStringLiteral("[Colors:Window]"),
                                       QStringLiteral("BackgroundNormal"));
    if (value.isEmpty())
        return std::nullopt;

    const QStringList parts = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 3)
        return std::nullopt;

    Rgb rgb;
    bool ok = false;
    rgb.r = parts.at(0).trimmed().toInt(&ok);
    if (!ok)
        return std::nullopt;
    rgb.g = parts.at(1).trimmed().toInt(&ok);
    if (!ok)
        return std::nullopt;
    rgb.b = parts.at(2).trimmed().toInt(&ok);
    if (!ok)
        return std::nullopt;

    return rgb;
}

} // namespace claudedial::core
