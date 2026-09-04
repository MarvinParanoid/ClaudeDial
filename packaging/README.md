# Packaging

Distribution artefacts are intentionally not wired into CI yet - the project
installs correctly via CMake first, and each format is added when there is
something worth releasing.

What is here:

- `PKGBUILD` - Arch source package, built from a release tarball.
- `PKGBUILD-bin` - the `claudometer-bin` AUR package, which unpacks the
  prebuilt binary tarball from a GitHub release instead of compiling.

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

## Releasing

One tag drives it. `.github/workflows/release.yml` builds on Ubuntu 22.04 -
chosen so the runner's glibc 2.35 becomes the floor rather than 24.04's 2.39 -
and attaches two artefacts:

```
claudometer-<version>-linux-x86_64.tar.gz   links the system Qt; what claudometer-bin consumes
Claudometer-x86_64.AppImage                 self-contained
```

```console
$ git tag -a v0.1.0 -m "..." && git push origin v0.1.0
```

The workflow reports two things worth reading in its log rather than asserting
them, because both depend on how the Qt it downloaded was built: whether
OpenSSL ended up bundled in the AppImage, and whether QtQuick's QML modules
did. If OpenSSL is absent the AppImage will fail to reach api.anthropic.com on
hosts whose OpenSSL differs, and `linuxdeploy --library` has to add it.

**Do not build the AppImage on a working KDE desktop.** Verified the hard way:
linuxdeploy's Qt plugin deploys everything in Qt's plugin directory, including
third-party plugins such as kimageformats, and then fails on their missing
dependencies. A clean Qt install has none of them, which is why this happens in
CI and not on a developer machine.

## Submitting to the AUR

`PKGBUILD-bin` has been run through `makepkg` locally and installs the right
four files. What remains is per-release:

```console
$ git clone ssh://aur@aur.archlinux.org/claudometer-bin.git
$ cp packaging/PKGBUILD-bin claudometer-bin/PKGBUILD
$ cd claudometer-bin
$ updpkgsums                          # replaces the SKIP checksums
$ makepkg --printsrcinfo > .SRCINFO   # the AUR requires this, and requires it current
$ git add PKGBUILD .SRCINFO && git commit && git push
```

`updpkgsums` needs the release to exist first, since it downloads the tarball to
hash it. The `SKIP` values in the committed file are placeholders and must never
be published to the AUR.

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
