# Arquitetura

*[Read in English](ARCHITECTURE.md) · [Leggi in italiano](ARCHITECTURE.it.md)*

Como o código é feito hoje, e onde entra uma funcionalidade nova. O porquê das grandes escolhas está
em [`DESIGN.pt.md`](DESIGN.pt.md); o porquê das pequenas quase sempre está no comentário no topo do
arquivo.

## Programas e bibliotecas

```
cnp_client ─┬─ cnp_script ─┬─ cnp_core
            ├─ cnp_net ────┘
            └─ raylib (só em src/client/gfx e src/client/snd)
cnp_server ─── cnp_script, cnp_net, cnp_core      (sem GPU, sem janela)
```

| Target | O que contém |
|---|---|
| `cnp_core` | Tudo o que não tem nem socket nem GPU: voxels, luz, mesher, mapgen, física, portais, registros, itens, ECS, mídia, protocolo e a máquina de estados das sessões |
| `cnp_net` | O que toca o mundo de fora: o transporte ENet, a criptografia, o codec dos chunks, `ClientSession`, SQLite (`storage/world_db`) |
| `cnp_script` | O runtime Luau, `ModHost` (uma VM por mod), o barramento de eventos, o código que um mundo manda ao cliente |
| `cnp_modpack_lib` | O formato `.cnpmod` |
| `cnp_client` | Janela, render, entrada, áudio, HUD; jogo local ou conectado |
| `cnp_server` | O servidor sem janela |
| `tools/` | `cnp_api_gen` (bindings), `cnp_modpack`, `cnp_bake_textures`, `cnp_netshape`, `cnp_clock_skew` |

## Regras que o código segue

- **Nada de raylib acima de `gfx.h`, `platform.h` e `snd.h`.** É o que mantém aberta a mudança
  para o sokol. `src/core` nunca inclui nada gráfico.
- **Nenhum estado global por mundo.** Tudo o que pertence a um mundo fica numa `Session` do
  cliente (`src/client/main.cpp`), e a `Scene` guarda mais de uma: é assim que um portal mostra
  outro servidor.
- **O servidor é a autoridade, mas não simula a física.** O cliente move o corpo e manda uma
  declaração; o servidor a **confere** (`check_move` em `core/physics/movement.h`, chamada de
  `net/session.cpp`) e corrige. Cavar, colocar, golpes, baús e fabricação são decididos pelo
  servidor.
- **Uma regra é escrita uma vez.** Quando cliente e servidor precisam concordar (quanto tempo leva
  para cavar, se uma caixa está dentro de um bloco, onde alguém nasce), a função fica em `core/` e
  os dois a chamam.
- **Os ids são dados na ordem de registro; os nomes viajam na rede e no disco.** O cliente adota a
  tabela de blocos do servidor; o arquivo do mundo recusa uma tabela diferente.
- **A mídia é endereçada pelo conteúdo** (`core/media/content_hash.h`): o nome de um arquivo é o
  hash dos seus bytes, e o cache do cliente é um só para todos os mundos.
- **O motor emite eventos e não tem regras de jogo particulares.** Proteger uma área, substituir um
  bloco, gerar minérios: são mods.

## Um mapa de `src/`

### `src/core`

