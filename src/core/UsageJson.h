#pragma once

#include "UsageState.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace claudedial::core {

/// Machine-readable output for `claudedial --json`.
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

/// Reads back what status() wrote.
///
/// Exists so that a one-shot CLI invocation can take its answer from an already
/// running tray instance over the local socket instead of spending an API call:
/// the rate-limit bucket is per access token, so a status bar polling `--json`
/// and the tray polling on its own timer were consuming it twice over.
/// nullopt when the payload carries no usable window.
std::optional<UsageState> parseStatus(const QByteArray& payload);

} // namespace json
} // namespace claudedial::core
