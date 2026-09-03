# Packaging

Distribution artefacts are intentionally not wired into CI yet - the project
installs correctly via CMake first, and each format is added when there is
something worth releasing.

What is here:

- `PKGBUILD` - Arch source package. The AUR target is `claudometer-bin`, which
  will consume the release tarball from GitHub instead of building from source.

What `cmake --install` lays down, which is what every format needs to package:

```
/usr/bin/claudometer
/usr/share/applications/claudometer.desktop
/usr/share/icons/hicolor/scalable/apps/claudometer.svg
```

Runtime dependencies are only `qt6-base` and `qt6-declarative`. There is no
Electron, webview, Python or Node runtime to ship.

## CPack

Configured in the top-level `CMakeLists.txt`. From a build directory:

```console
$ cpack -G TGZ    # the release tarball an AUR -bin package consumes
$ cpack -G DEB
$ cpack -G RPM
```

TGZ works anywhere. **DEB must be built on a Debian-ish host**: dependencies are
resolved by `dpkg-shlibdeps` from the binary itself, because hard-coding them is
a losing game - Debian and Ubuntu disagree on the Qt 6 package names, and
Ubuntu's 64-bit time_t transition renamed them again. Without `dpkg` on PATH
CPack still emits a `.deb`, but one with no dependency metadata and a guessed
architecture, which must not be published. RPM needs `rpmbuild`.

## Still to do

- **AppImage** - `linuxdeploy` with the Qt plugin. Needs the `wayland`/`xcb`
  platform plugins and the QtQuick runtime bundled. The QML is already inside
  the binary as a Qt resource, and the icons are drawn rather than loaded, so
  no `qt6-svg`.
- **Flatpak** - possible, but note that a sandboxed Claudometer needs a
  filesystem override to read `~/.claude/.credentials.json`, which is worth
  thinking about carefully before shipping.
- **GitHub Actions release job** - build on the oldest glibc that is practical,
  attach the AppImage and the tarball to the tag.
