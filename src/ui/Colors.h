#pragma once

#include <QColor>
#include <QObject>

namespace claudedial::ui {

/// The one palette QML reads.
///
/// It exists so that the usage ramp has a single definition shared with the tray
/// renderer, and so that interactive controls can follow the *user's* Plasma
/// accent rather than a colour we invented. Previously the QML theme carried its
/// own copies of both, which is how the popup came to show an accent-blue
/// warning step while the tray showed amber for the same number.
class Colors : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool dark READ dark NOTIFY changed)

    /// ClaudeDial's identity. The header mark, and nothing else.
    Q_PROPERTY(QColor brand READ brand NOTIFY changed)

    /// The user's own desktop accent, for things they interact with.
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)

    Q_PROPERTY(QColor usageNormal READ usageNormal NOTIFY changed)
    Q_PROPERTY(QColor usageWarning READ usageWarning NOTIFY changed)
    Q_PROPERTY(QColor usageCritical READ usageCritical NOTIFY changed)
    Q_PROPERTY(QColor usageSevere READ usageSevere NOTIFY changed)

public:
    explicit Colors(QObject* parent = nullptr);

    [[nodiscard]] bool dark() const { return m_dark; }
    void setDark(bool dark);

    [[nodiscard]] QColor brand() const;
    [[nodiscard]] QColor accent() const;
    [[nodiscard]] QColor usageNormal() const;
    [[nodiscard]] QColor usageWarning() const;
    [[nodiscard]] QColor usageCritical() const;
    [[nodiscard]] QColor usageSevere() const;

    /// Re-read whatever comes from the desktop.
    void refresh();

Q_SIGNALS:
    void changed();

private:
    bool m_dark = false;
    QColor m_accent;
};

} // namespace claudedial::ui