| Pasta | Arquivos principais |
|---|---|
| `voxel/` | `chunk` (32³, id + luz + orientação), `mesher` (greedy, vértice de 12 bytes com luz suave e direcional), `mesh_pool` (meshing em threads), `light` (flood fill do sol e dos blocos), `raycast`, `dig` (o lado do cliente ao cavar), `find` |
| `worldgen/` | `worldgen` (etapas em ordem, uma coluna de chunks por vez; `world_hash`), `biome`, `noise`, `spawn` (na frente de uma abertura, na praça, ou em terra firme num mundo de sobrevivência) |
| `world/` | `columns` (quais colunas estão na memória: blocos, depois luz, depois liberadas), `daylight` (o relógio do mundo; `sky_light_at`: sol e lua para desenhar), `modifiers` (ABM/LBM), `node_meta` (o que uma célula lembra) |
| `registry/` | `node_registry` (blocos), `node_shape` (as caixas de um bloco, giradas), `core_nodes` (os blocos do motor), `model_registry` |
| `physics/` | `aabb` (a caixa do jogador contra o mundo), `movement` (limites e validação), `fall`, `rigid` (caixas que tombam: um mundo Jolt por mundo ECS, os blocos em volta de cada caixa como chão estático; `core.mob_tumble`) |
| `portal/` | `portal_math` (near plane oblíquo, plano de corte), `portal_traverse` (onde um corpo sai, virado para onde), `portal_clip` (o corte dos sólidos), `portal_tuning` (os seis números de `config/portal.cfg`) |
| `item/` | `item_registry`, `inventory`, `container` (baús), `recipe`, `craft_grid`, `item_codec` (no disco) |
| `ecs/` | `components` (Flecs), `health`, `drop`, `mob`, `lua_ecs` (os componentes e sistemas dos mods) |
| `media/` | `content_hash`, `media_set` (lado do servidor), `media_cache` (lado do cliente), `png`, `gltf`, `texture_pack`, `face_tiles`, `tile_bake` |
| `net/` | `protocol` (mensagens, versão, handshake), `session` (`ServerSessions`: tudo o que o servidor faz com um peer, sem socket), `client_session`, `interest` (quais chunks e em que ordem), `chunk_codec`, `transport` (ENet), `crypto`, `bytes` |
| `storage/` | `world_db` (SQLite: chunks, baús, bolsas, metadados, migrações), `column_keeper` (as colunas que entram, ganham luz e são liberadas; servidor e cliente sozinho) |
| `ui/` | `panel` (painéis declarados pelos mods, layout e teste de clique) |
| `audio/` `diag/` `math/` | Atenuação e tons; profiler e dump da pilha; vetores, matrizes, frustum |

### `src/script`

| Arquivo | |
|---|---|
| `script.h`, `script_luau.cpp` | A fronteira com o Luau: VM, cotas, interrupção, permissões retiradas antes de o mod começar, a tabela `script_host` de funções que o motor oferece |
| `api.luau` → `api_generated.inc` | As assinaturas da API, **uma vez só**; a cola C++, `docs/api.d.luau` e `docs/API.md` são gerados (`cnp_bindings`) |
| `mod_host` | Carrega `mod.conf`, versões e dependências, uma VM por mod, hot reload, `side = server/client/both` |
| `event_bus` | Eventos com esquema, prioridades, cancelamento |
| `world_code` | O código que um mundo manda ao cliente: o nome dele na mídia, o pacote, uma tabela host reduzida |

### `src/client`

| Arquivo | |
|---|---|
| `main.cpp` | `Session` (um mundo) e `Scene` (todos os mundos), as funções host dos mods do lado do cliente (`make_client_host`), `run_interactive` (jogo local), `run_network` (jogo conectado), `run_bench`, `run_dual_bench`, `run_soak`, `run_verify`, e `main()` com as flags; o menu de título e `serve_world` (um mundo só seu, sandbox ou sobrevivência) |
| `player.cpp` | O movimento do jogador: entrada, gravidade, colisão, degrau |
| `gfx/` `snd/` | O único lugar que inclui o raylib |
| `render/` | `world_render` (chunks e atlas), `portal_render` (targets, recursão, `draw_closed`: a membrana dos portais fechados e a sua abertura), `body_render`, `model_render` e `model_store` (glTF), `selection`, `mod_pass` (o pass dos mods), `shaders.h` |
| `ui/hud.cpp` | Hotbar, bolsa, painéis, barra de cavar, avisos |

### `src/server/main.cpp`

