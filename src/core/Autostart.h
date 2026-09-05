#pragma once

#include <QString>

namespace claudedial::core {

/// Starting at login, which is the one thing every platform spells differently.
///
/// Windows keeps a registry value, macOS a LaunchAgent plist, and everything
/// else an XDG .desktop entry. None of that belongs in Config, which is
/// otherwise free of platform assumptions - so it lives here, in the one file
/// a fourth platform would have to touch.
namespace autostart {

/// True when an entry exists. Reads the real thing rather than a stored flag,
/// so an entry removed behind our back is reported honestly.
[[nodiscard]] bool isEnabled();

/// Creates or removes the entry. Returns false when it could not be written -
/// the caller must not claim success, because nothing would start at login.
bool setEnabled(bool enabled);

/// Where the entry lives: a path, or a description of the registry value.
/// For diagnostics and tests. Never a secret, never a credential.
[[nodiscard]] QString location();

} // namespace autostart
} // namespace claudedial::core
