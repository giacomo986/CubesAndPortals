# Compilar e distribuir

*[Read in English](BUILDING.md) · [Leggi in italiano](BUILDING.it.md)*

Como ir do código-fonte aos executáveis, e daí aos pacotes para quem joga. Como jogar e testar
depois de compilado está no [README](../README.pt.md#jogar-e-testar-referência).

## O que é preciso

O jogo traz todas as suas bibliotecas (raylib, Luau, ENet, zstd, SQLite, Monocypher, flecs, Jolt)
como submódulos em `third_party/`, e as compila junto com o resto. Do sistema você só precisa do
compilador e das bibliotecas que pertencem ao driver de vídeo.

Linux (Ubuntu 24.04; em outras distribuições os pacotes têm os mesmos nomes, ou quase):

```sh
sudo apt install build-essential cmake ninja-build git \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libasound2-dev
```

- **CMake 3.25 ou mais novo** (o raylib 6.0 pede), **GCC 13** ou Clang com C++20.
- O cliente quer **OpenGL 3.3** e **X11** (no Wayland roda pelo XWayland). O servidor não quer
  nenhum dos dois.

O código-fonte com os submódulos:

```sh
git clone --recursive <url>            # ou, num clone que você já tem:
git submodule update --init --recursive
```

## A compilação de desenvolvimento

```sh
cmake -S . -B build -G Ninja           # RelWithDebInfo se nada for dito
cmake --build build -j
ctest --test-dir build                 # 18 testes; -LE gpu pula o que abre uma janela
```

Saem quatro programas:

| | |
|---|---|
| `build/src/launcher/cnp_launcher` | o launcher: versões, mundos, mods; o que os pacotes abrem |
| `build/src/client/cnp_client` | o jogo: menu, mundos salvos, multijogador |
| `build/src/server/cnp_server` | um mundo para outros jogadores, sem janela |
| `build/tools/cnp_modpack` | um mod num arquivo `.cnpmod` e de volta |

Uma compilação de desenvolvimento lê `mods/`, `games/` e `assets/` do checkout, então roda de onde
foi compilada e não é copiada para outro lugar (`core/paths.h` explica em que ordem os procura).
`tools/play.sh` faz uma compilação Release em `build-release/` e inicia um servidor e um cliente
juntos.

Opções do CMake:

| | |
|---|---|
| `-DCMAKE_BUILD_TYPE=Release` | otimizado e sem símbolos de depuração (o padrão é `RelWithDebInfo`) |
| `-DCNP_PORTABLE=ON` | o runtime C++ dentro dos executáveis: uma compilação que roda em outra máquina Linux |
| `-DCNP_WERROR=ON` | os warnings do nosso código viram erros |

Uma compilação que para em `api_generated.inc` quer dizer que `src/script/api.luau` mudou: o
gerador (`build/tools/cnp_api_gen`) reescreve `src/script/api_generated.inc`, `docs/API.md` e
`docs/api.d.luau`, que estão no repositório e vão no mesmo commit.

`tools/typecheck.sh` verifica os tipos de cada mod contra `docs/api.d.luau` e o compila como o
motor faz; na primeira vez ele compila em `build-typecheck/` o verificador de que precisa. Rode-o
depois de mexer num mod ou na API: ele não deve imprimir nada além da contagem. O que ele verifica e
o que deixa de fora: [MODDING.pt.md](MODDING.pt.md#verificar-os-tipos-toolstypechecksh).

### Warnings e sanitizers

O nosso código -- não `third_party/` -- compila com `-Wall -Wextra -Wpedantic` e uns trinta outros
(`cnp_warnings` em `CMakeLists.txt`), cada um ligado só depois que a árvore inteira não acusava
nada com ele. Os poucos deixados desligados estão listados lá com o que acusavam. raylib, flecs,
sqlite e cgltf são incluídos como headers do sistema, então o que esses warnings dizem dos headers
deles não é problema nosso. Uma mudança deve compilar com `-DCNP_WERROR=ON`; a compilação para
Windows (Clang, via zig) tem a mesma lista sem os que só existem no GCC.

Uma compilação `Debug` também define `_GLIBCXX_ASSERTIONS`: um índice além do fim de um
`std::vector`, `front()` de um vazio ou um iterador inválido param o programa naquela linha, onde
qualquer outra compilação lê a memória que estiver lá.

AddressSanitizer e UndefinedBehaviorSanitizer, numa compilação à parte:

```sh
SAN="-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer"
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      "-DCMAKE_CXX_FLAGS=$SAN" "-DCMAKE_C_FLAGS=$SAN"
cmake --build build-asan -j
ctest --test-dir build-asan -LE gpu
```

Um problema encontrado para o programa com a linha e a pilha; os testes que rodam o servidor de
verdade guardam o log dele em `build-asan/test-logs/`. Duas coisas ali são conhecidas e não são
problemas: `left shift of 1 by 31 places` vem de `third_party/enet`, e `two_process_join` e
`mods_play` podem estourar o tempo -- uma compilação Debug sob ASan é tão lenta que uma sonda cansa
de esperar os primeiros chunks (os logs guardados então não mostram relatório nenhum).

## Os pacotes

Todo pacote é uma pasta com os quatro programas e, ao lado deles, `mods/`, `games/` e `assets/`
(`textures`, `sounds`, `lang`): `tools/dist/stage.sh` faz a cópia, e essa é a disposição que
`core/paths.h` procura. Um pacote se chama `cubes-and-portals-<versão>-<sistema>-<arquitetura>` e
vai parar em `<build>/dist/`.

### Linux: tarball, AppImage, Flatpak

Os três vêm da mesma compilação portátil:

```sh
cmake -S . -B build-dist -G Ninja -DCNP_PORTABLE=ON
cmake --build build-dist --target cnp_launcher cnp_client cnp_server cnp_modpack
tools/dist/package-linux.sh build-dist        # .tar.gz
tools/dist/package-appimage.sh build-dist     # .AppImage (precisa do appimagetool)
tools/dist/package-flatpak.sh build-dist      # .flatpak (precisa do runtime, abaixo)
```

- **Tarball** (~25 MB): descompacte em qualquer lugar e rode `./cnp_launcher` (ou `./cnp_client`
  só para o jogo). Roda em qualquer glibc a partir da 2.38, ou seja, da máquina em que foi
  compilado em diante: para alcançar distribuições mais antigas, precisa ser compilado numa mais
  antiga. `package-linux.sh` recusa uma compilação sem `CNP_PORTABLE`.
- **AppImage** (~23 MB): um arquivo só. Rodado, é o launcher; `… game`, `… server` e `… modpack`
  são os outros três. Baixe o `appimagetool` de <https://github.com/AppImage/appimagetool/releases>
  (continuous) e coloque-o no `PATH` ou em `$APPIMAGETOOL`.
- **Flatpak** (~17 MB): o tarball dentro de `/app/lib/cubes-and-portals`, sobre o runtime
  `org.freedesktop.Platform//25.08`. É montado só com os comandos do próprio flatpak, sem
  flatpak-builder nem SDK. Para montá-lo você precisa do runtime:
  `flatpak install --user flathub org.freedesktop.Platform//25.08`. Quem recebe o arquivo o instala
  com `flatpak install --user <arquivo>.flatpak`, e o runtime vem do Flathub. A partir daí ele
  inicia pelo menu do desktop ou com `flatpak run io.github.giacomoperin.CubesAndPortals` (aqui
  também `game`, `server` e `modpack` como primeiro argumento). As versões que o launcher baixa
  também rodam lá dentro: o runtime tem o `curl` e o `tar` que ele usa. Mundos e opções ficam em
  `~/.var/app/io.github.giacomoperin.CubesAndPortals/`.

#### O repositório Flatpak (atualizações com `flatpak update`)

Um Flatpak instalado a partir de um arquivo `.flatpak` nunca é atualizado. Por isso a CI também
publica um **repositório** no GitHub Pages (<https://giacomo986.github.io/CubesAndPortals/>):
`package-flatpak.sh` exporta para `build-dist/dist/flatpak-repo/` e, quando tem uma chave
(`FLATPAK_GPG_KEY_ID`, e `FLATPAK_GPG_HOMEDIR` se não for a do próprio gpg), o assina;
`tools/dist/flatpak-site.sh build-dist site` faz o site com ele: `repo/`, o `.flatpakref` para
instalar, o `.flatpakrepo` para adicionar o remote (os dois levam a chave pública,
`tools/dist/flatpak-repo.gpg`) e uma página. O job `pages` de `.github/workflows/release.yml` o
publica a cada tag, ou à mão pela aba Actions.

Ele precisa do segredo `FLATPAK_GPG_KEY` do repositório no GitHub: a chave privada, em formato
armored, uma vez só:

```sh
gh secret set FLATPAK_GPG_KEY < ~/.config/cubes-and-portals-release/flatpak-repo-secret.asc
```

e o Pages ligado com "GitHub Actions" como fonte (Settings, Pages). Sem o segredo a versão sai do
mesmo jeito, sem repositório. Guarde a chave privada: com outra chave, quem já instalou não recebe
mais atualizações até instalar de novo.

### O manifesto da versão

O launcher não roda um pacote baixado se o `release.manifest` da versão não o listar: tamanho e
hash de cada pacote, a versão, os protocolos dela e a API de mods, assinados com a **chave das
versões** (`src/core/release.h`). A metade pública é `kReleaseKeyHex` em `src/core/version.h`; a
privada fica em `~/.config/cubes-and-portals-release/release.sign.key` e no segredo
`RELEASE_SIGN_KEY` do repositório, definido uma vez só:

```sh
gh secret set RELEASE_SIGN_KEY < ~/.config/cubes-and-portals-release/release.sign.key
```

O job `publish` assina os pacotes com ela (`cnp_release sign`), os confere (`cnp_release check`) e
envia o manifesto junto com eles; sem o segredo ele para, porque uma versão sem manifesto é uma
que o launcher recusa. À mão:

```sh
cnp_release sign ~/.config/cubes-and-portals-release/release.sign.key <pasta com os pacotes>
cnp_release check <pasta com os pacotes>
```

Guarde a chave: uma versão assinada com outra é recusada por todo launcher que já está por aí. Um
fork faz a sua própria (`cnp_release key ARQUIVO` imprime a metade pública) e muda
`kReleaseKeyHex`.

### Windows: zip

É compilado a partir do Linux, não no Windows: o [zig](https://ziglang.org/download/) 0.16 no
`PATH` faz o papel de compilador e de MinGW (`tools/dist/zig-windows.cmake`).

```sh
tools/dist/package-windows.sh                 # configura e compila em build-win/, depois o zip
```

O zip (~25 MB) contém `cnp_launcher.exe`, `cnp_client.exe`, `cnp_server.exe`, `cnp_modpack.exe` e
os dados. Descompacte em qualquer lugar e dê um duplo clique no launcher. Precisa do Windows 10 ou
mais novo, porque só usa DLLs do sistema (UCRT, OpenGL32, Winsock). Mundos e opções ficam em
`%APPDATA%\cubes-and-portals`. Para testá-lo no Linux: `wine cnp_client.exe`. No wine dentro do
Xephyr os cliques não chegam ao menu, e um `--quick` ou um `--connect` testa do mesmo jeito.

No Windows ainda faltam algumas coisas (TODO.md, OPEN-40): nenhum dump da pilha nem watchdog, um
cliente que trava fecha o seu servidor sem salvar, e voltar ao menu pede a senha de novo.

### macOS: .app num .dmg

É compilado **num Mac**, porque o cliente precisa do SDK da Apple (Cocoa, OpenGL). Você precisa
das ferramentas de linha de comando do Xcode, de `cmake` e de `ninja` (`brew install cmake ninja`):

```sh
tools/dist/package-macos.sh                   # compila em build-mac/, depois o app e o .dmg
```

Os programas vão para `Cubes and Portals.app/Contents/MacOS`, os dados para `Contents/Resources`.
Precisa do macOS 11 ou mais novo. O pacote tem a arquitetura do Mac que o compila (arm64 no Apple
silicon). A assinatura é ad hoc, não de um desenvolvedor: a primeira execução precisa ser com
clique direito e Abrir, ou `xattr -dr com.apple.quarantine "Cubes and Portals.app"`. **Nunca foi
testado num Mac.**

### Todos juntos: GitHub Actions

`.github/workflows/release.yml` gera todos os pacotes: Linux e Windows no Ubuntu, macOS num runner
Apple silicon. Ele começa sozinho a cada tag `v*` e pode ser rodado à mão pela aba Actions. Numa
tag, confere que a tag é a versão escrita no código, e coloca os pacotes numa GitHub Release (um
pré-lançamento enquanto a versão é uma alfa ou uma beta) cujas notas são a seção dessa versão no
`CHANGELOG.md`. O repositório precisa estar no GitHub para tudo isso funcionar.

Nenhum pacote é assinado: o SmartScreen do Windows pede confirmação uma vez ("Mais informações",
depois "Executar assim mesmo") e o Gatekeeper do macOS também.

## Versões

A versão fica num lugar só, `kEngineVersion` em `src/core/version.h`. Dali a leem os nomes dos
pacotes, o log de entrada do servidor e o canto inferior direito do menu.

Enquanto o jogo está em alfa, a forma dela é `0.MINOR.0-alpha.N`:

- **N** sobe um a cada alfa entregue (`0.4.0-alpha.1`, `0.4.0-alpha.2`, ...);
- **MINOR** sobe quando uma alfa não lê mais os mundos salvos da anterior ou não joga mais com os
  servidores dela, e então N recomeça do 1 (`0.5.0-alpha.1`). Quando isso acontece, fica escrito no
  `CHANGELOG.md`, porque quem joga perde os seus mundos.

A versão serve para quem a lê, ela não decide nada. Se um cliente e um servidor podem jogar juntos,
quem decide é o número do protocolo (`net/protocol.h`) e o hash da tabela de blocos, como explica o
comentário em `version.h`.

Para fazer uma alfa:

1. aumente `kEngineVersion`;
2. adicione a nova entrada no topo do `CHANGELOG.md`, com a data e o que muda para quem joga (em
   inglês, e a mesma em italiano no `CHANGELOG.it.md` e em português no `CHANGELOG.pt.md`);
3. faça o commit, depois `git tag v<versão>` (por exemplo `git tag v0.4.0-alpha.2`);
4. `git push`, depois `git push origin v<versão>` (`--follow-tags` não leva tags simples): a CI
   gera os pacotes e os publica. Sem o GitHub, rode à mão os `package-*.sh` acima e entregue os
   arquivos de `<build>/dist/`. Todo `package-*.sh`, antes de gerar, move os pacotes das outras
   versões para `<build>/dist/vecchie/` (`tools/dist/archive-old.sh`): só o que vai ser entregue
   fica em `dist/`.