Flags, a criação do mundo (a região em volta do spawn, mods, arquivo), as funções host dos mods do
lado do servidor, e o tick de 20 Hz: as mensagens dos peers, os passos dos mods e dos sistemas ECS,
ABMs, colunas geradas ou lidas de volta em volta de quem está lá e liberadas onde não há ninguém,
envio dos chunks por interesse, entidades, vida, tempo, portas entre servidores.

A **dificuldade** é um número da sala (`ServerState::difficulty`, 0 pacífico .. 3 difícil), guardado
como `<arquivo do mundo>.difficulty` para que o menu de título o leia e escreva sem servidor. O
pacífico tem só duas barreiras: o spawn natural é pulado e `host_spawn_mob` recusa um tipo que
`fights()`; ligá-lo remove os que estão por aí. Fácil e difícil mudam o limite de spawn e o golpe
que uma criatura dá num jogador (o callback hurt de `step_mobs`). O que um mod faz nas suas próprias
lutas (cercos, o chefe de um altar) ele pergunta a `core.peaceful()`, e `core.mob_leave` dá os
drops de um tipo sem criatura. As **regras** do mundo (`core.rule`) são nomes e valores que o motor
guarda em `<arquivo do mundo>.rules` sem dar significado a eles: cada mod lê as suas.

**O mundo não tem borda.** Uma coluna de chunks entra na memória quando uma sessão chega a quatro
colunas dela: do arquivo do mundo se estiver lá, do gerador se não estiver (o core e as etapas dos
mods, escritas só dentro da coluna). Ela tem luz quando as oito em volta têm os seus blocos, e só
então é enviada, salva e varrida pelos ABMs. Ela sai depois de trinta segundos a mais de oito
colunas de todas as sessões e de tudo o que está nela (pilhas, mobs, as coisas dos mods): se tem
luz, é guardada no arquivo, ou na memória comprimida se mudou e não há arquivo, e quem tinha os
chunks dela recebe `ChunksGone`. Essas regras são uma classe só, `core/storage/column_keeper`, usada
pelo servidor e pelo cliente que joga sozinho; só mudam os hooks (quais etapas, quem avisar).

## Como uma partida roda

**Local.** `cnp_client` gera o mundo com o mesmo `Mapgen` do servidor, carrega os mods de `mods/`
na `Session`, e `run_interactive` roda entrada, `player_update`, mods, meshing e render no mesmo
processo.

**Conectado.** `ClientSession::connect` abre a conexão criptografada e manda `Hello`. O servidor
responde com `Welcome`: semente, o hash do mundo, recursos, os canais dos mods, a tabela de blocos
com formas e modelos, os limites de movimento. O cliente adota a tabela, baixa a mídia que não tem,
carrega o código do mundo a menos que o recuse, e daí em diante recebe chunks (o canal `Chunk`, por
onde também chega `ChunksGone` com os que devem ser liberados), corpos (`Entity`, não confiável) e
eventos (`Event`); ele manda declarações de posição, cavadas, colocações, golpes e cliques. Os
canais de `kModChannelBase` (6) para cima são dos mods.

**Atravessar.** Um portal local move o corpo com `portal_traverse`; o servidor decide e manda um
`Teleport` com a velocidade de saída (`exit_velocity` em `core/net/session.cpp`): a velocidade de
entrada girada pelo mapa, com a energia igualada só entre o ponto de entrada mapeado pelo par e o
ponto de saída, nunca entre as alturas das duas aberturas. Um portal para outro servidor usa um
ticket assinado: o cliente já tem uma sessão de observador aberta naquele mundo (para desenhá-lo
dentro da abertura) e a promove a sessão ativa. Se não puder, o servidor manda um `Redirect` e o
cliente se reconecta.

