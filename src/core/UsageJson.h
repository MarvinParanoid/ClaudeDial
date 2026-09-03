#pragma once

#include "UsageState.h"

#include <QByteArray>
#include <QString>

namespace claudometer::core {

/// Machine-readable output for `claudometer --json`.
///
/// Carries percentages and timestamps only: no token, no organisation UUID, no
/// workspace id, no email. Status-bar configs end up in public dotfiles repos.
namespace json {

/// The documented shape, plus the `text`/`tooltip`/`class` keys Waybar's
/// `return-type: json` expects, so a Waybar module needs no wrapper script.
QByteArray status(const UsageState& state, int warningThreshold, int criticalThreshold,
                  const QDateTime& now = QDateTime::currentDateTimeUtc());

/// Emitted instead of a state when there is nothing to report.
QByteArray unavailable(const QString& reason);

} // namespace json
} // namespace claudometer::core
