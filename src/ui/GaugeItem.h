#pragma once

#include <QColor>
#include <QQuickPaintedItem>

namespace claudometer::ui {

/// The popup header's gauge.
///
/// It exists so that the header mark and the tray icon are drawn by the same
/// function rather than by two implementations that merely look alike - the
/// previous QML Canvas version was a second copy of the drawing and would have
/// drifted the first time the mark changed.
class GaugeItem : public QQuickPaintedItem
{
    Q_OBJECT

    Q_PROPERTY(qreal percentage READ percentage WRITE setPercentage NOTIFY changed)
    Q_PROPERTY(bool hasValue READ hasValue WRITE setHasValue NOTIFY changed)
    Q_PROPERTY(QColor dialColor READ dialColor WRITE setDialColor NOTIFY changed)
    Q_PROPERTY(QColor valueColor READ valueColor WRITE setValueColor NOTIFY changed)
    Q_PROPERTY(qreal thicknessScale READ thicknessScale WRITE setThicknessScale NOTIFY changed)

    /// Draw the dial solid, with no usage fill over it.
    ///
    /// For the popup's header mark, which is the logotype: an identity must not
    /// fade because a fetch failed, and with a fill the dial is drawn as the
    /// *unused* part at 20% alpha - so with no data the mark washed out
    /// completely. The needle still moves with usage; only the fill goes.
    Q_PROPERTY(bool identity READ identity WRITE setIdentity NOTIFY changed)

public:
    explicit GaugeItem(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    [[nodiscard]] qreal percentage() const { return m_percentage; }
    void setPercentage(qreal percentage);

    [[nodiscard]] bool hasValue() const { return m_hasValue; }
    void setHasValue(bool hasValue);

    [[nodiscard]] QColor dialColor() const { return m_dialColor; }
    void setDialColor(const QColor& color);

    [[nodiscard]] QColor valueColor() const { return m_valueColor; }
    void setValueColor(const QColor& color);

    [[nodiscard]] qreal thicknessScale() const { return m_thicknessScale; }
    void setThicknessScale(qreal scale);

    [[nodiscard]] bool identity() const { return m_identity; }
    void setIdentity(bool identity);

Q_SIGNALS:
    void changed();

private:
    qreal m_percentage = 0.0;
    bool m_hasValue = false;
    QColor m_dialColor;
    QColor m_valueColor;
    qreal m_thicknessScale = 1.0;
    bool m_identity = false;
};

} // namespace claudometer::ui