**Salas.** Um servidor pode ter vários mundos na mesma porta (`--room`, protocolo 56): cada um é
`serve_room` numa thread própria, e o `Router` passa a cada sala os eventos dos seus peers. Uma sala
de cada vez segura a trava comum (`TurnLock` em `src/server/main.cpp`) e só a solta enquanto espera
a rede; a trava é passada adiante **na ordem em que foi pedida**, e uma sala atrasada nos ticks a
solta uma vez por poll de qualquer jeito. Com um `std::mutex`, uma sala ocupada (o Inferno gerando
colunas) a pegava de volta antes que as outras acordassem, e os jogadores da pista ficavam sem
resposta por mais de um segundo. Cada sala escreve no log por quanto tempo a segurou no máximo. As
bocas de uma sala (até 28 desde o protocolo 116) são os seus caminhos para as outras.

## Como a luz é desenhada

O servidor calcula só os níveis 0..15 (sol e blocos, flood fill em `core/voxel/light.cpp`) e os
manda com os chunks; todo o resto é do cliente, e o jogo (spawn, `core.light`) continua lendo os
níveis e `daylight_at`.

- **Mesher** (`core/voxel/mesher.cpp`): para cada canto de uma face, a média das quatro células que
  se tocam ali, já passada por `light_curve` (brilho, não nível). A luz dos blocos é virada para a
  face: a direção é a inclinação do flood fill (subindo em direção à lâmpada), cheia de frente, 0.6
  de lado, 0.25 por trás (`kGrazing`, `kAway`, `kNormalBias`). Por isso o snapshot tem uma borda de
  duas células e não de uma. Um bloco luminoso ilumina a si mesmo.
- **O shader do mundo** (`render/shaders.h`): céu = visibilidade do céu × (ambiente pela direção da
  face + sol × N·L × sombra); depois o mais claro entre céu e lâmpadas, × AO, sobre um piso de 2%
  (`kLightFloor`).
- **Sombras** (`WorldRenderer::draw_shadows`): um shadow map ortográfico em volta da câmera, preso
  à grade dos texels, com as folhas recortadas pelo alfa do atlas. A vista principal o desenha, e
  cada vista dentro de um portal para a sua própria câmera virtual, antes de abrir o seu próprio
  target (`gfx_shadow_create`: só profundidade, comparada no hardware).
- **Sol e lua** no pass das nuvens; **corpos e coisas na mão** pegam a luz da sua célula
  (`WorldRenderer::cell_light`).
- **Clássica** (`set_classic`, Opções → Vídeo, Luz: clássica): a fórmula antiga, a partir do byte de
  luz compactado que o vértice ainda carrega; sem sombras e sem sol.

## Armadilhas em que já se caiu

Cada uma destas foi um defeito encontrado jogando, e cada regra abaixo é o que impede que ele volte.
Quando uma mudança mexe numa delas, confira do jeito que a linha diz.

- **O rlgl solta o nosso shader pelas nossas costas.** `rlSetBlendMode`, ao mudar de modo, primeiro
  desenha o batch do próprio raylib, e se esse batch tem alguma coisa (texto de depuração, 2D) ele
  termina com `glUseProgram(0)`: o próximo desenho sai sem programa e não vai parar em lugar
  nenhum. `gfx_state_set` liga `g.current_shader` de novo logo depois. Toda chamada nova ao rlgl que
  possa esvaziar o batch (`rlDrawRenderBatch*`, `rlSetBlendMode`, `rlSetShader`) entre ligar um
  shader e desenhar precisa fazer o mesmo. Sintoma: o vidro (o pass translúcido) faltando em
  algumas cenas e não em outras, direto e pelos portais (2026-11-02). Conferir: o vidro das câmaras
  de teste com F3 ligado, e `tools/portallab.sh` `/lab view`.
- **Uma rotação espera com o seu bloco.** `core.set_facing` numa coluna que ainda não aceita
  escritas é adiado e conferido quando é aplicado, no bloco que está lá então -- nunca contra a
  célula como ela está antes (o ar que uma porta substitui). Sintoma: portas e peças de pista
  viradas para o lado errado de vez em quando. Conferir: o log do servidor de `tools/track.sh` em
  mundos novos não tem "is not a turn".
