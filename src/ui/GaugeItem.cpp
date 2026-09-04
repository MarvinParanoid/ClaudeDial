#include "GaugeItem.h"

#include "GaugePainter.h"

#include <QPainter>

namespace claudedial::ui {

GaugeItem::GaugeItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void GaugeItem::paint(QPainter* painter)
{
    const gauge::Colors colors { m_dialColor, m_valueColor };
    gauge::paint(*painter, QRectF(0, 0, width(), height()),
                 m_hasValue ? std::optional<double>(m_percentage) : std::nullopt, colors,
                 m_thicknessScale, gauge::Center::Needle,
                 m_identity ? gauge::Fill::None : gauge::Fill::Usage);
}

void GaugeItem::setPercentage(qreal percentage)
{
    if (qFuzzyCompare(m_percentage, percentage))
        return;
    m_percentage = percentage;
    update();
    Q_EMIT changed();
}

void GaugeItem::setHasValue(bool hasValue)
{
    if (m_hasValue == hasValue)
        return;
    m_hasValue = hasValue;
    update();
    Q_EMIT changed();
}

void GaugeItem::setIdentity(bool identity)
{
    if (m_identity == identity)
        return;
    m_identity = identity;
    update();
    Q_EMIT changed();
}

void GaugeItem::setThicknessScale(qreal scale)
{
    if (qFuzzyCompare(m_thicknessScale, scale))
        return;
    m_thicknessScale = scale;
    update();
    Q_EMIT changed();
}

void GaugeItem::setDialColor(const QColor& color)
{
    if (m_dialColor == color)
        return;
    m_dialColor = color;
    update();
    Q_EMIT changed();
}

void GaugeItem::setValueColor(const QColor& color)
{
    if (m_valueColor == color)
        return;
    m_valueColor = color;
    update();
    Q_EMIT changed();
}

} // namespace claudedial::ui
