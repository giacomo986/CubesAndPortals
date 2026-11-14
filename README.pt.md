# Cubes and Portals

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

[![Um vídeo do jogo](https://img.youtube.com/vi/WdtsBnpiZpA/hqdefault.jpg)](https://www.youtube.com/watch?v=WdtsBnpiZpA)

**[Um vídeo do jogo](https://www.youtube.com/watch?v=WdtsBnpiZpA)**

**Conteúdo**

- [Estado](#estado)
- [Plataformas](#plataformas)
- [Download](#download)
- [Compilar a partir do código](#compilar-a-partir-do-código)
- [Como contribuir](#como-contribuir)
- [Uso de IA](#uso-de-ia)
- [O jogo](#o-jogo)
  - [A praça e seus mundos](#a-praça-e-seus-mundos)
  - [O sandbox](#o-sandbox)
  - [O motor](#o-motor)
- [Jogar e testar: referência](#jogar-e-testar-referência)
  - [Controles](#controles)
  - [O menu de título](#o-menu-de-título)
  - [Cliente (cnp_client)](#cliente-cnp_client)
  - [Servidor (cnp_server)](#servidor-cnp_server)
  - [Administração do servidor](#administração-do-servidor)
  - [Ferramentas](#ferramentas)
  - [Testes](#testes)
- [Tecnologia](#tecnologia)
- [Documentação](#documentação)
- [Licença](#licença)

Um sandbox de voxels escrito do zero, em que **os portais fazem parte do motor**: um portal mostra
outro mundo, mesmo um que roda em outro servidor, e você atravessa sem tela de carregamento. Da
praça de spawn, os portais levam a mundos que são outros jogos inteiros: uma pista de kart, um
shooter de arena, um RPG de ação isométrico, câmaras de quebra-cabeça com portais, os backrooms,
um percurso de parkour e um assalto cooperativo. Dá para jogar sozinho ou com outras pessoas, e
cada mundo é feito de mods em Luau que qualquer um pode ler e alterar.

Este é um **projeto colaborativo, aberto a contribuições**: código, mundos, mods, pacotes de
texturas, traduções, relatórios de bugs e testes de jogo são todos bem-vindos (veja
[Como contribuir](#como-contribuir)).

A maior parte da documentação detalhada (`docs/`, os guias dos mundos, o changelog) está escrita
em italiano, e há versões em inglês de muitos arquivos; código, comentários, `TODO.md` e mensagens
de commit estão em inglês. Issues e pull requests podem ser em inglês, italiano ou português. O
jogo em si está traduzido para o português (Opções, Idioma: Português).

## Estado

**Alfa jogável.** Há um sandbox (um mundo infinito com biomas, cavernas, vilas com seus
habitantes e sua economia, animais e monstros, circuitos, xadrez e cartas), os sete mundos atrás
dos portais da praça, o laboratório final, mais dois mundos depois dele, e multijogador. Muita
coisa ainda está incompleta ou crua: o que falta ou está quebrado está no [`TODO.md`](TODO.md), e
o que mudou em cada versão está no [`CHANGELOG.md`](CHANGELOG.md).

Enquanto o jogo estiver em alfa, uma versão nova pode não ler os mundos salvos pela anterior, nem
jogar com os servidores dela. O changelog avisa sempre que isso acontece.

## Plataformas

| Sistema | Pacotes | Estado |
|---|---|---|
| **Linux** x86_64 | `.tar.gz`, `.AppImage`, `.flatpak` | A plataforma principal: o jogo é desenvolvido e testado aqui. O tarball precisa do glibc 2.38 ou mais novo (Ubuntu 24.04, Fedora 39 e posteriores) |
| **Windows** 10 ou mais novo, x86_64 | `.zip` | Funciona, mas foi testado principalmente no wine. Lacunas conhecidas: nenhum dump da pilha num crash, e um cliente que trava derruba seu servidor local sem salvar |
| **macOS** 11 ou mais novo (Apple silicon) | `.dmg` | O pacote é gerado, mas **nunca foi testado num Mac** |

O cliente precisa de uma placa de vídeo com **OpenGL 3.3**; no Linux roda no X11, ou no Wayland
pelo XWayland. O servidor (`cnp_server`) não tem janela e roda em qualquer lugar onde o resto
roda. Relatos de máquinas reais com Windows e macOS são especialmente bem-vindos.

## Download

Os pacotes prontos estão na página **[Releases](../../releases)**. Para cada versão (uma tag
`v*`) o GitHub Actions os gera para os três sistemas e os publica como pré-lançamento.

- **Linux**: descompacte o `.tar.gz` em qualquer lugar e rode `./cnp_launcher`; ou torne o
  `.AppImage` executável (`chmod +x`) e rode; ou instale o Flatpak pelo repositório, que o mantém
  atualizado (`flatpak update` traz cada versão nova):

  ```sh
  flatpak install --user https://giacomo986.github.io/CubesAndPortals/cubes-and-portals.flatpakref
  ```

  O arquivo `.flatpak` de cada versão também instala (`flatpak install --user <arquivo>.flatpak`),
  mas um Flatpak instalado de um arquivo nunca é atualizado: instale pelo repositório.
- **Windows**: descompacte o `.zip` e dê um duplo clique em `cnp_launcher.exe`.
- **macOS**: abra o `.dmg` e arraste o app para onde quiser.

Todo pacote abre o **launcher**: Jogar inicia o jogo; Versões mantém várias versões lado a lado
(baixadas da página Releases, cada uma conferida com o manifesto assinado da versão antes de
rodar); Mundos joga um mundo salvo, copiando-o antes quando uma versão mais nova o abre; Mods
mostra, adiciona (solte uma pasta ou um `.cnpmod` na janela), desliga e remove mods, em todo lugar
ou por mundo. Um servidor que quer outra versão manda o jogo de volta para o launcher, que inicia
essa versão. O jogo sozinho é o `cnp_client` (`AppRun game`, `flatpak run … game`).

Os pacotes não são assinados. Na primeira execução, o SmartScreen do Windows pede confirmação
("Mais informações", depois "Executar assim mesmo"); no macOS, clique com o botão direito no app
e escolha Abrir.

**Atualizações.** O jogo procura uma versão nova sozinho: quando o menu de título abre, ele
pergunta ao GitHub qual é a versão mais recente deste repositório (essa requisição é tudo o que
ele envia, nada sobre o jogador). Se houver uma mais nova, aparece um botão "Nova versão …:
baixar" que abre a página Releases; um Flatpak instalado pelo repositório (acima) é atualizado
pelo `flatpak update`. Desligue a verificação em Opções, Atualizações, ou só por uma execução com
`--no-update-check`.

Mundos salvos e opções ficam em `~/.local/share/cubes-and-portals` e
`~/.config/cubes-and-portals` no Linux, em `~/.var/app/io.github.giacomoperin.CubesAndPortals`
com o Flatpak, e em `%APPDATA%\cubes-and-portals` no Windows.

## Compilar a partir do código

Todas as bibliotecas que o jogo usa (raylib, Luau, ENet, zstd, SQLite, Monocypher, flecs, Jolt)
estão em `third_party/`, como submódulos git, e são compiladas junto com o jogo. Você precisa do
**CMake 3.25+** e de um compilador C++20 (**GCC 13** ou Clang). No Linux (Ubuntu/Debian):

```sh
sudo apt install build-essential cmake ninja-build git \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libasound2-dev

git clone --recursive https://github.com/giacomo986/CubesAndPortals.git
cd CubesAndPortals
cmake -S . -B build -G Ninja
cmake --build build -j
./build/src/client/cnp_client
```

Um clone feito sem `--recursive` pode ser completado com
`git submodule update --init --recursive`.

A compilação gera estes programas:

| | |
|---|---|
| `build/src/launcher/cnp_launcher` | o launcher: versões, mundos, mods; o que os pacotes abrem |
| `build/src/client/cnp_client` | o jogo: menus, mundos salvos, multijogador |
| `build/src/server/cnp_server` | um mundo para outros jogadores, sem janela |
| `build/tools/cnp_modpack` | empacota um mod num único arquivo `.cnpmod` |

Uma compilação de desenvolvimento lê `mods/`, `games/` e `assets/` do checkout, então rode-a de
lá. Os testes rodam com `ctest --test-dir build` (13 testes, alguns segundos).

Os pacotes para os jogadores são feitos pelos scripts em `tools/dist/` e vão para
`<pasta de build>/dist/`. O pacote do Windows é compilado **a partir do Linux** com zig; o do
macOS precisa ser gerado **num Mac**. Todos os detalhes (opções do CMake, pacotes, versões) estão
em [`docs/BUILDING.md`](docs/BUILDING.md) (em inglês).

## Como contribuir

O projeto é aberto a todos, e há muitas formas de ajudar:

- **jogar e relatar** bugs, travamentos e qualquer coisa confusa, abrindo uma issue: o que
  aconteceu, o que você esperava, seu sistema e a versão do jogo (no canto inferior direito do
  menu de título), e o log se puder;
- **traduzir**: um idioma novo é um arquivo novo em `assets/lang/` (veja `it.txt` e `pt.txt`);
  revisar o `pt.txt` também ajuda muito;
- **fazer mods, mundos e pacotes de texturas**: veja [`docs/MODDING.md`](docs/MODDING.md),
  [`docs/API.md`](docs/API.md) e [`docs/TEXTURE_PACKS.md`](docs/TEXTURE_PACKS.md); os arquivos
  Blender dos modelos estão em [`assets/blender/`](assets/blender/README.md); um mod que adiciona
  algo ao jogo de fora dele -- uma espécie para o jogador, por exemplo -- pode ser testado primeiro
  como add-on, do jeito que [`contrib/`](contrib/README.md) guarda o homem-lagarto;
- **trabalhar no motor**: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) explica como o código
  está organizado e onde entra um recurso novo; [`TODO.md`](TODO.md) lista o que falta.

Como propor uma mudança:

1. para uma mudança grande (um mundo novo, uma mudança no protocolo de rede ou no formato de
   salvamento), abra uma issue antes e converse sobre ela;
2. um pull request faz **uma coisa só**, e a descrição diz o que muda, por quê e como você
   testou (no jogo e/ou com `ctest`);
3. o código compila sem avisos novos e os testes passam;
4. código novo se parece com o código em volta (nomes, comentários, estilo), e a documentação da
   parte que você mexeu é atualizada junto;
5. texturas, modelos e sons são seus ou têm licença compatível, indicada num `CREDITS.md` na pasta
   do mod (como [`games/track/CREDITS.md`](games/track/CREDITS.md)). Nada de nomes, marcas ou
   material de jogos comerciais.

**Toda contribuição é aceita, desde que não seja maliciosa nem bagunçada.**

- *Maliciosa*: malware, backdoors, coleta de dados dos jogadores, código que esconde o que faz,
  material copiado sem direito, conteúdo ofensivo. Um pull request assim é fechado, e o autor não
  abre outros.
- *Bagunçada*: pull requests enormes que misturam coisas sem relação, reformatação em massa,
  código morto, mudanças sem descrição, build quebrado, código gerado que ninguém leu. Um pull
  request assim não é rejeitado por princípio: vão pedir que você o divida ou arrume.

Ao contribuir você concorda que sua contribuição é distribuída sob a licença do projeto (MIT).

## Uso de IA

Este projeto é desenvolvido com **uso intenso de IA**, e diz isso abertamente:

- **Código, documentação e mensagens de commit** são em grande parte escritos pelo Claude
  (Anthropic), pelo Claude Code, sob a direção do mantenedor do projeto, que decide o que
  construir, joga o jogo e aprova cada mudança. Commits escritos assim dizem isso com uma linha
  `Co-Authored-By: Claude`.
- **Modelos 3D, texturas e quase todos os sons** são gerados por scripts Python (e Blender) em
  `tools/`, também escritos com IA: nenhum asset é tirado de outros jogos.
- **As ilustrações do baralho** e alguns sons são gerados com o ElevenLabs.

Contribuições feitas com ajuda de IA são bem-vindas nas mesmas condições de qualquer outra: quem
abre o pull request responde por ele, leu, entendeu e testou, e diz isso na descrição. Uma saída
de modelo colada sem conferir conta como bagunçada.

## O jogo

### A praça e seus mundos

Você começa numa praça. Em volta dela, sete portais, cada um dentro de uma construção que lembra o
mundo atrás dele (`mods/gates`), levam a sete mundos, todos salas do mesmo servidor
(`tools/track.sh`, ou qualquer mundo iniciado pelo menu de título):

- **A pista** (`games/track`): karts de corpo rígido (Jolt) com suspensão e nitro, pilotados pelo
  servidor a partir das teclas do jogador. Cinco pistas cronometradas com placares, uma pelo
  Inferno de verdade e uma com cinco loops. Peças de pista como as dos jogos de corrida
  acrobática (loops baixos, altos e largos, adaptadores de largo para estreito), e um editor para
  construir pistas novas: ele oferece as peças que encaixam na ponta aberta, desfaz e fecha o
  circuito sozinho. Caixas de item na estrada (turbo, bananas, cascos verdes e vermelhos,
  estrela, raio, tinta), derrapagem com marcas de pneu, chamas dos escapamentos e uma garagem
  (`/garage`) para escolher as cores e 14 peças do seu kart.
- **O Inferno** (`games/inferno`): cavernas de escória, lava correndo, fortalezas, brutamontes,
  espectros e brasas. Dá para chegar lá também por um portal de obsidiana que você mesmo constrói.
- **A base** (`games/yafps`): um shooter de arena moderno com cinco armas e ondas de demônios.
- **As terras devastadas** (`games/arpg`): um RPG de ação isométrico de apontar e clicar. Uma
  cidade com mercadores e um baú, três zonas de áreas e corredores, pontos de teleporte e um
  portal da cidade, três classes com quatro habilidades cada, níveis, e itens normais, mágicos,
  raros, únicos e de conjunto que aparecem no personagem, com suas animações.
- **As câmaras de testes** (`games/chambers`): seis câmaras de quebra-cabeça com portais, uma arma
  de portais que cresce com você, cubos, botões e plataformas de lançamento.
- **Os backrooms** (`games/backrooms`): três níveis infinitos com escadas, poços, piscinas,
  escorregadores e duas criaturas; caixas raras guardam o caminho de volta. Guia em
  [`games/backrooms/README.md`](games/backrooms/README.md).
- **O percurso de parkour** (`games/parkour`): corrida na parede, quiques, deslizes, rolamentos e
  uma arma de corda. Guia em [`games/parkour/README.md`](games/parkour/README.md).

Mais dois jogos ficam nas salas `platformer` e `shmup`, com portões na praça: um jogo de
plataforma visto de lado (`games/platformer`) e um shooter de fliperama visto de cima com um caça
(`games/shmup`).

**Uma casa maior por dentro** (`mods/pocket`). Uma porta de bolso (uma porta e um rubi num anel de
molduras de portal) colocada numa parede vira uma passagem para um salão só seu, de quinze por
quinze e seis de altura: construa uma cabana de três por três em volta e ela fica maior por dentro
que por fora. Os salões ficam numa sala própria, `pockets`, com todos os mods do sandbox, os seus
também; quebre a moldura e a porta se dobra, para abrir no mesmo salão onde quer que você a
coloque de novo. O prefeito de uma vila vende uma cabana de bolso pronta; quatro portas de bolso
podem estar em uso ao mesmo tempo.

**Uma sala só sua** (`mods/home`). Construa uma moldura de portal em pé em volta de um buraco de
um de largura e dois de altura, clique direito nela e escolha "minha sala": a moldura leva a um
mundo inteiro só seu, criado na primeira vez que você chega perto e guardado num arquivo próprio
ao lado do do sandbox. Ele dorme um minuto depois que você sai e acorda quando você volta à
moldura. Diga "minha sala" de outra moldura e a entrada muda para lá; a porta lá dentro fica perto
do spawn e leva de volta para fora.

**As sete chaves** (`mods/keys`, o final do jogo). O engenheiro que construiu os portais trancou o
Hóspede no laboratório e dividiu a fechadura em sete chaves, uma em cada mundo, atrás de uma
prova. O oitavo portal da praça, uma moldura de aço escuro com sete pedestais, leva ao
**laboratório** (`games/lab`) e recusa quem não segurou as sete chaves. Cada mundo é deixado por
uma saída no fim da sua prova, e cada chave espera num pedestal dourado ao lado dela. No
laboratório, o Hóspede é derrotado com um pouco de cada mundo: arma de portais, fuzil, uma escalada
de parkour, válvulas sob a lava, uma lanterna no escuro, gelo e, por fim, uma fuga de kart por um
túnel e os créditos. Depois disso o sandbox fica livre, e mais dois mundos se abrem:

- **O front** (`games/front`): um campo de batalha no estilo dos grandes shooters de equipe, duas
  bases e três pontos para tomar, vermelho contra azul, com bots e veículos; cada base tem uma pista
  de onde decolam seus aviões. A galeria onde você escolhe um lado tem o campo inteiro em miniatura
  sob o piso de vidro, e o lança-foguetes trava nos veículos inimigos pela mira de vidro.
- **O assalto** (`games/assault`): o assalto cooperativo final à dimensão de Basoalth: quatro
  fortalezas, uma loja, aliados e construtores, um chefe que cresce até virar um gigante e uma fuga
  de jipe por um túnel de lava.

`ALL_OPEN = true` em `mods/keys/init.luau` libera todos os mundos para todos, para testar sem
vencer nada: as saídas, os arcos, o portão do laboratório, o front e o assalto. `false` é o jogo
como ele é distribuído.

Na praça, perto de 31 20 43, uma portinha numa cerca viva e uma vitrine no chão (`mods/garden`):
pelo vidro um jardim cercado a leste da praça é visto de cima em miniatura; pela porta você sai
nele com um dezesseis avos do seu tamanho, os blocos de grama grandes como salas e a cerca viva um
penhasco. De volta pela mesma aberturinha, na moldura de madeira entre dois cogumelos, você sai da
porta do tamanho de antes. Pequeno, você anda tão rápido quanto seu tamanho permite: o jardim leva
mais ou menos um minuto para atravessar. Formigas vão em fila entre o formigueiro e as flores,
joaninhas passeiam, borboletas pousam nas flores: no tamanho real delas, uma formiga é um cachorro
para você ali. Uma criatura que passa pela portinha muda de tamanho como você (um dezesseis avos
entrando, até quatro vezes maior saindo). Uma sela numa joaninha a doma e ela te carrega como um
pônei: montada para fora pela portinha ela sai tão grande quanto você, com mais de um metro, e
espera, selada, onde você desce (Ctrl, ou clique direito no chão; clique direito nela para montar
de novo).

### Dificuldade, regras e objetivos

"Monstros" na página de mundo novo, ou na página Editar de um mundo salvo, escolhe o quanto o
sandbox é difícil: nenhum (pacífico), poucos e fracos (fácil), como sempre, muitos e fortes
(difícil); `/difficulty` durante o jogo (`cnp_server --difficulty`). Pacífico não deixa sair
nenhuma criatura que luta e manda embora as que estão por perto, e o que só os monstros deixam
continua sendo encontrado: o gerador calmo de uma masmorra deixa ossos, carne podre, pólvora e
flechas de vez em quando, o altar das profundezas dá o coração e os fragmentos sem luta, as vilas
não têm cercos. Fácil corta pela metade o golpe de uma criatura, quantas saem e a fome; difícil
aumenta as três pela metade. As regras (`/rule`): manter a bolsa ao morrer num mundo de
sobrevivência, o sol se movendo, o tempo mudando, a fome. O botão **Mundo** do menu de pausa
mostra tudo isso, com o tempo, a hora e a estação, como botões para um admin, e leva aos
**objetivos**: 21 coisas para fazer, da primeira madeira até a chave de cada mundo, anunciadas
quando alcançadas (`/page goals`).

### Sobrevivência: os mundos espalhados pela terra

"Modo: Sobrevivência" na página de mundo novo do menu de título (`cnp_server --survival` na sala
do sandbox, `tools/track.sh --survival`) faz o mesmo jogo sem praça:

- **Sem praça, um spawn aleatório.** O terreno é o ruído em todo lugar; um jogador começa numa
  terra seca e plana que a semente escolhe até 2500 do centro, sob céu aberto se houver algum a
  menos de 64. Sem catálogo (C), nem para o dono (`/perm creative owner` devolve), e uma morte
  deixa a bolsa num túmulo onde aconteceu, desenhado no mapa (`mods/grave`).
- **Portais espalhados pela terra** (`mods/gates`): cinco células de 256 em seis têm um, um portão
  no visual do seu mundo com um arco ao lado para a volta e uma fileira de dez lâmpadas em cima --
  tantas acesas quanto a dificuldade, de 1 a 10, que sobe um passo a cada 300 blocos do spawn. Um
  de cada mundo fica a menos de 950 do spawn, fácil; o laboratório fica de 1500 a 2200, trancado
  pelas sete chaves como na praça; o front e o assalto, além de 1500, abrem depois do final.
- **Um nível próprio atrás de cada portal** (`mods/keys` dá a cada local um espaço do seu mundo):
  o parkour monta um percurso com suas estações e peças novas (saltos, degraus, pilares,
  trampolins, lajes que cedem), 3 + d delas, com vãos do tamanho do pulo; os backrooms uma sala de
  saída a 48 + 24 d, no escuro do Nível 1 a partir de d 4, com as luzes se apagando de vez em
  quando a partir de d 5; a pista um circuito fechado próprio (chicanes, uma colina, uma rampa
  conforme d) com três voltas a bater; o arpg terras com um caminho de 3 + d áreas até um covil e
  seu senhor; as câmaras de testes uma cópia da sua fileira, um trecho de 2 + d / 2 câmaras mais
  adiante quanto mais difícil; a arena, o coração do Inferno, o front e o assalto ajustam ondas
  (com uma surpresa cada a partir de d 4), guardiões, vida, bots e guarnições conforme d. A saída
  de cada nível fica fechada, perto de onde ele começa e escondida atrás de uma parede própria sob
  uma lâmpada apagada; concluído, a parede cai, a lâmpada acende, e uma passagem ou o próprio
  circuito traz o jogador de volta a ela. Dois locais do mesmo mundo podem ser jogados ao mesmo
  tempo.
- **Recompensas e chaves.** De volta ao portal, um baú guarda o que aquele mundo dá (rubis e
  trilhos da pista, ouro e obsidiana do Inferno, flechas e armadura da arena, ...), 40% a mais a
  cada passo de d, um quarto disso por um nível já feito antes, de vez em quando algo raro. A chave
  de um mundo só vem de um nível de d 4 ou mais.
- **Direções** (`mods/villagers`): um aldeão de vez em quando fala do portal mais próximo, de forma
  vaga (como ele é, para que lado, perto ou longe, como voltaram os que foram); uma caravana, de um
  perto da vila de onde ela vem; o prefeito, por 2 + d rubis, com exatidão, com o caminho no mapa.

### O sandbox

**A terra.** Um mundo infinito: planícies, desertos, tundra e montanhas de algumas centenas de
blocos de largura, oceanos e mares rasos com praias, rios que correm para o mar, lagos de
montanha, desfiladeiros com um riacho no fundo, cavernas que se ligam, minérios em veios dentro
da pedra, e um ciclo de dia e noite com nuvens por cima das quais dá para voar. No subsolo há
minas abandonadas e criptas com baús, e uma escada de volta para cima.

**Criaturas que escalam** (`mods/mobs`). Zumbis sobem uma parede atrás de você quando você está em
cima dela, e um tipo mais rápido, o corredor, dispara, escala e pula em você a sete blocos de
distância.

**Vilas** (`mods/villages`, `mods/villagers`). Um mundo novo tem cidades (uma por célula de 2048
onde o terreno permite, com até 500 de largura, prédios perto do centro) ligadas por rodovias
pavimentadas que atravessam montanhas e fazem pontes sobre a água, cidadezinhas densas em volta
das cidades e vilas espalhadas pelo interior; `/town city` leva você à mais próxima. Vilas e
cidadezinhas com praça, ruas, casas, campos, uma forja e lampiões, ligadas numa rede de caminhos
sinuosos -- cascalho, arenito, pedregulho ou terra batida conforme o terreno, com placas, marcos,
lanternas e pontes. São habitadas por três espécies: agricultores humanos, pescadores sapos e
coletores toupeiras, cada um com casa, trabalho, refeições e cama. Toda vila tem:

**Museu, álbum e jornal** (`mods/villagers`, `mods/album`). Toda vila constrói um museu: o curador
expõe o primeiro de cada peixe, minério, gema e achado que um jogador traz e paga por ele em
rubis. Uma câmera tira fotos de verdade -- segure o botão direito para olhar pelo visor, clique
esquerdo para disparar -- instantâneas que todo mundo vê como foram tiradas, guardadas num álbum
(`/album`) e penduradas em molduras; uma foto de um pico, das Cavidades, de uma relíquia ou das
nuvens vale algo para um museu. E o quadro de uma vila guarda o jornal dela: quem resistiu a um
cerco, trouxe uma caravana, deu algo ao museu ou foi pego roubando.

- uma guarda (guardas, arqueiros e um inspetor que revista bolsas atrás de mercadoria roubada: o
  roubo fica marcado, e um ladrão é perdoado uma vez, depois caçado);
- um prefeito que vende terrenos e casas, e um construtor que as constrói bloco a bloco com os
  materiais trazidos pelos lenhadores, mineiros e escavadores da vila;
- um mercador e trabalhadores que negociam em rubis, com preços definidos pela oferta, e um tesouro;
- uma mina, um artesão que refaz ferramentas e casas para cada função.

**Territórios** (`mods/territory`): um marco funda um território que cresce enquanto os donos
ficam nele. Lá dentro, os donos são mais rápidos e resistentes, os estranhos mais fracos; o que os
estranhos pegam ali fica marcado como roubado.

**Jogos da praça** (`mods/chess`, `mods/arena`): a mesa de xadrez também joga damas italianas e
trilha; um gongo começa uma rodada PvP de três minutos no ringue em volta dele, com pontuação e
classificação -- ou um esconde-esconde de objetos, em que os escondidos viram blocos do mundo e os
caçadores têm três minutos para achá-los.

**Decoração** (`mods/building`): vitrais e vidraças em dezesseis cores, muros de pedra e tijolo,
azulejos esmaltados e quadriculados, cadeiras e mesas de todas as madeiras, tapetes, lanternas.

**Ferrovias** (`mods/building`, `mods/carts`): trilhos, curvas, rampas, trilhos energizados e
desvios como modelos de verdade; um carrinho de mina em que você sobe e anda sentado.

**Plantas** (`mods/blueprints`): copie uma construção entre dois cantos (até 32x24x32) numa
planta, carregue-a, demarque-a como um fantasma transparente e construa à mão -- ou peça ao
construtor da vila ou a um robô. Um robô ergue uma torre de andaimes para o que a mão dele não
alcança e desmonta depois; andaimes (seis com três tábuas e quatro gravetos) são escalados como uma
escada, por qualquer um.

**Zonas** (`mods/zones`): um pedaço de nível salvo fora do mundo, para montar níveis. Construa uma
sala, coloque blocos de **passagem** nos lados onde outro pedaço pode se juntar e blocos de
**marcador** onde um mundo coloca suas coisas (início, saída, criatura, saque, chave, gatilho), e
`/zone save MUNDO NOME X1 Y1 Z1 X2 Y2 Z2 [start|room|end]` salva a caixa, até 64x48x64, com a
rotação de cada bloco. Um nível é feito de zonas ligadas passagem a passagem, giradas e espelhadas
como precisarem, de um início a um fim, igual para a mesma semente; uma passagem que não liga a
nada é fechada. Uma zona é de quem a fez, e só entra nos níveis pedidos em nome dele, até um admin
aprová-la (`/zone approve`); `/zone try MUNDO` monta um nível ao lado de um admin. Para quem faz
mods: `docs/MODDING.md`, "Levels out of saved pieces".

**Tarefas**: cada vila pendura até três num quadro de avisos perto do poço -- trazer o que falta,
conseguir um material, fabricar ferramentas, caçar à noite, levar uma encomenda a outra vila,
explorar um lugar distante -- aceitas e pagas (pelo tesouro) com o prefeito e o mercador; depois de
cinco, o mercador dá desconto.

**Eventos da vila**, anunciados no sino da prefeitura e no quadro: caravanas de um mercador e uma
mula levando mercadorias de verdade a outra vila (compre delas na estrada, ou escolte-as por
pagamento), cercos noturnos raros avisados ao entardecer (três ondas contra a guarda: nenhum aldeão
perdido, rubis para os defensores), caravanas de vilas que você ainda não viu (o mercador mostra o
caminho, no seu mapa também), e a festa da colheita no último dia do verão -- barracas e lanternas
na praça, comida pela metade do preço, música, fogos à noite (`mods/fireworks`).

**Criação de animais** (`mods/animals`): tinja uma ovelha para ter lã colorida, ordenhe uma vaca
com um balde (queijo, bolo), mantenha um estábulo com um cocho de trigo ou sementes que segura os
animais e os faz procriar, e mande um robô cuidar deles.

**Pesca**: o que belisca depende da água -- percas e carpas nos lagos, trutas nos rios, bacalhau e
enguias no mar, atum no oceano, um peixinho dourado raro em qualquer lugar -- e os pescadores sapos
compram.

**Alquimia** (`mods/alchemy`). Um alambique destila um frasco de água e um ingrediente numa poção:
visão noturna, rapidez, pele de pedra, cura, pressa, respiração na água, vigor -- três minutos
cada, mantidos se você sair ou o servidor reiniciar. Debaixo d'água o ar dura quinze segundos
(`mods/breath`).

**Abelhas** (`mods/bees`). Ninhos selvagens entre as flores dos campos, e colmeias de tábuas e
favos: enchem de mel mais rápido quanto mais flores houver em volta, dormem durante o inverno e
ajudam as plantações em volta a crescer. Um frasco pega uma garrafa de mel (comida), a mão nua dois
favos (uma poção, uma colmeia); uma tocha embaixo delas, ou um fumigador, evita que as abelhas
piquem. De dia as abelhas voam em volta. Abelhas criadas: uma rede pega a rainha de um ninho
selvagem, e um apiário a recebe, quadros para encher mais rápido, um agasalho de lã para o inverno
e um infusor que faz mel especial com o que está em volta (cogumelos luminosos, lava, ametista);
uma prensa o transforma em lâmpadas de mel, cera de brasa para a fornalha, e ametista com um
diamante de vez em quando.

**Cavidades** (`mods/deep`). Sob a rocha-mãe de algumas regiões há uma terra de cavernas selada,
alcançada por um sumidouro com degraus na lateral: fungos e musgo luminosos, cristais, lagos de
lava e um altar cujo guardião desperta quando você chega perto. Espreitadores pálidos rondam as
cavernas.

**Relíquias** (`mods/relics`). Templos raros no deserto, naufrágios e ruínas submersas, cada um com
um pequeno quebra-cabeça na frente do baú: glifos para ajustar como a parede mostra, os passos de
um diário de bordo para contar e cavar, fragmentos para recolocar nos pedestais.

**Mapa e bússola** (`mods/maps`). Uma bússola na mão aponta para casa, sua cama ou o spawn; um mapa
na mão mostra a terra em volta por onde você passou, guardado pelo servidor entre as sessões.

**Pedras de caminho** (`mods/travel`). Uma pedra de caminho fica perto do poço de cada vila, e dá
para fazer uma para um território: toque numa para memorizá-la, depois pague um rubi a cada 200
blocos para abrir um portal de qualquer pedra para uma que você lembra, para qualquer um
atravessar, por cinco minutos. Um mundo comporta quantos portais forem construídos; só os perto de
jogadores ficam ativos ao mesmo tempo.

**Carrinhos** (`mods/carts`). Cinco lingotes de ferro fazem um carrinho: clique direito num
trilho para sentar e andar, pule para sair. Os trilhos vão nos dois sentidos, fazem curvas e sobem
rampas; um trilho energizado empurra o carrinho a 11 m/s quando o cinábrio o alimenta e é uma
estação quando não; um desvio energizado vira o carrinho para a direita.

**Barcos** (`mods/boats`). Cinco tábuas fazem um barco: clique direito na água para sentar, reme
ao dobro da velocidade de nado e vire com A e D (ele empurra as criaturas e quica em outros
barcos), clique direito numa margem (ou encalhe) para descer com ele de volta na bolsa.

**Voar** (`mods/glider`, `mods/balloon`). Uma asa-delta de lã e gravetos, na mão, se abre quando
você pula de uma altura e leva você para onde olha -- olhe para baixo para mergulhar, para cima para
frear, agache para fechá-la e cair. Um balão de ar quente sobe de um terreno aberto: pule para
subir, agache para descer, as teclas e o vento o levam devagar, por cima das nuvens; pousado,
clique direito para guardá-lo. O piloto da asa-delta fica deitado na barra; o do balão fica em pé
no cesto e puxa a válvula do queimador ao subir, a chama rugindo para dentro do envelope.

**Depois do final** (`mods/rewards`). Quem derrotou l'Ospite encontra as ferramentas dos mundos no
sandbox, numa versão menor: a arma de corda (trinta ganchos por dia), uma arma de portais (doze
tiros por dia, em qualquer parede) e o kart, para dirigir em qualquer lugar (F para sair).
`/rewards` as dá de novo.

**Tempo** (`mods/weather`). Céu limpo, depois chuva ou tempestade por alguns minutos: o céu e as
nuvens ficam cinza, a chuva cai em volta de você e para nos telhados, os campos sob o céu aberto são
regados. Na tundra e nos picos neva e a neve se acumula; no deserto não cai nada. Uma tempestade
solta raios perto dos jogadores, com trovão. Estações de cinco dias cada: as folhas ficam laranja
no outono e opacas no inverno (o abeto continua verde), o verão tem mais tempestades, a primavera
mais chuva, os campos desaceleram no outono e no inverno só crescem sob lâmpadas, e no inverno neva
em todo lugar menos no deserto. Os comerciantes cobram o dobro pela comida no inverno e pela
madeira no outono.

**O céu** (`mods/sky`). Uma noite limpa em cinco traz uma chuva de meteoros, e de vez em quando um
meteorito que cai perto de você, cava uma cratera e deixa uma pedra com um diamante e ferro bruto
dentro. No inverno a aurora fica sobre o norte, vista da tundra e dos picos. Um dia em doze um
eclipse escurece a terra por três minutos, e as criaturas da noite saem. `/sky` provoca um.

**Criaturas.** Zumbis e esqueletos arqueiros saem no escuro e queimam no sol; porcos, vacas,
ovelhas e galinhas pastam, procriam e crescem; um cachorro e um gato podem ser domados e seguem
você (`mods/mobs`, `mods/animals`, `mods/pets`).

**Sobrevivência e fabricação.** Bolsa e hotbar, fabricação, baús (dois lado a lado fazem um
duplo), uma fornalha com fileiras de entrada, combustível e saída, ferramentas em cinco materiais
que se desgastam, armadura em quatro, fome, experiência e uma mesa de encantamento, camas que
pulam a noite, agricultura (enxada, sementes, trigo, pão, melancias e abóboras, água e luz),
pesca, cana-de-açúcar, papel e livros, portas, escadas e alçapões. A mão esquerda segura um segundo
item (H troca as mãos), e uma tocha vai no chão, na parede ou no teto; uma tocha (ou qualquer bloco
que faz luz) em qualquer uma das mãos ilumina em volta de quem a segura, para todos verem.

**Máquinas.** Circuitos de cinábrio (`mods/cinnabar`): pó, alavancas, botões, tochas,
repetidores, comparadores, lâmpadas, pistões, observadores e funis, então relógios e lógica
funcionam. Esteiras (`mods/conveyor`) que se juntam, viram e sobem, com separadores e extratores.
Robôs ajudantes (`mods/helpers`) que você programa com até seis ordens: cortar árvores, plantar,
cavar, minerar, organizar baús, cozinhar, abrir pedreiras, iluminar a terra, seguir você.

**Circuitos** (`mods/circuits`). Fio de liga que leva energia sem enfraquecer, fio isolado em
dezesseis cores que só se liga à própria cor, cabos que levam dezesseis sinais ao mesmo tempo,
elevadores, dezesseis portas lógicas (AND, OR, NOT, XOR e companhia, travas, pulso, temporizador,
contador, aleatório, sensor de luz) e lâmpadas que acendem na sua cor. Funcionam com o cinábrio; o
pó entra num fio por um repetidor ou uma porta buffer.

**Câmeras** (`mods/cameras`). Uma câmera -- no chão, numa parede ou no teto, onde for colocada -- e
uma tela: clique direito na câmera, depois na tela, e a tela mostra ao vivo o que a câmera vê -- a
base, a corrida, uma vila. Clique direito de novo na tela para assumir os controles da câmera: a
visão vai para a tela e W A S D giram a câmera. Uma câmera que vê alguém energiza o cinábrio ao
lado: uma porta que abre, uma lâmpada, um alarme.

**Passaporte** (`mods/passport`). Cada servidor amigo de onde um portal traz você carimba seu
passaporte (`/passport`). Uma caixa de correio envia uma encomenda -- o que está na sua mão -- ou uma
carta para `nome@servidor` num servidor amigo, entregue na próxima vez que a pessoa entrar.

**Jogos na praça.** Xadrez (`mods/chess`) com todas as regras clássicas, um relógio e nove
variantes (960, rei da colina, três xeques, corrida de reis, perde-ganha, atômico, crazyhouse,
horda). Cartas (`mods/cards`) com o baralho napolitano de quarenta cartas: briscola, cinquiglio,
scopa e tresette, de dois a cinco jogadores, com a mesa jogando nos lugares vazios.

**Personagens e equipamento.** Escolha robô, humano, homem-toupeira ou homem-sapo (ou um de um
add-on, como o homem-lagarto em `contrib/lizard`), um estilo, três cores e dois acessórios, todos
vistos por todo mundo. Nas costas: um jetpack para voar, uma mochila de couro com 27 espaços
próprios, ou uma mochila de metal com dois braços articulados que seguram uma ferramenta cada. O
catálogo criativo (C) tem uma caixa de busca. `/creative on` é o modo criativo: voar sem o jetpack,
sem dano, escavação rápida (livre num mundo só seu, admin num servidor). `/time` define a hora,
`/kill` leva você de volta à cama ou ao spawn quando você está preso. Num mundo com uma prova,
`/giveup` desiste dela: você morre e volta no sandbox, fora do portão do mundo. O que um pedestal ou
um altar segura (uma chave, a arma de portais, um fragmento) fica flutuando em cima dele, girando.

### O motor

- **Voxels.** Chunks de 32³, greedy meshing em threads, vértices de 12 bytes, luz por flood fill do
  sol e dos blocos, oclusão de ambiente, frustum e occlusion culling, um profiler na tela (F6).
- **Luz.** Escuridão de verdade: uma caverna sem tochas é preta, e a noite quase. Sol e lua
  projetam sombras (as folhas também); as tochas iluminam as faces viradas para elas, e uma luz
  carregada na mão se espalha como a de um bloco colocado, contornando cantos e parando nas paredes.
  A luz antiga continua lá: Opções, Luz: clássica. As lâmpadas iluminam na sua própria cor: um bloco
  dá luz da cor com que foi registrado, mais clara, e duas luzes se encontram numa mistura.
- **Portais.** Uma imagem contínua da vista real, recursão até três níveis, todas as 24
  orientações (chão, parede, teto, girados), corpos cortados ao atravessar, e o horizonte nivelado
  depois de uma travessia inclinada. Você mantém a velocidade ao passar por um portal, girada junto
  com ele. Os portais são **construídos no jogo** (`mods/portal`): uma moldura em volta de um
  buraco se abre quando o último bloco entra, até 16 por mundo. Uma abertura sem a outra ponta é
  fechada por uma membrana animada. No menu da moldura (clique direito) quem a construiu, ou um
  admin, escolhe aonde ela leva, a cor, se escala e se é uma janela: ela mostra a outra ponta e não
  deixa nada passar.
- **Mundos e servidores.** Dois mundos num processo, portais para **outro servidor** com tickets
  assinados, e vários mundos (salas) num servidor e numa porta. Um admin liga uma moldura
  construída numa sala a uma moldura em outra pelo menu; esses portais ocupam uma das bocas de uma
  sala só enquanto há um jogador perto, então um mundo comporta mais deles do que tem bocas. O
  mundo não tem borda: as colunas são geradas ou carregadas quando os jogadores chegam perto, e
  descartadas quando não há ninguém.
- **Rede.** Um servidor sem janela a 20 ticks por segundo, ENet + zstd, criptografia X25519 +
  XChaCha20-Poly1305, identidade do jogador por chave, mídia endereçada por conteúdo com um cache
  em disco compartilhado entre mundos. O cliente **adota** a tabela de blocos do servidor; o
  servidor valida o movimento e lança os raios de cada tiro.
- **Modding.** Luau, uma VM por mod com cota de memória, orçamento de interrupção e permissões.
  Recarga a quente (F5), um barramento de eventos tipado, um ECS programável, ABM/LBM, etapas de
  geração do mapa, blocos como caixas ou modelos, movimento por mundo e por corpo, pacotes
  `.cnpmod` com versões e dependências, teclas próprias dos mods (`core.register_key`). Um mundo
  pode enviar **código Luau para o cliente**, numa sandbox sem arquivos nem rede, e o jogador pode
  recusar. Guia em [`docs/MODDING.md`](docs/MODDING.md).
- **Salvamento.** SQLite: chunks, baús, bolsas, posições dos jogadores e o armazenamento dos mods.
  Só o que mudou é gravado, numa transação, e o servidor salva quando é parado com Ctrl+C. Os
  blocos são guardados pelo nome, então adicionar um mod nunca obriga a criar um mundo novo.

O plano completo e suas decisões estão em [`docs/DESIGN.md`](docs/DESIGN.md).

## Jogar e testar: referência

Depois de compilar ([acima](#compilar-a-partir-do-código)), a partir do checkout:

```sh
ctest --test-dir build                       # 13 testes, ~3 s; -LE gpu pula o que abre uma janela

./build/src/client/cnp_client                # o menu de título
./build/src/client/cnp_client --quick        # início rápido: um mundo local de teste, nada é salvo

./build/src/server/cnp_server --listen 25301 --ticks 0 --world 128 --world-file world.db
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name jogador

# O shooter de arena: um sandbox e a arena num servidor
tools/fps.sh --fresh

# A arena sozinha: o cliente não tem mods, o servidor os envia
./build/src/server/cnp_server --listen 25302 --mods games/yafps/mods --textures games/yafps/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25302 --name jogador

# O RPG de ação: um sandbox e o mundo arpg num servidor, atrás de um arco de pedra
tools/arpg.sh --fresh

# A pista cronometrada: com --portals você nasce na estrada, na frente da largada
./build/src/server/cnp_server --listen 25303 --portals --mods games/track/mods --textures games/track/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25303 --name jogador

# Um sandbox com dois portais, para a arena e para a pista, em três servidores.
# Cada um leva de volta à abertura por onde se entrou (/0, /1). Sem --ticks 0 um servidor para depois de 50 s
./build/src/server/cnp_server --listen 25301 --ticks 0 --portals --portal-to 127.0.0.1:25302/0 --portal-to 127.0.0.1:25303/0
./build/src/server/cnp_server --listen 25302 --ticks 0 --portals --portal-to 127.0.0.1:25301/0 --mods games/yafps/mods --textures games/yafps/textures
./build/src/server/cnp_server --listen 25303 --ticks 0 --portals --portal-to 127.0.0.1:25301/1 --mods games/track/mods --textures games/track/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name jogador

# O mesmo num servidor só, como salas: sandbox, pista e inferno (tools/track.sh faz isso, com todos os mundos)
./build/src/server/cnp_server --listen 25301 --ticks 0 --portals \
    --room sandbox --portal-to @pista/0 --portal-to @inferno/0 \
    --room pista --mods games/track/mods --textures games/track/textures --portal-to @sandbox/0 --portal-to @inferno/2 --portal-to @inferno/1 \
    --room inferno --mods games/inferno/mods --textures games/inferno/textures --fallback-textures assets/textures --portal-to @sandbox/1 --portal-to @pista/2 --portal-to @pista/1
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name jogador   # --room inferno para ir direto para lá
```

### Controles

| | |
|---|---|
| WASD, Espaço, F | mover, pular, voar (com um jetpack) |
| Clique esquerdo / direito | quebrar / colocar (com a mão vazia, usar o bloco) |
| Roda ou 1-9, E, C, Q, H | espaço da hotbar, bolsa, catálogo criativo, largar, trocar de mão (a mão esquerda segura o que a direita não segura) |
| T, / | chat, chat com um comando (`/help`, `/list`) |
| V, F11, ESC, F1 | terceira pessoa, tela cheia, menu de pausa, esconder mãos e hotbar |
| F3, F4, F8 | informações, limites dos territórios, células onde monstros podem nascer |
| F5, F6, F7, F2 | recarregar mods, profiler, próximo pacote de texturas, occlusion culling desligado |

Toda tecla da tabela, menos o ESC, pode ser trocada em **Opções > Controles**: clique numa tecla e
aperte a nova (ESC deixa como estava, Backspace deixa a ação sem tecla). O botão do meio e os
laterais do mouse também servem. As teclas próprias dos mods (`core.register_key`) também aparecem
ali. Uma tocha vai no chão, numa parede ou no teto, e uma tocha (ou qualquer bloco que faz luz)
segurada em qualquer uma das mãos ilumina em volta do jogador, e os outros jogadores veem.

As opções ficam salvas entre uma partida e outra. `CNP_HITCH_MS=30 cnp_client ...` imprime cada
frame mais longo que 30 ms com as seções do profiler.

### O menu de título

**Jogar** inicia um mundo só seu num servidor local, com a chave em
`~/.local/share/cubes-and-portals/player.key`; a lista mostra os mundos salvos, o botão verde em
cima cria um novo (nome e semente), e Editar renomeia ou apaga um. Um mundo feito por uma versão
anterior tem uma moldura âmbar e essa versão ao lado do nome (o arquivo `version` dele, escrito uma
vez quando é criado; "versão antiga" para um de antes do arquivo). **Multijogador** guarda uma
lista de servidores, cada um com uma bolinha colorida (cinza não testado, âmbar testando, verde com
o ping, vermelho se não respondeu). A conta pertence ao servidor: você escolhe um servidor, depois
dá um nome e uma senha para ele, de onde sua identidade é derivada (Argon2id) em vez de ficar num
arquivo; a chave do servidor é fixada na primeira entrada. **Hospedar um servidor** abre seus
mundos para outros numa porta à sua escolha, e o menu de pausa mostra o endereço para passar a
eles. **Personagem** escolhe espécie, estilo, cores e acessórios. **Opções** tem o mouse, o volume,
o idioma (English, Italiano, Português) e a verificação de atualizações, e sua página
**Controles** troca as teclas; a página **Vídeo** tem janela, sem bordas ou tela cheia, a resolução
do jogo (independente do tamanho da janela, em todos os modos; a imagem é escalada para preencher),
VSync, limite de FPS, distância de visão, campo de visão, luz, sombras, detalhe dos portais, um céu
de tempestade simples (sem redemoinhos, para PCs fracos) e brilho para o escuro.

**As traduções** são `assets/lang/<código>.txt`, uma linha `texto como está escrito = tradução`
com `{}` para as partes que variam. Valem para tudo o que o cliente desenha: menus, painéis e
avisos dos mods, chat e texto do HUD, e nomes dos itens. Uma linha que falta deixa o texto como
está. `python3 tools/lang_check.py` lista o que os mods e mundos mostram e que o `it.txt` (para
texto em inglês) ou o `en.txt` (para texto em italiano) não tem; o `pt.txt` traduz as linhas dos
dois. Num sistema em português o jogo já abre em português.

### Cliente (`cnp_client`)

| Opção | O que faz |
|---|---|
| (nenhuma) | O menu de título (acima) |
| `--quick` | Início rápido, sem menu: um mundo local num processo só que não é salvo |
| `--connect HOST` `--port N` `--name NOME` | Entrar num servidor em vez de criar um mundo local |
| `--livery N` | A aparência do jogador, para uma entrada com `--connect` |
| `--addon-species NOME` | A espécie de um add-on, pelo nome, para uma entrada com `--connect` (cada servidor a numera do seu jeito) |
| `--server-key HEX` | Aceitar só o servidor com esta chave (o servidor a imprime ao iniciar) |
| `--player-key ARQUIVO` | Onde fica a identidade do jogador (padrão `player.key`, criado na primeira vez) |
| `--watch HOST:PORTA` | Olhar outro mundo como observador (repetível) |
| `--world-code refuse` | Não rodar o código que os mundos enviam ao cliente |
| `--no-update-check` | Não perguntar ao GitHub por uma versão mais nova (como Opções, Atualizações: não, mas só nesta execução) |
| `--media-cache PASTA` `--no-media-cache` `--media-cache-size MB` | Cache da mídia baixada |
| `--textures PASTA` (repetível) `--no-textures` | Pacotes de texturas; com mais de um, F7 alterna entre eles |
| `--models PASTA` `--no-models` `--model ARQUIVO.gltf` | Onde procurar modelos; um personagem na frente do spawn |
| `--mods PASTA` `--seed N` `--world N` | Mundo local: mods, semente, lado em blocos |
| `--user-mods PASTA` | Mundo local: mais mods, carregados com `--mods` como um só conjunto |
| `--play-world NOME` | Direto para esse mundo salvo, pulando a tela de título (o que o launcher inicia) |
| `--join-listed HOST:PORTA` | Essa linha da lista de servidores, como se fosse clicada (o launcher, seguindo um servidor) |
| `--day-length S` `--time T` | Duração do dia em segundos, hora inicial (0..1) |
| `--dual` `--facing` `--oriented` | Cenas de teste: dois mundos, dois portais frente a frente, um portal deitado |
| `--recursion N` `--portal-scale F` `--portal-config ARQUIVO` | Profundidade da recursão (0-3), resolução dos render targets, forma do portal ([`config/portal.cfg`](config/portal.cfg)) |
| `--log-view` | Registrar a vista em volta de cada travessia |
| `--bench` `--dual-bench` `--soak S` | Medições: FPS e custo dos scripts, custo do segundo mundo, voo automático |
| `--verify` | O teste de continuidade dos portais (rodado pelo `ctest`) |
| `--play-seconds S` `--watchdog S` | Sair depois de S segundos; despejar a pilha se um frame não chegar em S segundos |
| `--shot S` | Uma captura em `bench/play.png` depois de S segundos de uma execução cronometrada |
| `--fly-down N` `--walked-down` | Numa execução cronometrada: subir N blocos voando e descer, informando quanto custa pousar |

A lista completa está em `main()` em [`src/client/main.cpp`](src/client/main.cpp).

### Servidor (`cnp_server`)

| Opção | O que faz |
|---|---|
| `--listen PORTA` | Aceitar jogadores. Sem ela, gera o mundo, roda `--ticks` ticks e imprime o hash |
| `--bind HOST` | O endereço onde escutar (padrão todos); o menu do cliente usa 127.0.0.1 |
| `--world N` `--seed N` `--world-file ARQUIVO` | Lado da região gerada ao iniciar (padrão 128; o mundo não tem borda), semente (padrão 1), arquivo SQLite onde salvar. Um servidor por arquivo: um segundo encontra a trava e sai |
| `--world-depth N` `--world-top N` | Só quando um mundo é criado: até onde vai a rocha escavável abaixo de y 0 (padrão 0, a rocha-mãe antiga em y 0; até 1024, em passos de 32, sobre um piso de rocha-mãe) e a altura do mundo (padrão 64). Guardado ao lado do arquivo do mundo como `<arquivo>.shape`; um mundo criado antes fica com 0 e 64. O mundo novo da tela de título usa 128 e 256 por padrão |
| `--save-every S` | Com que frequência gravar o que mudou (padrão 10 s; 0 só ao desligar). Ctrl+C ou SIGTERM salvam e param |
| `--mods PASTA` `--no-mods` | Os mods do mundo |
| `--user-mods PASTA` (repetível) `--no-mod NOME` (repetível) | Mais mods, os do jogador, carregados com `--mods` como um só conjunto: uma ordem de `depends`, e um nome já carregado é recusado. Um mundo iniciado pela tela de título passa `~/.cubes-and-portals/mods` (Windows: `%APPDATA%/cubes-and-portals/mods`) para a sala do sandbox e os salões de bolso, depois os `mods/` do próprio mundo, e `--no-mod` para cada nome que o `mods.conf` dele desliga (`off = a, b`). Um mod entre eles com `rooms = all` o servidor carrega em todas as salas |
| `--add-mod PASTA` (repetível) | A pasta de um mod, carregado com os outros; o servidor a escreve sozinho para um mod `rooms = all` |
| `--textures PASTA` `--no-textures` | A mídia que o mundo serve |
| `--day-length S` `--time T` | Ciclo de dia e noite |
| `--key-file ARQUIVO` `--no-encryption` | A identidade do servidor; criptografia desligada (só para testes) |
| `--max-players N` `--require-identity` `--no-observers` | Vagas, jogadores obrigados a ter uma chave, nenhum observador de outros mundos. Um nome pertence a quem o usou primeiro, guardado no arquivo do mundo |
| `--names` `--free-name NOME` | Com `--world-file` e nenhum servidor rodando nele: quem é dono de cada nome, ou liberar um. Quem entrar depois com esse nome herda a bolsa e a posição do dono anterior |
| `--owner NOME` | Quem joga como NOME com uma identidade é dono do servidor. O cliente passa isso para os mundos que inicia |
| `--config ARQUIVO` | As opções de um arquivo, uma `chave valor` (ou `chave = valor`) por linha, `[room NOME]` para uma sala, comentários com `#`; `chave = true` é a opção sem valor. Lido onde está, então as opções depois dele prevalecem |
| `--rcon PORTA` `--rcon-password SENHA` | Comandos de admin por TCP só em 127.0.0.1: `tools/admin.sh -p PORTA ban bob 2h`. Coloque a senha no arquivo de configuração, não na linha de comando |
| `--motd TEXTO` | A mensagem do dia, dita no chat a cada jogador que entra (também `/motd`) |
| `--restart-at HH:MM` | Um reinício todo dia, com dez minutos de avisos; sob o `tools/serve.sh`, que o inicia de novo |
| `--backup-every 6h` `--backup-keep N` | Cópias de cada arquivo de mundo ao lado dele (`NOME.backup-DATA`), feitas com o servidor rodando; as N mais novas ficam (5). `/backup` faz uma agora |
| `--max-per-address N` | No máximo N jogadores de um mesmo endereço |
| `--watchdog S` | Uma sala que não terminou um tick em S segundos derruba o servidor com a pilha impressa, para o `tools/serve.sh` iniciá-lo de novo |
| `--federation PORTA` | Servidores amigos (`/friend`): linhas seladas e assinadas nesta porta TCP, sob a chave dos tickets (`--ticket-key`, ou WORLD.sign) |
| `--rules-file ARQUIVO` `--no-update-check` | Regras que cada jogador aceita antes de jogar; não perguntar ao GitHub por uma versão nova ([`docs/ADMIN.md`](docs/ADMIN.md)) |
| `--webhook URL` | Ações de admin, início, parada e denúncias enviadas por POST para lá em JSON ([`docs/ADMIN.md`](docs/ADMIN.md#webhooks)) |
| `--report ARQUIVO` `--since 7d` `--json` | Sem servidor: o que um arquivo de mundo lembra -- jogadores, tempo de jogo, máximo ao mesmo tempo, entradas por hora, salas, quem foi recusado, ações da equipe, reinícios e travamentos, travessias vindas de outros servidores |
| `--portals` `--portal-to HOST:PORTA[/N]` | Um par de aberturas controladas pelo servidor; `--portal-to` diz aonde a primeira leva (e por qual abertura `N` você sai); repetido, a segunda, terceira e quarta |
| `--ticket-key ARQUIVO` `--portal-peer-key HEX` `--trust HEX` `--require-ticket` | Tickets assinados para atravessar entre servidores |
| `--mobs N` `--ticks N` `--quiet` | Criaturas de teste, ticks antes de sair, menos log |
| `--rooms-file ARQUIVO` | Salas iniciadas e paradas com o servidor rodando: uma linha `[room NOME]` e as opções dessa sala; uma inicia quando um portal ou um jogador leva até ela e salva e para quando fica sozinha. Um nome que termina com asterisco (`[room home-*]`) é um modelo, uma sala feita a partir dele para cada nome pedido: a sala própria de cada jogador. `/room` as gerencia ([ADMIN](docs/ADMIN.md)) |
| `--room NOME` | Uma sala: mais um mundo no mesmo servidor e na mesma porta. O que vem depois vale só para essa sala (`--mods`, `--world-file`, `--portal-to`...), o que vem antes do primeiro `--room` vale para todas. `--portal-to @NOME[/N]` leva a outra sala; bolsa e vida são compartilhadas entre as salas. O cliente escolhe uma sala com `--room NOME` |

### Administração do servidor

Digitados no chat por um jogador cujo cargo permite, no stdin do servidor sem a barra, por RCON
(`tools/admin.sh`), ou com os botões da página `/admin`. Cargos (convidado, jogador, ajudante,
moderador, admin, dono, também por sala), banimentos, silenciamentos, a lista branca, advertências,
denúncias e notas, regras para aceitar, contas alternativas, reinícios com avisos e manutenção, a
mensagem do dia, visitas e `--report`, `/inspect` e `/rollback` do que um jogador mudou, backups,
webhooks, `/bag` e `/give`, e servidores amigos que compartilham banimentos, presença, um chat, uma
bolsa para cada jogador e portais construídos entre eles (uma moldura dourada que um admin
configura pelo menu).

**Todo comando, permissão, opção e chave do `server.conf` está em [`docs/ADMIN.md`](docs/ADMIN.md);
todo comando de chat, os dos jogadores também, com o que faz, em [`docs/COMMANDS.md`](docs/COMMANDS.md).**
`/help` no jogo lista os que você pode usar.

`tools/serve.sh -l logs build-release/src/server/cnp_server --config server.conf` mantém um
servidor rodando através de reinícios e travamentos, com um log por dia.

### Ferramentas

| | |
|---|---|
| `tools/play.sh` (`local`, `--fresh`) | Compila em Release, inicia servidor e cliente com todos os mods e um mundo salvo em `worlds/play/` |
| `tools/track.sh` (`--fresh`) | Um servidor com o sandbox e todos os mundos como salas, como o menu de título o inicia mas com todos os mundos liberados (uma cópia de `mods/keys` com `ALL_OPEN`; mundos e logs em `worlds/track/`); `--fresh` também torna você dono, então o catálogo criativo (C) fica aberto |
| `tools/arpg.sh`, `tools/fps.sh`, `tools/chambers.sh`, `tools/platformer.sh`, `tools/shmup.sh` (`--fresh`) | O sandbox e um mundo, para trabalhar nesse mundo |
| `tools/portallab.sh` | Uma cena para conferir portais a olho (games/portallab): vidro visto pelas aberturas, uma no teto para pular dentro, dois pisos para quicar entre eles, aberturas que deslizam, perseguem e giram em todos os eixos; `/lab wall`, `glass`, `head`, `bounce`, `view`, `slide`, `chase`, `spin`, `spiny`, `spinz`, `still`, `at <graus>`; uma abertura do tamanho de uma porta para uma com um oitavo da altura que encolhe você para 1/16, `/lab tiny`, `g1`, `scale <s>` |
| `tools/windowlab.sh` | Corpos vistos por uma janela para outro mundo (games/windowlab): duas salas, seis criaturas marchando diante da boca distante. |
| `tools/vehiclelab.sh` | Veículos pelos portais (games/vehiclelab, sobre os mods do sandbox): uma linha de carrinho que entra numa abertura e sai de outra girada um quarto, um canal de barco igual; um portal levado por um trem numa curva; `NAME=boat` ou `train` começa lá, `/vlab rail`, `boat`, `train`, `watch`; um cliente cronometrado precisa de `--keep-portals` |
| `build/tools/cnp_modpack pack mods/ore` | Um mod num arquivo `.cnpmod`, que carrega de `mods/` como uma pasta |
| `tools/dist/package-*.sh` | Os pacotes para Linux, Windows e macOS ([`docs/BUILDING.md`](docs/BUILDING.md)) |
| `build/tools/cnp_netshape` | Um relay UDP com atraso e perda, para testar conexões lentas |
| `build/tools/libcnp_clock_skew.so` | Um `LD_PRELOAD` que desloca o relógio de um processo, para os tickets |
| `tools/two_machines.sh` | Dois servidores em contêineres, com MTU e NAT reais |
| `tools/admin.sh` (`-p PORTA`) | Um comando de admin para um servidor rodando, por RCON; a senha em `CNP_RCON_PASSWORD` |
| `tools/serve.sh` (`-l PASTA` `-k DIAS` `-w URL`) | Mantém um servidor rodando através de reinícios e travamentos, um log por dia; travamentos enviados por POST à URL |
| `build/tools/cnp_bake_textures` | Gera de novo as texturas provisórias dos blocos |
| `tools/make_torch_models.py` | Os modelos da tocha (na mão, no chão, na parede, no teto), no Blender |

### Testes

Treze, escolhidos porque protegem coisas que quebram em silêncio e custam caro: entrada da rede
(`protocol`, `chunk_codec`, `crypto`), mundos salvos (`storage`, `migration`, `persistence`),
matemática e colisões dos portais (`portal_math`, `portal_traverse`, `physics_box`,
`portal_continuity` na GPU), a sandbox dos mods (`script_isolation`), e cliente e servidor de
acordo sobre o mundo (`worldgen`, `two_process_join`). O resto é conferido jogando. Por que cada um
existe está em [`tests/CMakeLists.txt`](tests/CMakeLists.txt).

## Tecnologia

| | |
|---|---|
| Gráficos | raylib 6.0 atrás de uma interface `gfx_*` fina, OpenGL 3.3 core |
| Linguagens | C++20 para o motor, Luau 0.733 para os scripts |
| Rede | ENet 1.3.18, zstd 1.5.7, Monocypher 4.0.3 |
| Física | Jolt (veículos) e a do próprio motor (corpos, blocos, portais) |
| Armazenamento | SQLite 3.50.4 |
| ECS | Flecs 4.1.6 |

## Documentação

Os documentos estão em italiano, muitos com uma versão em inglês ao lado (`.md` em inglês,
`.it.md` em italiano).

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): como o código está organizado hoje e onde entra
  um recurso novo.
- [`docs/DESIGN.md`](docs/DESIGN.md): estudo de viabilidade, decisões, plano de marcos, riscos.
- [`docs/MODDING.md`](docs/MODDING.md): escrever um mod; [`docs/API.md`](docs/API.md): a API Luau,
  gerada a partir de `src/script/api.luau`.
- [`docs/TEXTURE_PACKS.md`](docs/TEXTURE_PACKS.md): fazer um pacote de texturas.
- [`docs/BUILDING.md`](docs/BUILDING.md): compilação, pacotes, versões e o repositório Flatpak.
- [`TODO.md`](TODO.md) (inglês): o que falta e o que está quebrado.
- [`docs/archive/`](docs/archive/README.md): a história, marco por marco, com as medições.

## Licença

O código está sob a licença MIT ([`LICENSE`](LICENSE)). As bibliotecas em `third_party/` têm cada
uma a sua licença, na própria pasta.