- **Uma travessia tem um só ponto do corpo.** Os PÉS por um piso (chegam lá primeiro), o OLHO por
  uma parede e por um teto (`crossing_point`, `core/portal/portal_traverse.cpp`). Medido nos pés, um
  teto levava antes a cabeça 1.6 para dentro do bloco. Conferir: `/lab head`, um pulo sob o teto
  atravessa.
- **A saída continua a imagem.** `exit = trigger + view` (config/portal.cfg) vale para uma
  travessia pelo olho. De um piso para uma parede, o olho ainda está na sua altura acima do plano,
  então a saída é onde a imagem o mostrava, até onde um corpo cabe no bloco da abertura. Conferir:
  o salto entre onde a abertura mostrava o olho e onde ele é colocado -- 0.01 de parede para
  parede, 0.92 de piso para parede (o resto é parede); uma mudança que aumente qualquer um dos dois
  é uma regressão.
- **Entre duas aberturas horizontais a direção é mantida** (`portal_carry_look`): só a inclinação
  segue o mapa, e a rolagem volta a ficar nivelada (`settle_roll`, suave, 0.25-0.6 s). O olhar
  nunca é girado a pé depois de uma travessia; só o horizonte. Conferir: `/lab bounce`, o yaw fica
  o mesmo em toda travessia.
- **Um corpo remoto desliza de onde tinha chegado.** `glide` (`main.cpp` do cliente) começa cada
  trecho novo do ponto a que o anterior tinha chegado, nunca de onde o corpo foi desenhado no frame
  anterior com o relógio zerado: com uma mensagem a cada frame (abaixo de ~20 fps), esse recomeço
  deixava todos os outros corpos parados onde foram vistos primeiro (2026-11-07). Conferir: os
  insetos do jardim se movem no Xephyr.
- **O centro de uma abertura é o meio de um bloco.** Ao longo da frente dela, `Portal::center` é o
  centro de um bloco e a imagem fica na face de trás desse bloco (`plane` em `config/portal.cfg`).
  Um centro na divisa entre blocos põe a abertura meio bloco para fora da parede e a imagem meio
  bloco para dentro: as portas do front ficaram assim até 2026-11-08 e não dava para ver através
  delas direito. Para uma abertura embutida, esvazie esse bloco e dê um bloco à parede atrás dele.
  Conferir: as duas portas da galeria.
- **Uma janela é só para os olhos** (`Portal::view`, `core.portal_window`, protocolo 120). `view` 1
  é olhada através, 2 é aquela de onde se olha e não é desenhada de lugar nenhum; nenhuma das duas é
  atravessada por um corpo ou uma criatura, nem dá isenção de piso. O servidor manda tudo o que a
  ponta distante abrange em largura (`PlayerInterest::Window::radius`, e os corpos em
  `entities_for`); o cliente dá à janela um `PortalLink` e nenhum lugar em `linked_openings`.
  Conferir: debaixo da ponta distante da miniatura do front o céu está vazio, e o vidro da galeria
  em cima dela segura o corpo.
- **Corpos pequenos mantêm a caixa sob os pés.** `body_box` levanta a caixa no máximo
  `altura - min(0.1, altura/2)`; o antigo "altura - 0.1" ficava negativo abaixo de 0.2 de altura e
  pendurava a caixa 8 cm abaixo dos pés. Uma criatura nunca é menor que 0.01 × 0.01
  (`size_mob_kind`, e a caixa de mira do cliente também). Uma abertura que escala, menor que uma
  porta, tem cada profundidade menor na mesma proporção (`portal_depth_scale`: vão, plano da imagem,
  ponto de vista, corte, gatilho, saída), senão um olho encolhido fica dentro de um tubo oito vezes
  mais fundo que o buraco e vê o céu. A saída dela faz o corpo, no novo tamanho, caber dentro do
  buraco distante: um dezesseis avos um fio fora do centro saía pela portinha do jardim metade na
  cerca viva, preso; e o `ahead_of_crossing` do cliente coloca o corpo onde a imagem o mostrava, a
  partir dos pés de antes do passo daquele frame (já dado com as medidas do corpo novo, que chegam
  logo antes da travessia), a menos que ele não caiba ali. Conferir: `/lab tiny`, as formigas do
  jardim na grama, sair do G fora do centro e voltar.
