# Compilare e distribuire

*[Read in English](BUILDING.md)*

Come si passa dal sorgente agli eseguibili, e da lì ai pacchetti per chi gioca. Come si gioca
e si prova una volta compilato è nel [README](../README.it.md#giocare-e-testare-riferimento).

## Cosa serve

Il gioco porta con sé tutte le sue librerie (raylib, Luau, ENet, zstd, SQLite, Monocypher,
flecs, Jolt) come submodule in `third_party/`, e le compila insieme al resto. Dal sistema
servono solo il compilatore e le librerie che il driver grafico possiede.

Linux (Ubuntu 24.04; su altre distribuzioni i pacchetti hanno gli stessi nomi o quasi):

```sh
sudo apt install build-essential cmake ninja-build git \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libasound2-dev
```

- **CMake 3.25 o più recente** (lo chiede raylib 6.0), **GCC 13** o Clang con C++20.
- Il client vuole **OpenGL 3.3** e **X11** (su Wayland gira attraverso XWayland). Il server
  non vuole nessuno dei due.

Il sorgente con i submodule:

```sh
git clone --recursive <url>            # oppure, in un clone che già c'è:
git submodule update --init --recursive
```

## Il build di sviluppo

```sh
cmake -S . -B build -G Ninja           # RelWithDebInfo se non si dice altro
cmake --build build -j
ctest --test-dir build                 # 13 test; -LE gpu salta quello che apre una finestra
```

Escono tre programmi:

| | |
|---|---|
| `build/src/client/cnp_client` | il gioco: menù, mondi salvati, multiplayer |
| `build/src/server/cnp_server` | un mondo per altri giocatori, senza finestra |
| `build/tools/cnp_modpack` | un mod in un file `.cnpmod` e ritorno |

Un build di sviluppo legge `mods/`, `games/` e `assets/` dal checkout, quindi gira da dove è
stato compilato e non si copia altrove (`core/paths.h` spiega l'ordine in cui li cerca).
`tools/play.sh` fa un build Release in `build-release/` e avvia server e client insieme.

Opzioni di CMake:

| | |
|---|---|
| `-DCMAKE_BUILD_TYPE=Release` | ottimizzato e senza simboli di debug (il default è `RelWithDebInfo`) |
| `-DCNP_PORTABLE=ON` | il runtime C++ dentro gli eseguibili: un build che gira su un'altra macchina Linux |
| `-DCNP_WERROR=ON` | i warning del nostro codice diventano errori |

Un build che si ferma su `api_generated.inc` vuol dire che `src/script/api.luau` è cambiato:
il generatore (`build/tools/cnp_api_gen`) riscrive `src/script/api_generated.inc`, `docs/API.md`
e `docs/api.d.luau`, che stanno nel repository e vanno committati insieme.

## I pacchetti

Ogni pacchetto è una cartella con i tre programmi e, accanto, `mods/`, `games/` e `assets/`
(`textures`, `sounds`, `lang`): la copia la fa `tools/dist/stage.sh`, ed è la disposizione in
cui `core/paths.h` cerca i file. Un pacchetto si chiama
`cubes-and-portals-<versione>-<sistema>-<architettura>` e finisce in `<build>/dist/`.

### Linux: tarball, AppImage, Flatpak

Tutti e tre vengono dallo stesso build portabile:

```sh
cmake -S . -B build-dist -G Ninja -DCNP_PORTABLE=ON
cmake --build build-dist --target cnp_client cnp_server cnp_modpack
tools/dist/package-linux.sh build-dist        # .tar.gz
tools/dist/package-appimage.sh build-dist     # .AppImage (serve appimagetool)
tools/dist/package-flatpak.sh build-dist      # .flatpak (serve il runtime, sotto)
```

- **Tarball** (~25 MB): si scompatta dove si vuole e si avvia `./cnp_client`. Gira su
  qualsiasi glibc dalla 2.38 in su, cioè dalla macchina su cui è stato compilato in poi: per
  raggiungere distribuzioni più vecchie va compilato su una più vecchia.
  `package-linux.sh` rifiuta un build senza `CNP_PORTABLE`.
- **AppImage** (~23 MB): un file solo. Avviato è il client; `… server` e `… modpack` sono gli
  altri due. `appimagetool` si scarica da
  <https://github.com/AppImage/appimagetool/releases> (continuous) e va nel `PATH` o in
  `$APPIMAGETOOL`.
- **Flatpak** (~17 MB): il tarball dentro `/app/lib/cubes-and-portals`, sul runtime
  `org.freedesktop.Platform//25.08`. Si costruisce con i comandi di flatpak e basta, senza
  flatpak-builder né SDK. Per costruirlo serve il runtime:
  `flatpak install --user flathub org.freedesktop.Platform//25.08`. Chi lo riceve lo installa
  con `flatpak install --user <file>.flatpak`, e il runtime arriva da Flathub. Da lì si avvia
  dal menù del desktop o con `flatpak run io.github.giacomoperin.CubesAndPortals` (anche qui
  `server` e `modpack` come primo argomento). Mondi e opzioni stanno in
  `~/.var/app/io.github.giacomoperin.CubesAndPortals/`.

#### Il repository Flatpak (aggiornamenti con `flatpak update`)

Un Flatpak installato da un file `.flatpak` non si aggiorna mai. Per questo la CI pubblica
anche un **repository** su GitHub Pages (<https://giacomo986.github.io/CubesAndPortals/>):
`package-flatpak.sh` esporta in `build-dist/dist/flatpak-repo/` e, se ha una chiave
(`FLATPAK_GPG_KEY_ID`, e `FLATPAK_GPG_HOMEDIR` se non è quella di gpg), lo firma;
`tools/dist/flatpak-site.sh build-dist site` ne fa il sito: `repo/`, il `.flatpakref` per
installare, il `.flatpakrepo` per aggiungere il remote (tutti e due con la chiave pubblica,
`tools/dist/flatpak-repo.gpg`) e una pagina. Il job `pages` di
`.github/workflows/release.yml` lo pubblica a ogni tag, o a mano dalla scheda Actions.

Serve il segreto `FLATPAK_GPG_KEY` del repository GitHub: la chiave privata in forma
armored, una volta sola:

```sh
gh secret set FLATPAK_GPG_KEY < ~/.config/cubes-and-portals-release/flatpak-repo-secret.asc
```

e Pages acceso con sorgente "GitHub Actions" (Settings, Pages). Senza segreto la release esce
lo stesso, senza repository. La chiave privata va tenuta: con un'altra chiave chi ha già
installato non riceve più aggiornamenti finché non reinstalla.

### Windows: zip

Si compila da Linux, non su Windows: [zig](https://ziglang.org/download/) 0.16 nel `PATH` fa
da compilatore e da MinGW (`tools/dist/zig-windows.cmake`).

```sh
tools/dist/package-windows.sh                 # configura e compila in build-win/, poi lo zip
```

Lo zip (~25 MB) contiene `cnp_client.exe`, `cnp_server.exe`, `cnp_modpack.exe` e i dati. Si
scompatta dove si vuole e si fa doppio clic sul client. Serve Windows 10 o più recente, perché
usa solo DLL di sistema (UCRT, OpenGL32, Winsock). Mondi e opzioni stanno in
`%APPDATA%\cubes-and-portals`. Per provarlo su Linux: `wine cnp_client.exe`. Sotto
wine in Xephyr i clic non arrivano al menù, e un `--quick` o un `--connect` lo provano lo
stesso.

Su Windows mancano ancora alcune cose (TODO.md, OPEN-40): niente stack dump né watchdog, un
client che va in crash chiude il suo server senza salvare, e tornare al menù richiede di nuovo
la password.

### macOS: .app in un .dmg

Si compila **su un Mac**, perché il client ha bisogno dell'SDK di Apple (Cocoa, OpenGL).
Servono gli Xcode command line tools, `cmake` e `ninja` (`brew install cmake ninja`):

```sh
tools/dist/package-macos.sh                   # compila in build-mac/, poi l'app e il .dmg
```

I programmi vanno in `Cubes and Portals.app/Contents/MacOS`, i dati in `Contents/Resources`.
Serve macOS 11 o più recente. Il pacchetto ha l'architettura del Mac che lo compila (arm64 su
Apple silicon). La firma è ad hoc, non di uno sviluppatore: il primo avvio va fatto con clic
destro e Apri, oppure `xattr -dr com.apple.quarantine "Cubes and Portals.app"`.
**Non è mai stato provato su un Mac.**

### Tutti insieme: GitHub Actions

`.github/workflows/release.yml` costruisce tutti i pacchetti: Linux e Windows su Ubuntu, macOS
su un runner Apple silicon. Parte da solo a ogni tag `v*` e si può lanciare a mano dalla
scheda Actions. Su un tag controlla che il tag sia la versione scritta nel codice, e mette i
pacchetti in una GitHub Release (marcata pre-release finché la versione è un'alpha). Il
repository deve stare su GitHub perché tutto questo funzioni.

Nessun pacchetto è firmato: Windows SmartScreen chiede conferma una volta ("Ulteriori
informazioni", poi "Esegui comunque") e macOS Gatekeeper pure.

## Versioni

La versione sta in un posto solo, `kEngineVersion` in `src/core/version.h`. Da lì la leggono
i nomi dei pacchetti, il log di join del server e l'angolo in basso a destra del menù.

Finché il gioco è in alpha la forma è `0.MINORE.0-alpha.N`:

- **N** sale di uno a ogni alpha consegnata (`0.4.0-alpha.1`, `0.4.0-alpha.2`, ...);
- **MINORE** sale quando un'alpha non legge più i mondi salvati della precedente o non gioca
  più con i suoi server, e allora N riparte da 1 (`0.5.0-alpha.1`). Quando succede va scritto
  nel `CHANGELOG.md`, perché chi gioca perde i suoi mondi.

La versione serve a chi legge, non decide nulla. Se un client e un server possono giocare
insieme lo decidono il numero di protocollo (`net/protocol.h`) e l'hash della tabella dei blocchi,
come spiega il commento in `version.h`.

Per fare un'alpha:

1. alzare `kEngineVersion`;
2. aggiungere in cima a `CHANGELOG.md` la nuova voce, con la data e cosa cambia per chi gioca
   (in inglese, e la stessa in italiano in `CHANGELOG.it.md`);
3. fare un commit, poi `git tag v<versione>` (per esempio `git tag v0.4.0-alpha.2`);
4. `git push`, poi `git push origin v<versione>` (`--follow-tags` non porta i tag semplici):
   la CI costruisce i pacchetti e li pubblica. Senza GitHub, si
   lanciano a mano i `package-*.sh` qui sopra e si consegnano i file di `<build>/dist/`.
   Ogni `package-*.sh`, prima di costruire, sposta in `<build>/dist/vecchie/` i pacchetti
   delle altre versioni (`tools/dist/archive-old.sh`): in `dist/` resta solo quella da
   consegnare.
