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

## Still to do

- **AppImage** - `linuxdeploy` with the Qt plugin. Needs the QML module and the
  `xcb`/`wayland` platform plugins bundled, and `qt6-svg` if the icon ends up
  being loaded through `QIcon` rather than drawn.
- **.deb / .rpm** - CPack is already available through the CMake install rules;
  `CPACK_GENERATOR="DEB;RPM"` plus dependency metadata is most of the work.
- **Flatpak** - possible, but note that a sandboxed Claudometer needs a
  filesystem override to read `~/.claude/.credentials.json`, which is worth
  thinking about carefully before shipping.
- **GitHub Actions release job** - build on the oldest glibc that is practical,
  attach the AppImage and the tarball to the tag.