- **Um par que escala só aceita um corpo que consegue redimensionar.** Um que poria a escala de um
  jogador fora de 1/32..4 (`kMinBodyScale`, `kMaxBodyScale`) não o deixa atravessar: o lugar seria o
  do par e o tamanho o do limite. Conferir: um pulo na redoma de vidro do jardim fica no vidro.
- **Os raios do céu desfazem a projeção e a vista separadamente.** `kWaterSkyVS` aplica
  `inverse(uView)` a `inverse(uProjection) * ponto`. `inverse(P * V)` fazia as nuvens tremerem 0,82
  bloco em 1/16 enquanto o olho se movia; assim, no mesmo teste em float, sobra 0,0000.
- **Nada de near plane oblíquo mais perto que o near plane.** `portal_oblique_near` desiste quando
  a câmera virtual está mais perto do plano de corte do destino do que o seu próprio near plane: um
  plano oblíquo tão perto espreme a faixa de profundidade e o outro lado sai listrado, folhas e céu
  em faixas. Tudo o que fica no meio já é cortado pelo near plane. Um par que encolhe põe a câmera
  tão perto durante um dezesseis avos de bloco do caminho na sua ponta grande, e frames caíam ali
  (a portinha do jardim, "a imagem pisca", 2026-11-07). Conferir: a portinha do jardim com o olho
  em 40.016..40.04 (a face da frente dela é 40), sem listras; `portal_continuity` 0 pixels.
- **Uma cena dedicada antes de um palpite.** `tools/portallab.sh` monta casos de portal em que cada
  número pode ser lido (com sol, sem outros mods). Quando um defeito aparece num jogo mas não lá, a
  diferença entre os dois é a causa: liste-a antes de mudar qualquer coisa.

## Onde entra uma funcionalidade

| O quê | Onde |
|---|---|
| Uma função Lua nova | A assinatura em `src/script/api.luau`; a função host em `src/server/main.cpp` e, se ela também for necessária no jogo local, em `make_client_host` de `src/client/main.cpp`; duas linhas em `docs/MODDING.md` (e `MODDING.it.md`, `MODDING.pt.md`). `docs/API.md` se regenera sozinho |
| Um evento do motor | O esquema entre os eventos do motor (`define_core_events` no cliente, a tabela de eventos no `main()` do servidor) e o `emit` onde ele acontece |
| Uma regra de jogo | Primeiro pergunte se pode ser um mod. Se é do motor: a função em `src/core/`, chamada pelo servidor (`net/session.cpp`) e pelo cliente local |
| Uma mensagem de rede | Uma struct com `encode`/`decode` em `core/net/protocol.h`, `kProtocol` +1 com uma linha dizendo o que muda, o tratamento em `net/session.cpp` e `net/client_session.cpp`. Um decoder precisa recusar entrada truncada: o teste `protocol` testa isso |
| Dados salvos | `core/storage/world_db`, com `kWorldSchemaVersion` +1 e uma migração do formato anterior (o teste `migration`) |
| Conteúdo (blocos, itens, mapgen, mobs) | Um mod em `mods/`, veja [`MODDING.pt.md`](MODDING.pt.md) |
| Gráficos | `src/client/render/`, por meio de `gfx_*`; se precisar de uma primitiva nova, em `gfx.h` e `gfx_raylib.cpp` |

