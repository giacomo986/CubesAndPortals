# Building and distributing

*[Leggi in italiano](BUILDING.it.md)*

How to get from the source to the executables, and from there to the packages for players. How to
play and test once it is built is in the [README](../README.md#playing-and-testing-reference).

## What you need

The game carries all its libraries (raylib, Luau, ENet, zstd, SQLite, Monocypher, flecs, Jolt) as
submodules in `third_party/`, and builds them along with the rest. From the system you need only
the compiler and the libraries the graphics driver owns.

Linux (Ubuntu 24.04; on other distributions the packages have the same names, or nearly):

```sh
sudo apt install build-essential cmake ninja-build git \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libasound2-dev
```

- **CMake 3.25 or newer** (raylib 6.0 asks for it), **GCC 13** or Clang with C++20.
- The client wants **OpenGL 3.3** and **X11** (on Wayland it runs through XWayland). The server
  wants neither.

The source with its submodules:

```sh
git clone --recursive <url>            # or, in a clone you already have:
git submodule update --init --recursive
```

## The development build

```sh
cmake -S . -B build -G Ninja           # RelWithDebInfo unless told otherwise
cmake --build build -j
ctest --test-dir build                 # 13 tests; -LE gpu skips the one that opens a window
```

Three programs come out:

| | |
|---|---|
| `build/src/client/cnp_client` | the game: menu, saved worlds, multiplayer |
| `build/src/server/cnp_server` | a world for other players, with no window |
| `build/tools/cnp_modpack` | a mod into a `.cnpmod` file and back |

A development build reads `mods/`, `games/` and `assets/` from the checkout, so it runs from where
it was built and is not copied elsewhere (`core/paths.h` explains the order it looks for them in).
`tools/play.sh` makes a Release build in `build-release/` and starts a server and a client together.

CMake options:

| | |
|---|---|
| `-DCMAKE_BUILD_TYPE=Release` | optimised and without debug symbols (the default is `RelWithDebInfo`) |
| `-DCNP_PORTABLE=ON` | the C++ runtime inside the executables: a build that runs on another Linux machine |
| `-DCNP_WERROR=ON` | warnings in our own code become errors |

A build that stops on `api_generated.inc` means `src/script/api.luau` changed: the generator
(`build/tools/cnp_api_gen`) rewrites `src/script/api_generated.inc`, `docs/API.md` and
`docs/api.d.luau`, which are in the repository and are committed together.

## The packages

Every package is a folder with the three programs and, beside them, `mods/`, `games/` and
`assets/` (`textures`, `sounds`, `lang`): `tools/dist/stage.sh` makes the copy, and it is the layout
`core/paths.h` looks for. A package is named `cubes-and-portals-<version>-<system>-<architecture>`
and ends up in `<build>/dist/`.

### Linux: tarball, AppImage, Flatpak

All three come from the same portable build:

```sh
cmake -S . -B build-dist -G Ninja -DCNP_PORTABLE=ON
cmake --build build-dist --target cnp_client cnp_server cnp_modpack
tools/dist/package-linux.sh build-dist        # .tar.gz
tools/dist/package-appimage.sh build-dist     # .AppImage (needs appimagetool)
tools/dist/package-flatpak.sh build-dist      # .flatpak (needs the runtime, below)
```

- **Tarball** (~25 MB): unpack it anywhere and run `./cnp_client`. It runs on any glibc from 2.38
  on, that is from the machine it was built on onwards: to reach older distributions it has to be
  built on an older one. `package-linux.sh` refuses a build without `CNP_PORTABLE`.
- **AppImage** (~23 MB): a single file. Run, it is the client; `… server` and `… modpack` are the
  other two. Download `appimagetool` from <https://github.com/AppImage/appimagetool/releases>
  (continuous) and put it in the `PATH` or in `$APPIMAGETOOL`.
- **Flatpak** (~17 MB): the tarball inside `/app/lib/cubes-and-portals`, on the
  `org.freedesktop.Platform//25.08` runtime. It is built with flatpak's own commands only, without
  flatpak-builder or an SDK. To build it you need the runtime:
  `flatpak install --user flathub org.freedesktop.Platform//25.08`. Whoever receives the file
  installs it with `flatpak install --user <file>.flatpak`, and the runtime comes from Flathub. From
  there it starts from the desktop menu or with `flatpak run io.github.giacomoperin.CubesAndPortals`
  (here too `server` and `modpack` as the first argument). Worlds and options live in
  `~/.var/app/io.github.giacomoperin.CubesAndPortals/`.

#### The Flatpak repository (updates with `flatpak update`)

A Flatpak installed from a `.flatpak` file is never updated. So the CI also publishes a
**repository** on GitHub Pages (<https://giacomo986.github.io/CubesAndPortals/>):
`package-flatpak.sh` exports into `build-dist/dist/flatpak-repo/` and, when it has a key
(`FLATPAK_GPG_KEY_ID`, and `FLATPAK_GPG_HOMEDIR` if not gpg's own), signs it;
`tools/dist/flatpak-site.sh build-dist site` makes the site of it: `repo/`, the `.flatpakref` to
install, the `.flatpakrepo` to add the remote (both carrying the public key,
`tools/dist/flatpak-repo.gpg`) and a page. The `pages` job of `.github/workflows/release.yml`
publishes it on every tag, or by hand from the Actions tab.

It needs the GitHub repository's `FLATPAK_GPG_KEY` secret: the private key, armoured, once:

```sh
gh secret set FLATPAK_GPG_KEY < ~/.config/cubes-and-portals-release/flatpak-repo-secret.asc
```

and Pages turned on with "GitHub Actions" as its source (Settings, Pages). Without the secret a
release is made just the same, with no repository. Keep the private key: with another key,
whoever has installed gets no more updates until they install again.

### Windows: zip

It is built from Linux, not on Windows: [zig](https://ziglang.org/download/) 0.16 in the `PATH` is
the compiler and the MinGW (`tools/dist/zig-windows.cmake`).

```sh
tools/dist/package-windows.sh                 # configures and builds in build-win/, then the zip
```

The zip (~25 MB) holds `cnp_client.exe`, `cnp_server.exe`, `cnp_modpack.exe` and the data. Unpack
it anywhere and double-click the client. It needs Windows 10 or newer, since it uses only system
DLLs (UCRT, OpenGL32, Winsock). Worlds and options live in `%APPDATA%\cubes-and-portals`. To try it
on Linux: `wine cnp_client.exe`. Under wine in Xephyr clicks do not reach the menu, and a `--quick`
or a `--connect` tries it just the same.

On Windows a few things are still missing (TODO.md, OPEN-40): no stack dump or watchdog, a client
that crashes closes its server without saving, and going back to the menu asks for the password
again.

### macOS: .app in a .dmg

It is built **on a Mac**, because the client needs Apple's SDK (Cocoa, OpenGL). You need the Xcode
command line tools, `cmake` and `ninja` (`brew install cmake ninja`):

```sh
tools/dist/package-macos.sh                   # builds in build-mac/, then the app and the .dmg
```

The programs go into `Cubes and Portals.app/Contents/MacOS`, the data into `Contents/Resources`. It
needs macOS 11 or newer. The package has the architecture of the Mac that builds it (arm64 on Apple
silicon). The signature is ad hoc, not a developer's: the first run has to be right-click and
Open, or `xattr -dr com.apple.quarantine "Cubes and Portals.app"`. **It has never been tried on a
Mac.**

### All together: GitHub Actions

`.github/workflows/release.yml` builds every package: Linux and Windows on Ubuntu, macOS on an
Apple silicon runner. It starts by itself on every `v*` tag and can be run by hand from the
Actions tab. On a tag it checks that the tag is the version written in the code, and puts the
packages in a GitHub Release (a pre-release while the version is an alpha or a beta) whose notes
are that version's section of `CHANGELOG.md`. The repository has to be on GitHub for all this to
work.

No package is signed: Windows SmartScreen asks for confirmation once ("More info", then "Run
anyway") and macOS Gatekeeper does too.

## Versions

The version is in one place only, `kEngineVersion` in `src/core/version.h`. From there the
packages' names, the server's join log and the bottom right corner of the menu read it.

While the game is in alpha its form is `0.MINOR.0-alpha.N`:

- **N** goes up by one with every alpha handed out (`0.4.0-alpha.1`, `0.4.0-alpha.2`, ...);
- **MINOR** goes up when an alpha no longer reads the previous one's saved worlds or no longer
  plays with its servers, and then N starts again from 1 (`0.5.0-alpha.1`). When that happens it
  is written in `CHANGELOG.md`, because whoever plays loses their worlds.

The version is for whoever reads it, it decides nothing. Whether a client and a server can play
together is decided by the protocol number (`net/protocol.h`) and the block table's hash, as the
comment in `version.h` explains.

To make an alpha:

1. raise `kEngineVersion`;
2. add the new entry at the top of `CHANGELOG.md`, with the date and what changes for players
   (and the same in `CHANGELOG.it.md`);
3. commit, then `git tag v<version>` (for example `git tag v0.4.0-alpha.2`);
4. `git push`, then `git push origin v<version>` (`--follow-tags` does not carry plain tags): the
   CI builds the packages and publishes them. Without GitHub, run the `package-*.sh` above by hand
   and hand over the files in `<build>/dist/`. Every `package-*.sh`, before building, moves the
   other versions' packages into `<build>/dist/vecchie/` (`tools/dist/archive-old.sh`): only the
   one to hand over stays in `dist/`.
