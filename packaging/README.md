# Packaging

Distribution artefacts are intentionally not wired into CI yet - the project
installs correctly via CMake first, and each format is added when there is
something worth releasing.

What is here:

- `PKGBUILD` - Arch source package, built from a release tarball.
- `PKGBUILD-bin` - the `claudedial-bin` AUR package, which unpacks the
  prebuilt binary tarball from a GitHub release instead of compiling.

What `cmake --install` lays down, which is what every format needs to package:

```
/usr/bin/claudedial
/usr/share/applications/claudedial.desktop
/usr/share/icons/hicolor/scalable/apps/claudedial.svg
```

Runtime dependencies are `qt6-base`, `qt6-declarative` and `qt6-svg`. The last
one is easy to talk yourself out of, because `ldd` shows no link against
`libQt6Svg` - but the application icon is an SVG loaded from a Qt resource, and
the plugin that renders it (`iconengines/libqsvgicon.so`) is dlopened at
runtime. Only the *tray* icon is drawn rather than loaded. There is no Electron,
webview, Python or Node runtime to ship.

## CPack

Configured in the top-level `CMakeLists.txt`. From a build directory:

```console
$ cpack -G TGZ    # the release tarball an AUR -bin package consumes
$ cpack -G DEB
$ cpack -G RPM
```

**What the tarball needs on the host.** Qt 6.8 or newer, including QtQuick and
QtQuick Controls. On Arch that is `qt6-base`, `qt6-declarative` and `qt6-svg`,
which is what the AUR package depends on; the names differ elsewhere, and
`ldd claudedial-*/bin/claudedial | grep 'not found'` names whatever is missing
on a given machine. A Debian box with no Qt stops at
`libQt6QuickControls2.so.6`.

TGZ builds anywhere, but note what it is: an install tree that links the
system Qt, not a portable bundle. On a Debian box with no Qt installed it stops
at `libQt6QuickControls2.so.6`, which is worth saying in release notes because
"tarball" invites the opposite assumption. The AppImage is the portable one, and
even that needs FUSE on the host unless it is run with
`--appimage-extract-and-run`.

**DEB must be built on a Debian-ish host**: dependencies are
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
claudedial-<version>-linux-x86_64.tar.gz   links the system Qt; what claudedial-bin consumes
ClaudeDial-x86_64.AppImage                 self-contained
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

**Blocked upstream, not on our side.** AUR account registration is paused while
they handle a wave of automated account creation; the page answers HTTP 503 and
asks explicitly that nobody script retries against it. Reopening is announced on
`aur-general` and the Arch news feed, and nothing here will know sooner.

Until then Arch users install by downloading `PKGBUILD-bin` from this repository
and running `makepkg -si`, which the README documents and which is verified to
produce the same package the AUR would serve.

Two prerequisites, and only the first is visible from the error message if it is
missing. `ssh` must be told which key to offer - AUR takes no default and
`Permission denied (publickey)` is all it says, with no `Offering public key`
line in `ssh -v` output:

```
Host aur.archlinux.org
    HostName aur.archlinux.org
    User aur
    IdentityFile ~/.ssh/aur
    IdentitiesOnly yes
```

Second, that public key has to be pasted into the AUR account's *SSH Public Key*
field. `ssh aur@aur.archlinux.org` answers `Welcome to AUR, <user>!` and closes
when both are right; there is no shell there, so that is success.

`PKGBUILD-bin` has been run through `makepkg` locally and installs the right
four files. What remains is per-release:

Clone it **beside this repository, not inside it**: the AUR package is a separate
git repository with its own remote and history, and its root must contain only
`PKGBUILD` and `.SRCINFO`. A nested clone would either be committed here by
accident or refused as a stray repository.

```console
$ cd ..
$ git clone ssh://aur@aur.archlinux.org/claudedial-bin.git
$ cp ClaudeDial/packaging/PKGBUILD-bin claudedial-bin/PKGBUILD
$ cp ClaudeDial/packaging/.SRCINFO     claudedial-bin/
$ cd claudedial-bin
$ git add PKGBUILD .SRCINFO && git commit -m "Initial import" && git push
```

The clone prints `warning: you appear to have cloned an empty repository` for a
package that does not exist yet. That is expected.

`PKGBUILD-bin` and `.SRCINFO` in this directory are the master copies and carry
the real checksums for the current release, so nothing needs regenerating for a
first import. **For each later release**, refresh both before copying:

```console
$ updpkgsums PKGBUILD-bin              # or sha256sum the two published files
$ makepkg --printsrcinfo > .SRCINFO    # the AUR requires this, and requires it current
```

`updpkgsums` needs the release to exist first, since it downloads the tarball to
hash it.

## Still to do

- **AppImage** - done; see Releasing above.
- **Flatpak** - possible, but note that a sandboxed ClaudeDial needs a
  filesystem override to read `~/.claude/.credentials.json`, which is worth
  thinking about carefully before shipping.
(The release job is done - see Releasing above. It builds on Ubuntu 22.04 and
attaches both artefacts to the tag.)