## Mods que vêm com o jogo

| Mod | |
|---|---|
| `spike` | O mod do spike M0: blocos, um portal que se move |
| `beacons` | Pilares luminosos que pulsam, com a permissão `os` |
| `guard` | Protege a praça dos portais cancelando `core:pre_place` |
| `palette` | Vinte blocos coloridos e um pincel |
| `console` | Um painel declarado em Lua |
| `chest`, `craft`, `tools` | Baús, receitas, ferramentas com nível e dureza |
| `steps` | Uma laje, uma escada e um bloco desenhado como modelo |
| `timber` | Toras que giram, degraus na face clicada, musgo com ABM/LBM |
| `cave`, `ore`, `trees`, `lakes`, `ruins` | Cavernas ligadas, minérios, árvores, lagos e torres em ruínas como etapas do mapgen, uma coluna por vez |
| `depths` | Minas abandonadas e criptas no subsolo, planejadas por região e construídas uma coluna por vez |
| `portal` | Portais feitos de blocos: uma moldura de `portal:frame` em volta de um buraco de ar, em pé ou deitada, abre uma abertura; clique direito na moldura para o menu |
| `furnace` | Uma fornalha: o carvão funde minério bruto em lingotes, areia em vidro, e comida; ela guarda a experiência da fundição e a dá a quem a abre (`furnace:xp`) |
| `creative` | Um inventário criativo num painel, tecla C (`side = both`); pede a permissão `creative` |
| `doors` | Portas de dois blocos, também para quem procura um caminho (`core.find_path`) |
| `mobs`, `robot` | Zumbis, esqueletos, o robô que é o corpo do jogador |
| `animals`, `pets` | Porcos, vacas, ovelhas, galinhas (comida, lã, ovos, couro, criação); gato, cachorro, vara de pescar |
| `armor` | Dezesseis peças de armadura nos quatro espaços vestidos |
| `infernoportal` | A obsidiana e o portal do Inferno construído à mão |
| `farm` | Enxada, terra arada, trigo em quatro estágios, pão, pés de melancia e de abóbora |
| `bed` | Cama: renascer; à noite você se deita e, com todos na cama, amanhece, mas não com monstros perto (`core.player_spawn`, `core.set_time`, `core.monsters_near`) |
| `screwdriver` | Chave de fenda: gira um bloco já colocado (`core.node_turns`) |
| `compact` | Blocos de nove lingotes, gemas ou pós |
| `cinnabar` | Pó, alavanca, botão, tocha, repetidor, comparador, lâmpada, pistões, observador, funil; o circuito é resolvido em Lua com uma fila de mudanças atrasadas |
| `torches`, `cane` | Tochas; cana-de-açúcar nas margens, papel, livros |
| `hunger` | Fome com saturação e cansaço, uma barra que o mod desenha (`side = both`) |
| `enchant` | Experiência e uma mesa de encantamento com sorteio e uma dica (`side = both`) |
| `gates`, `keys`, `grave` | As portas dos mundos (na praça, ou num mundo de sobrevivência os locais espalhados e as suas bocas); as sete chaves, os encaixes e o protocolo de níveis por trás dos locais; o túmulo da bolsa num mundo sem praça |

Um mundo com um jeito de jogar próprio fica em `games/<nome>/`, com `mods/` e `textures/` para
passar ao servidor (`worlds/` continua ignorada: é onde um jogo deixa os seus saves). `games/yafps`:
a arena FPS do fork anterior, reescrita -- `yafps_core` (blocos, a arena como etapa, movimento) e
`yafps_game` (`side = both`: alvos, o golpe decidido pelo servidor, HUD). `games/track`: a pista de
kart cronometrada. `games/inferno`: cavernas de escória, lava, fortalezas, brutos, espectros e
cinzas. `tools/track.sh` serve todos eles de um servidor só, como salas do sandbox.
