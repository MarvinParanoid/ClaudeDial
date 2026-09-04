#pragma once

namespace claudedial::cli {

/// One-shot, non-graphical entry points: `--json` and `--once`.
///
/// These run under QCoreApplication, never QApplication, so they work over SSH
/// and from a status-bar startup script before any compositor exists.
int run(int argc, char** argv, bool jsonOutput);

} // namespace claudedial::cli
