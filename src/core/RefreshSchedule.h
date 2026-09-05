#pragma once

#include "UsageState.h"

#include <QDateTime>

namespace claudedial::core {

/// When to poll next, as a pure function of what is known.
///
/// Extracted from UsageService so it can be tested at all. The interesting half
/// of it is the rate-limit ladder, which by definition only ever runs when
/// something has already gone wrong - so in production it is exercised for the
/// first time on the day it matters most. That is the wrong place to find out
/// whether the numbers are right.
///
/// Two behaviours worth stating, because both are decisions rather than
/// consequences:
///
/// - Every rule can only ever *lengthen* the wait, never shorten it. A server
///   that has just refused us is not somewhere to hurry back to, and the
///   configured interval is a floor the user chose.
/// - Reset alignment is skipped entirely while rate-limited. Polling on the
///   stroke of a new window is exactly the burst a rate limiter is objecting
///   to.
[[nodiscard]] qint64 nextRefreshMs(int refreshIntervalSeconds, int rateLimitStrikes,
                                   int retryAfterSeconds, const UsageState& state,
                                   const QDateTime& now);

/// The ladder, in minutes, capped at its last entry. Exposed so a test can
/// assert against the real numbers rather than a copy of them.
[[nodiscard]] int rateLimitBackoffMinutes(int strikes);

} // namespace claudedial::core
