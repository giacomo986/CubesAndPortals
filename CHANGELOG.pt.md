# Mudanças

*[Read in English](CHANGELOG.md) · [Leggi in italiano](CHANGELOG.it.md)*

Uma entrada para cada alfa distribuída, a mais nova no topo. Como elas são numeradas e como se faz
uma alfa: [docs/BUILDING.pt.md](docs/BUILDING.pt.md#versões).

## Próxima alfa (ainda sem tag)

- **Corrigido**: blocos que um mod coloca de novo (o túnel da fuga do assalto depois de uma nova
  tentativa) podiam ficar desenhados quebrados num cliente enquanto o servidor os tinha inteiros:
  uma mudança de bloco ultrapassava o chunk em que estava.
- **Corrigido**: sair de um mundo enquanto o fantasma amaldiçoado te segurava te deixava, ao
  voltar, andando por ele sem conseguir se soltar.

## 0.21.0-alpha.3 — 2026-11-24

- **Tower defense**: um mundo novo atrás de um portão na praça, sozinho ou junto. Monstros dos
  outros mundos saem de uma fenda e andam até um cristal; cinco torres construídas numa bancada com
  o que você minerou (flechas, braseiro, gelo, raio, portal), melhoráveis até o nível 3,
  barricadas que os monstros contornam, e um chefe a cada quinta onda.
  As torres se viram para os alvos e o que atiram se vê: flechas, fogo, gelo, raios, portais.
  Cada uma mira no monstro mais perto, mais longe, mais adiantado, mais atrasado, mais forte ou
  mais fraco, como se escolhe no seu painel. Cada onda segurada dá
  materiais para guardar; `/defense` mostra os recordes.
- **O mundo das nuvens**: um mundo novo atrás de um portão na praça: ilhas sem fim sobre um mar
  de nuvens, pontes, cristais do céu, **homens-coruja** em aldeias (andam, planam entre ilhas,
  trocam e indicam o caminho) e **correntes ascendentes** que vagam devagar e te levam de volta
  para cima depois de uma queda. No fim da estrada do céu a **Arraia das Nuvens**, um chefe para um
  ou vários, com a vida na tela: suas rajadas se veem voando, para-raios a derrubam, homens-coruja
  vêm ajudar, luta-se com ela na ilha central, depois as pontes caem e ela é finalizada planando.
  Dá a **asa-delta da tempestade**, que os ventos da tempestade continuam levantando, e funciona
  em todo mundo.
- **Motor**: `core.cloud_sea(base, top)`, as nuvens do céu desenhadas uma segunda vez mais abaixo,
  para um mundo de ilhas.
- **Backrooms**: dois níveis novos, por distrito no lugar dos antigos: o hotel (carpete
  vermelho, corredores de portas fechadas) e a festa (paredes rosa, confete, balões, bolos). E as
  **backrooms profundas**, um mundo à parte atrás de uma porta rosa perto da entrada: mais níveis
  novos, e portais que a cada dez minutos levam a outro lugar.
- **Troféus**: vinte, pelos mundos aonde você vai, as sete chaves e cinco portais escondidos pela
  terra. As taças ficam numa estante no seu quarto; `/page trophies` lista todos, com uma dica e
  para que lado do spawn fica cada portal escondido.
- **Cinco portais escondidos** no sandbox, onde o terreno da semente tem lugar: um velho poço com
  uma corda, um arco no ponto mais alto que só abre no inverno, uma moldura no fundo de um lago profundo, uma porta numa
  encosta e um círculo de pedras que só abre à noite. Cada um leva a uma ilha no céu ou a uma
  caverna de cristais, com um diamante e cinco rubis para quem chegar primeiro.
- Passar pelo portal do jogo de plataformas ou do jogo de tiros de arcada conta como ir àquele
  mundo.
- **Fantasmas próprios**: a melhor volta de cada jogador numa pista fica guardada também como
  fantasma. `/ghost mine` corre contra a sua, `/ghost NOME` contra a de um amigo, `/ghost record`
  contra o recorde da pista, `/ghost off` contra nenhum; `/laps` lista a melhor volta de todos na
  pista, não só as cinco do quadro.
- **Várias pistas de peças num mundo**: cada linha de largada posta é uma pista própria, com um
  dono; só o dono tira as linhas dela, e um jogador pode ter oito. Antes, uma linha de largada nova
  tirava a pista anterior, fosse de quem fosse.
- **Minimapa**: mapa, bússola e um lingote de ouro; em qualquer lugar da bolsa, mantém no canto
  superior direito um pequeno mapa da terra em volta, como está construída, com portais e vilas.
  Duas melhorias: o minimapa buscador (quatro lápis-lazúlis) marca portais escondidos a até 128
  blocos, o minimapa profundo (um diamante e dois rubis) mostra diamante, rubi e ouro no subsolo.
- Para quem escreve mods: `core.map_cell(x, z)` na máquina do jogador, o topo de uma coluna e a
  sua cor.
- **Portais de atalho na pista**: `/pieces` dá uma ferramenta de portal; dois cliques numa estrada
  põem um par de portais de través, e um kart que entra no primeiro sai do segundo.
- Para admins: `/trophies sites`, `/trophies go ID [place] [NOME]`, `/trophies home [NOME]`.
- Para admins: o histórico dos blocos mudados (`/inspect`, `/rollback`) só é guardado com
  `--block-history`; sem ela, nada é anotado.
- Para admins: `--map-tiles PASTA` desenha um arquivo de mundo visto de cima, em tiles PNG e um
  `index.html` que mostra as coordenadas do bloco sob o ponteiro. Com o servidor desligado, ou numa
  cópia feita com `/backup`.

## 0.21.0-alpha.2 — 2026-11-22

Joga com a 0.21.0-alpha.1 (protocolo 131) e mantém os mundos dela.

- **Tamanho da interface** em Opções > Vídeo: auto, pequena, normal ou grande. A barra, a
  mochila, a vida, o chat, os avisos e todos os painéis crescem ou diminuem; auto escolhe pelo
  tamanho real da tela.
- **Os painéis cabem na tela**: um alto demais (o do robô) é desenhado menor em vez de sair dela.
- **ESC fecha** os painéis do robô, da oficina, do portal e do território; o da mesa de
  encantamento não quebra mais com ele.
- **Os primeiros objetivos dão para fazer**: Madeira conta o tronco de qualquer árvore, qualquer
  tronco dá tábuas, e as tábuas de cada madeira viram tábuas comuns, então a picareta de madeira,
  o barco e a cama saem da primeira árvore. As dicas dizem as receitas certas, e **J** abre a
  lista de todos os objetivos.
- Para quem escreve mods: `core.hud.scale()`, o fator do tamanho da interface.

## 0.21.0-alpha.1 — 2026-11-22

**Não joga com a 0.20.0-alpha.1** (protocolo 131): a cor da luz através de um bloco, as
palavras que o Tab completa no chat, as abas de um baú e as roupas de uma criatura são novas na
rede.

- **O vitral colore a luz das lâmpadas que passa por ele**: uma tocha ou uma lâmpada atrás de um
  vidro vermelho ilumina o chão de vermelho. A luz do dia continua passando branca.
- **Escrevendo há um cursor**: o chat, os campos dos painéis dos mods, os da tela inicial e a busca
  do baú se movem com Esquerda, Direita, Home, End e Delete. No chat, o **Tab completa** um comando
  depois de uma `/`, ou o nome de um jogador; Tab de novo passa pelos outros que servem.
- **As palavras das placas somem aos poucos com a distância** em vez de sumir de repente.
- **A primeira hora**: os objetivos agora servem de guia -- um tronco, uma picareta de madeira,
  atravessar um portão e voltar, uma chave, uma cama, depois o resto -- e o próximo aparece
  pequeno à esquerda com a sua dica (`/goals hide`, `/goals show`).
- **As tarefas aparecem no mapa**: o caminho de uma entrega até a outra aldeia, o lugar de uma
  exploração e o caminho de volta ao prefeito.
- **Ossos e carne servem para algo**: dois ossos fazem seis farinhas de osso, que fazem uma
  plantação subir um estágio na hora; o fazendeiro compra farinha de osso, o pescador compra carne
  para isca.
- **Circuitos**: fios, fios isolados e cabos vão também em paredes e tetos, e dobram esquinas; as
  **portas de barramento** (AND, OR, XOR, NOT) trabalham nos dezesseis canais de um cabo; a
  **bancada de chips** guarda um circuito montado, com os seus pinos de entrada e saída, como um
  chip que o faz funcionar onde for colocado.
- **Duas novas câmaras de testes**: a 16, uma **ponte de luz** estendida através dos portais sobre
  um poço; a 17, **géis** derramados através dos portais -- o azul te lança por cima de um muro, o
  laranja te faz correr por cima de um vão. As câmaras de um sítio da sobrevivência agora são uma
  fileira só dele, escolhida pela sua dificuldade e em parte espelhada, que fica depois de
  reiniciar.
- **As cartas como itens**: quem ganha uma mão numa mesa de cartas fica com uma das suas cartas.
  Cada uma das quarenta tem a sua figura, pode ser segurada e posta no chão, e o museu da aldeia
  quer todas para os seus pedestais (o sete de ouros paga mais). Um lugar atrás de uma parede não é
  mais oferecido, e o leque de cartas fica acima dos punhos.
- **Os sons em loop são ouvidos através de portais e janelas**, dando a volta, como os outros
  sons; um mod que quebra leva os seus loops com ele. **Novos loops**: a lava que ronca, o motor de
  um kart dirigido (mais alto quanto mais rápido), o queimador do balão e uma música calma conforme
  a terra (planície, deserto, frio, mar; `--no-mod music` para ficar sem).
- **De onde vem um golpe**: ao ser ferido, a borda da tela fica vermelha por um instante, mais do
  lado de onde veio o golpe ou a flecha, e em volta toda, mais fraca, numa queda ou queimadura.
- **O fantasma amaldiçoado** (`mods/ghost`): uma nova criatura do escuro, à noite e nas cavernas,
  rara. De longe só um tremeluzir; se te alcança, se apossa de você, te leva ao precipício mais
  próximo ou a outros monstros e joga no chão as coisas da sua bolsa, até cinco golpes, seus ou de outros
  monstros, o quebrarem. Flutua, estica as garras, agarra e morre num esqueleto só seu.
- **Karts**: uma caixa de itens dá a quem lidera o que defende a vantagem e ao último o que o faz
  alcançar, e o HUD mostra a sua posição. Um **fantasma** dirige ao seu lado a volta recorde da
  pista (na pista do Inferno, a do engenheiro). Os karts **se batem** e se afastam.
- Para quem escreve mods: `core.node_glass(id, opacity, tint)` deixa passar a luz na cor do bloco;
  `core.player_launch(..., add)` soma à velocidade em vez de defini-la, e move também um kart;
  `core:hurt {player, damage}` diz que o golpe de uma criatura ou uma flecha atingiu
  um jogador; `core.render.model(..., alpha, tint, clip, time)` desenha um modelo transparente, tingido e posado
  por um clip dele; `maps:route`
  aceita `~R;` antes dos pontos para um fim alcançado dentro de R.
- **O prop hunt foi removido** do gongo da arena, que agora só inicia a luta.
- **Os karts são monopostos**, e os power-ups valem só na pista onde são usados: o raio, a
  tinta, o alvo de um casco teleguiado e o toque de uma estrela alcançam só quem corre naquela
  pista.
- **Arpg**: figuras pintadas no inventário em cinco raridades; um cinto de três poções ao lado dos
  globos; os mercadores recompram as últimas doze coisas vendidas; oito materiais de criaturas,
  objetos e baús; catorze missões em quatro cadeias, com uma marca sobre quem as dá; o Açougueiro e
  o Guardião refeitos; armas, escudos e peças de armadura novas para cada espécie; os baús e o
  depósito da cidade agora são modelos; a saída da cripta é um corredor selado com uma sala só
  dela, e as suas escadas têm meios degraus.
  Um **ajudante** para os guerreiros, contratado com o guardião do portão: segue você, luta com a
  arma que recebe, veste oito peças de equipamento, bebe as suas poções de vida e o guardião o
  levanta quando cai. **Armadilhas** escondidas na maioria das áreas depois da primeira (fogo,
  choque, espinhos, gelo, gás, ou uma horda). A criatura sob o ponteiro tem um **contorno**, a que
  você está enfrentando em dourado; a contagem de uma missão de abate aparece a cada morte e as
  suas criaturas voltam se sobrarem poucas; o covil do Açougueiro se fecha com uma parede de luz
  vermelha enquanto ele vive; quem cai nos andares de baixo da cripta volta ao topo da escada.
- **O catálogo criativo tem abas** por tipo de item, e uma caixa de busca.
- **Correções**: um meteorito não cai mais no chão de uma caverna ao lado de um jogador; uma
  tarefa se pega no quadro da aldeia; um caminho que termina numa ponte tem a sua placa; as
  formigas do jardim andam lado a lado e nenhum inseto sai pela portinha; a pedreira de um robô
  ajudante desce em espiral além das 24 camadas (até 96) do lado da base livre de construções dos
  jogadores; o F8 não mostra mais o fundo dos lagos; uma fornalha antiga arruma o que estava nas
  fileiras erradas; as portas abrem com o fio isolado aceso e com uma porta lógica acesa voltada
  para elas; um mundo sem arquivo não faz mais crescer sem fim o seu arquivo temporário.
- Para quem escreve mods: `core.is_liquid(id)`; `core.keep_loaded` mantém no máximo 256 colunas
  para todos os mods de um servidor juntos; `core.mob_wear(id, slot, model, tint)` veste uma
  criatura; `core.container_tabs` dá abas a um contêiner; `core.item_kind(name)`;
  `core.outline(id, r, g, b, px, a)` desenha um contorno em volta de uma criatura e
  `core.cursor_creature()` diz qual está sob o ponteiro.
- **Bibliotecas atualizadas**: raylib 6.2, Luau 0.748, Flecs 4.2.0, Jolt 5.7.0, SQLite 3.54.1.

## 0.20.0-alpha.1 — 2026-11-20

**Não joga com a 0.19.0-alpha.1** (protocolo 127): numa sala de um servidor com várias, depois da
primeira, só se entra com um tíquete assinado para aquela sala, que um cliente mais antigo não
pede.

- **Acabaram os clarões verdes no céu do front**: a ponta distante de uma janela (o mapa da galeria
  sobre o campo) não é desenhada, e a sua abertura não se repete mais -- no céu,
  a cada mudança nos portais do mundo -- nem a da própria janela, no piso da galeria, sem parar.
- **Nas salas só se entra pelos seus portais**: entrando direto, uma sala depois da primeira do
  servidor só pode ser olhada; um tíquete para uma sala não deixa mais entrar em outra do mesmo
  servidor. `cnp_join_probe --room NOME` entra direto numa sala, para testes.
- **Logo depois de entrar**, as criaturas, os trilhos e as coisas na mão perto de você são lidos
  antes do resto dos modelos do mundo, e um trilho ou outro bloco com modelo ainda não lido é
  desenhado como as suas caixas em vez de não aparecer.
- **Uma sala que começa com o servidor rodando** não para mais as outras: elas ficavam paradas até
  três segundos enquanto uma sala do inferno carregava, agora no máximo cerca de um quarto de
  segundo.
- **Os portões de um mundo de sobrevivência** liberam os portais de que ninguém está perto, para que
  ali também os portais entre salas construídos à mão tenham bocas para pegar emprestadas.
- **O golpe do prop hunt sempre acha quem se esconde**: não achava ninguém quando o corpo de quem
  se escondia tinha um id já usado (alguém que entrou depois que outro saiu). Para quem escreve
  mods: o `target` de `core:hit` é o id que todas as outras chamadas recebem, como
  `core.spawn_mob` o dá, e não mais um número que `core.body_player` não sabia ler.
- **Quem se esconde no prop hunt é atingido onde está o seu bloco**: acabaram os golpes que acertam o
  ar vazio acima dele ou erram a sua borda de baixo, e quem está disfarçado de laje, escada ou cerca
  é desenhado com essa forma, não como um cubo inteiro.
- **As pilhas de itens no chão são salvas com o mundo**: depois de reiniciar elas estão onde
  estavam, inclusive as gastas, encantadas e coloridas, com o resto dos seus cinco minutos pela
  frente.
- **Um kart estacionado mantém a pintura e as peças do seu último piloto** para todos, também depois
  de reiniciar, e ocupa o seu tamanho real, não o de um bloco só.
- **Xadrez**: um adversário que sai no meio (fecha o jogo, se afasta, passa por um portal) é esperado
  por 60 segundos; depois a partida é de quem ficou, e o tabuleiro fica livre para outra.
- **Os sons em loop giram sem buracos**: chuva, motores e ambientes não param mais por um frame a
  cada volta (mais de um quinto de segundo numa máquina lenta).
- **Um jogador que o código de um mundo manda para outra sala** (`core.room_send`) não faz mais o
  cliente travar ao chegar.
- Para quem escreve mods: `core.cell_look(x, y, z, tint, pieces)`, no cliente, veste o modelo com
  que o bloco de uma célula é desenhado, como `core.ui_model` faz com uma prévia.
- **A documentação também está em português**: `docs/`, os guias dos mundos, `contrib/` e este
  changelog, ao lado do inglês e do italiano; `docs/API.md`, gerado, está em inglês.
- **Para testes**: o cliente cronometrado sabe dar clique esquerdo (`--click-at x,y,z[,s]`,
  `--left-at t[,s]`) e gravar o que toca (`--record-audio FILE.wav`, numa saída que ninguém ouve
  com `tools/null_sink.sh`); `tools/audio_check.py` diz quando houve som e com que volume.
  `--left-at t,s,x,y` põe o ponteiro ali durante o clique (frações da tela: de cima, como no arpg,
  o clique vai aonde ele está), e `--left-at` pode ser dado mais de uma vez; uma caminhada guiada
  pelo código do mundo (`core.walk`, o botão segurado do arpg) agora anda também numa corrida
  cronometrada.
- **O front e o assalto dividem armas, granadas, bots e máquinas**: um só arquivo que os dois
  carregam (`battle.luau`, o do front_game; o do assalto é um link para ele), então uma correção
  num é uma correção no outro. Duas correções que só o assalto tinha chegam ao front: um tanque
  preso na borda de uma cratera é erguido para fora depois de um momento, e os soldados numa
  máquina se movem com ela no mesmo tick em vez de um tick atrasados. Um arquivo de um mod agora
  pode ser um link para o de outro mod: ele é empacotado, e enviado aos jogadores, com o nome que
  tem no mod.
- **A oficina**: `/workshop` leva um jogador para uma sala plana só dele, onde os pedaços de níveis
  (zonas) são feitos sem coordenadas: uma varinha escolhe o volume por dois cantos, desenhado na
  tela, e um painel dá um nome a ele, diz para que serve, salva, monta um nível com ele para testar
  (e volta) e o propõe. Lá os blocos colocados não se gastam e os blocos cavados não dão nada; a
  bolsa é a mesma em todas as salas, então não há catálogo. Um servidor precisa do modelo
  `[room workshop-*]` no seu arquivo de salas; os mundos da tela de título e `tools/track.sh` o
  escrevem. O menu de qualquer moldura de portal também tem "minha oficina".
- **O dono pode entrar na sala ou na oficina de um jogador** pelo nome: o servidor lia o nome do
  jogador depois de perguntar se ele podia entrar, então `--owner` era recusado como um estranho.
- `--no-plaza`: uma sala sem praça e sem mais nada da sobrevivência (as oficinas usam isso).
- **Câmaras de testes feitas nas oficinas**: a oficina tem os blocos das câmaras, colocados com um
  pincel a partir de uma paleta (sem bolsa: `/workshop blocks`), e um novo, o fundo de um poço. Um
  nível de câmaras montado com zonas funciona por regra -- cada botão, botão de apertar, alvo e
  torreta controla a porta mais perto dele, as saídas de cubos soltam cubos, as plataformas lançam
  no alvo mais perto, um pedestal dá a arma. As quinze câmaras continuam; o nível de um local de
  sobrevivência é feito com as zonas aprovadas quando há alguma, e a última sala tem um botão para
  mais câmaras, mais difíceis a cada uma vencida.
- Para mods: `zones:watch` / `zones:block` dizem a um mundo onde os seus próprios blocos foram
  colocados.
- **O arpg feito nas oficinas**: a oficina também tem os blocos do arpg, e as terras de um local de
  sobrevivência são montadas com as zonas de arpg aprovadas quando há alguma -- bandos onde estão
  os marcadores `mob`, o senhor no `exit`, baús em `loot`, coisas para quebrar em `trigger`.
  `/zone try` agora monta um nível com o piso na altura do jogador.
- **Os backrooms feitos nas oficinas**: os seus blocos na oficina, e o nível de um local de
  sobrevivência novo montado com as zonas de backrooms aprovadas -- entra-se pelo `start`, o rádio
  leva ao `exit`.
- **O parkour feito nas oficinas**: os seus blocos na oficina, e o percurso de um local de
  sobrevivência novo montado com as zonas de parkour aprovadas -- as suas plataformas são os
  lugares para onde uma queda leva de volta, o `exit` é o topo.
- **O Inferno feito nas oficinas**: os seus blocos na oficina, e a fortaleza de um local de
  sobrevivência novo montada com as zonas de inferno aprovadas -- os braseiros para acender, o
  portão, o guardião no `exit`.
- **O front e o assalto feitos nas oficinas**: o campo do front entre as bases é montado de novo a
  cada rodada com as zonas de front aprovadas (os seus marcadores `key` são os pontos), e os vales
  do assalto antes das suas três primeiras fortalezas com as zonas de assalto aprovadas. Para mods:
  `zones:scatter` coloca zonas lado a lado sobre um campo aplainado.
- **Zonas propostas**: `/zone propose` pede aos admins, que são avisados, e `/page zones` lista o
  que espera, com mostrar, aprovar e recusar; `/zone show` monta uma zona sozinha para olhar.
- Para mods: `core.room_send` inicia uma sala do arquivo de salas que está parada (ou uma que o seu
  modelo cria) em vez de responder 0; `zones:do` executa uma linha de `/zone` como se um jogador a
  tivesse dito.
- **O handler de um mod que emite um evento próprio** e depois muda um campo de texto do evento que
  está tratando não derruba mais o servidor nem devolve texto de memória já liberada (isso parava o
  servidor do front num build de debug).
- **`cnp_netshape` no Windows**: o relay que deixa uma conexão lenta e com perdas compila e roda lá
  também; o build do Windows parava nele.
- **Para desenvolvedores**: uns trinta avisos do compilador a mais no código do próprio motor, um
  build de debug que confere cada índice de contêiner, e um servidor que libera a memória dos seus
  mods quando para -- cada jogo deixava dezenas de megabytes para o LeakSanitizer. Como compilar com
  AddressSanitizer: [docs/BUILDING.pt.md](docs/BUILDING.pt.md#warnings-e-sanitizers).

## 0.19.0-alpha.1 — 2026-11-19

**Não joga com servidores 0.18** (protocolo 126): o código de um mundo pode estar em `api = 2`, que
um cliente 0.18 não sabe executar. Os mundos são lidos como estão.

**API de mods 2.** Uma chamada que responde sim ou não (`core.loaded`, `core.node_solid`,
`core.has_perm` e outras 139) dá `true` ou `false` a um mod que diz `api = 2`; um que diz
`api = 1`, ou nada, continua recebendo 1 ou 0. Todos os mods do jogo estão na 2. Como passar um:
[docs/MODDING.pt.md](docs/MODDING.pt.md#api-ou-seja-para-qual-motor-o-mod-foi-escrito).

- **Um mod desligado por um erro é avisado no chat** aos admins daquele mundo (a permissão `logs`),
  com o erro, em vez de só no log do servidor.
- **Os backrooms**: entrar num nível não levava a lugar nenhum -- o handler do nível parava numa
  variável usada acima de onde é declarada. Agora leva, e do nível 5 em diante as luzes voltam a se
  apagar como deviam.
- **O Inferno tem de novo os seus robôs ajudantes**: desde que vieram as plantas (0.10) o mod dos
  ajudantes precisava delas, e o mundo do inferno não as tinha, então deixava os ajudantes de fora
  sem dizer nada.
- **Barcos, carrinhos, comércio e encomendas** não param mais num jogador que saiu no mesmo
  instante: o que teria caído aos pés dele não cai, em vez de o handler inteiro falhar.
- **Os pontos de captura do front**: as bandeiras e o mastro eram colocados uma vez, no começo,
  mesmo em terreno ainda não carregado, e nunca mais -- `core.loaded` responde 0 ou 1, e 0 é
  verdadeiro em Luau. Agora eles esperam o terreno.
- **O front e o assalto** ignoram uma mensagem de mira de um cliente que não seja de seis números;
  bastava uma para parar o jogo para todos.
- **O ensopado de cogumelos** leva os dois cogumelos marrons e o vermelho que sempre listou; estava
  sendo feito com um só cogumelo marrom.
- **Para quem faz mods**: `docs/api.d.luau` carrega de novo num editor (uma descrição tinha escapado
  do seu comentário) e agora declara `core.ecs`, `craft`, `net`, `render`, `hud`, `register_abm` e
  `register_lbm`, o `MapgenStage` inteiro e os payloads dos eventos com tipos. Todo mod do jogo
  passa limpo na checagem de tipos -- todos em `--!strict` agora, os jogos grandes também -- e
  `tools/typecheck.sh` confere os seus do mesmo jeito:
  [docs/MODDING.pt.md](docs/MODDING.pt.md#verificar-os-tipos-toolstypechecksh).
- **Para quem faz mods**: `require("@self/x")`, o "dentro de mim" do Luau -- de um `init.luau` ao
  lado, de `a.luau` em `a/`. Escreva assim num `init.luau` em vez de `./x`, que ainda carrega mas
  que um editor agora lê como a pasta de cima.
- **Dois testes novos**: `mods_smoke` inicia o sandbox e cada jogo e falha num mod que não carregou
  ou foi desligado; `api_types` falha quando uma parte de `core` escrita à mão não tem tipo. Um
  terceiro, `mods_play`, manda para cada mundo um jogador sem janela que conversa, quebra blocos e
  anda; um quarto, `mod_notice`, confere que um mod desligado é avisado ao dono e a mais ninguém.
- **Para quem faz mods**: `.luaurc` e `.vscode/settings.json` configuram para o repositório um
  editor com o Luau Language Server -- strict, e `core` com tipos de `docs/api.d.luau`.
- **Cada push é conferido** (`.github/workflows/check.yml`): o build com avisos como erros, os
  testes que não precisam de tela, os tipos de cada mod e as traduções.
- **Salas de demonstração no laboratório de portais**, ideias do Portal Explorer do optozorax feitas
  com pares simples: uma sala que leva a ela mesma (`/lab room`), três e cinco salas em volta de uma
  esquina (`loop3`, `loop5`), uma cabine maior por dentro (`booth`).
- **Uma porta vista através de um portal mostra o que há depois dela**, não o céu atrás da sala: as
  aberturas desenhadas a cada quadro agora são as que cobrem mais tela, contando as vistas através
  de outra, em vez das mais próximas à vista direta (`/lab loop3x`).

## 0.18.0-alpha.1 — 2026-11-18

**Não joga com servidores 0.17** (protocolo 125). Os mundos são lidos como estão; um mundo do jogo
de plataforma já salvo fica com os blocos antigos até ser iniciado de novo com `--fresh`.

- **Tubos de itens** (mod `pipes`): redes que separam, guardam e buscam, configuradas com chips
  (extrator, receptor, transbordo, terminador, anunciador, estoque...). Um tubo de pedidos abre um
  painel com uma busca para pedir à rede qualquer coisa que ela tenha ou saiba fazer, e uma bancada
  de trabalho e uma fornalha automáticas fabricam e fundem em cadeia. Três demonstrações funcionando
  a leste do spawn; o guia é [docs/PIPES.pt.md](docs/PIPES.pt.md).
- **As placas** têm o texto escrito nas suas tábuas no mundo, em pé ou penduradas, em qualquer uma
  de oito direções.
- **A sua sala**: `/myroom` escolhe quem pode entrar (uma lista negra ou uma lista branca) e se os
  outros podem espiar pela moldura; `/myroom leave` leva você de volta ao spawn.
- **O jogo de plataforma**: um nível de castelo com lava e um chefe, um mastro para escorregar até
  o castelo, tijolos que quebram, o power-up de fogo com uma aura, um power-up para cada jogador.
- **O shooter de fliperama**: um hangar e uma decolagem, oito power-ups, uma fortaleza voadora, um
  segundo nível de dez ondas e um porta-aviões no fim; vários pilotos podem jogar juntos.
- **Armas em primeira pessoa** para todas as espécies, e **o homem-raposa**, uma nova espécie
  adicional.
- **Português**: os tubos e `/myroom` traduzidos também.

## 0.17.0-alpha.1 — 2026-11-14

**Não joga com servidores 0.16** (protocolo 124). Os mundos são lidos como estão; uma bolsa salva
antes ganha uma mão esquerda vazia.

- **Tochas** têm um modelo de verdade na mão e vão no chão, nas paredes e no teto. Uma tocha, ou
  qualquer bloco que dá luz, segurada em qualquer uma das mãos ilumina em volta como faria colocada
  -- dobrando esquinas, não atravessando paredes -- e todo mundo vê.
- **A mão esquerda**: uma segunda coisa para segurar, trocada com H, na figura da bolsa e ao lado da
  hotbar, vista por todos.
- **Opções > Controles**: toda tecla menos ESC pode ser mudada, para outra tecla ou para os botões
  do meio e laterais do mouse. Mods podem acrescentar teclas próprias ali (`core.register_key`).
- **Português**: o jogo e um README em português do Brasil.
- **Dois jogos novos** atrás de portões na praça: um jogo de plataforma visto de lado, com
  power-ups, canos e uma sala bônus, e um shooter de fliperama pilotado visto de cima.
- **Salas**: portais entre salas de um servidor construídos pelo menu de uma moldura, salas
  iniciadas e paradas a partir de um arquivo, uma sala só sua para cada jogador, dificuldade e
  regras em cada sala.
- **Molduras de portal** têm uma cor, podem mudar a escala ou ser uma janela, e pertencem a quem as
  construiu.
- **Zonas**: pedaços de um nível salvos do mundo, e níveis montados com eles.
- **O homem-lagarto**, uma espécie adicional, e espécies adicionais guardadas pelo nome em todo
  servidor.
- A medalha balança presa no seu alfinete; o jetpack voa no tamanho do corpo; dirigindo em primeira
  pessoa só os braços de quem dirige são desenhados.

## 0.16.0-alpha.1 — 2026-11-10

**Não joga com servidores 0.15** (protocolo 123). Os mundos são lidos como estão; lâmpadas em
colunas salvas antes desta versão brilham na sua cor quando são acesas de novo.

- **Cidades e rodovias**: os mundos novos têm cidades ligadas por rodovias, povoados em volta delas
  e vilas espalhadas pela natureza, com uma placa de direção onde dois caminhos se cruzam. Cada
  mundo escolhe a sua profundidade e a sua altura.
- **Luz colorida**: cada lâmpada ilumina na sua cor.
- **Circuitos** (mod `circuits`): fio de liga que leva energia sem enfraquecer, fio isolado em
  dezesseis cores, cabos de dezesseis sinais, elevadores, dezesseis portas lógicas e lâmpadas
  coloridas, tudo funcionando com cinábrio.
- **O corredor** (`mobs:runner`): um monstro noturno raro que dispara, escala paredes e pula em
  você de 7 blocos de distância. Os zumbis também escalam, até 6 blocos, e os que escalam se agarram
  numa borda quando caem e sobem.
- Monstros mortos se desfazem em vermelho. O arco mira na mira em primeira pessoa.
- **Humano, toupeira e sapo refeitos**, com chapéus, medalha e os outros acessórios feitos sob
  medida para cada corpo; um cubo com dois portais substitui a estrela no boné, na cartola e na
  medalha.
- **A resolução** funciona em todo modo de exibição, tela cheia incluída, sem redimensionar a
  janela: o jogo é escalado para preenchê-la.
- As criaturas do jardim falam muito menos. Uma fornalha termina as fundições que venceram enquanto
  você estava fora. Uma janela flutuante mantém o seu tamanho.
- Para admins: `/mute` também impede comerciar, entrar numa partida, acrescentar donos a um
  território e a caixa de cartas. De duas cópias de uma bolsa fica a que mais se moveu, a outra vai
  para `/oldbags`.

## 0.15.0-alpha.2 — 2026-11-08

- **Opções de vídeo** (Opções, depois Vídeo): janela, sem bordas ou tela cheia, resolução, VSync,
  limite de FPS, distância de visão, campo de visão, sombras, luz, detalhe dos portais, brilho para
  o escuro, e um céu de tempestade simples: sem as nuvens em redemoinho do mundo final, para as
  máquinas que não dão conta delas.
- **Uma joaninha para montar**: sele uma no jardim e ela te carrega como um pônei, com o seu
  próprio andar e a sua corrida. Montada para fora pela portinha ela cresce até o seu tamanho,
  domada: espera selada onde você desce e continua lá depois de reiniciar.
- O kart do final: F sai dele.
- **Todo item tem a sua figura e algo para segurar**: as armas do front, do assalto e da arena têm
  ícones em vez do nome no espaço; armaduras, tinturas, o fragmento e o resto são segurados como
  eles mesmos em vez de um cubo.
- Para admins: `/story` mostra ou define as chaves e o final de um jogador; `/time` agora funciona
  em todo mundo, não só no sandbox.

## 0.15.0-alpha.1 — 2026-11-08

**Não joga com servidores 0.14** (protocolo 120). Os mundos são lidos como estão, mas um mundo do
front já salvo mantém as paredes antigas em volta das portas novas: apague `front.world` para que
sejam construídas de novo.

- **O front**: o lança-foguetes tem uma mira de vidro e, mantido sobre um veículo inimigo por 1,2 s,
  trava (LOCKING, LOCKED) e o seu foguete persegue o veículo. As portas para as bases ficam
  embutidas na parede, 2 por 3, e se vê através delas direito. Sob o piso de vidro da galeria fica o
  campo inteiro em miniatura, em 1:32, com a batalha em cima; ninguém no campo consegue vê-lo. Cada
  base tem uma pista de pouso.
- **Portais que mudam a escala**: uma abertura do tamanho de uma porta para uma minúscula encolhe
  quem passa, criaturas e caixas que rolam também; e **aberturas que se movem**, carregadas ou
  giradas livremente para qualquer lado, atravessadas de acordo com o modo como se movem. Para mods:
  `core.portal_scale`, `core.body_scale`, `core.body_size`, `core.portal_move`, e
  `core.portal_window` para uma vista de mão única que nada atravessa.
- **Um jardim visto enorme**, por uma portinha perto do spawn: formigas em fila, joaninhas e
  borboletas no seu tamanho real, com as suas vozes.
- **O kart**: uma variante dragão para cada um dos cinco espaços da garagem, uma garagem que vale
  em todas as salas e montada também no Inferno, caixas de item no Inferno e, depois do final, o
  kart anda em qualquer lugar do sandbox. Uma câmera que sai suave dos loops.
- **Andaimes** de postes e tábuas: montados e desmontados por robôs, escalados pelos jogadores.
- **Uma quarta espécie**, o cavaleiro, nos dados mas desligada por enquanto.
- **Sombras em corpos e modelos**, e nuvens que ficam paradas quando você está pequeno.
- **Coisas menores**: as poções duram depois de sair e de reiniciar; no arpg os botões segurados
  repetem a habilidade, e a cripta tem os seus sons; power-ups e a tocha têm sons; trilhos
  colocados por um mod tomam a sua forma; uma peça de pista recolhida devolve o terreno; eclipses
  desenhados em toda máquina; o baralho ilustrado está completo; `/mute` não se engana com
  maiúsculas, pontuação e aspas; traduções alinhadas.

## 0.14.0-alpha.1 — 2026-11-05

- **Abelhas**: ninhos selvagens entre as flores dos prados e colmeias de tábuas se enchem de mel (um
  frasco pega uma garrafa, comida) e de favos, ajudam os campos em volta a crescer e picam quem os
  rouba sem fumaça. Abelhas criadas: um fumigador as acalma (uma baforada de fumaça que todos veem),
  uma rede pega a rainha de um ninho selvagem, e um apiário a recebe com quadros para encher mais
  rápido, um forro de lã para o inverno e um infusor que faz mel luminoso, de brasa ou de cristal
  com o que há em volta; uma prensa o transforma em cera, lâmpadas de mel, cera de brasa para a
  fornalha, ametista e de vez em quando um diamante.
- **O céu**: chuvas de meteoros à noite, que de vez em quando deixam um meteorito numa cratera; a
  aurora sobre o frio no inverno; um eclipse raro que solta de dia as criaturas da noite (`/sky`).
- **Voar**: uma asa-delta (segurada, ela abre quando você sai de uma altura; você fica pendurado de
  bruços na barra) e um balão de ar quente (pule para subir com o queimador roncando, agache para
  descer, até acima das nuvens). Montar num pônei enquanto voa com o jetpack não faz mais o pônei
  voar.
- **Câmeras e telas**: uma câmera posta no chão, numa parede ou no teto aparece ao vivo em qualquer
  tela ligada a ela; clique direito na tela para pegar os controles e girar a câmera com W A S D, a
  cabeça dela girando para todos. Uma câmera que vê alguém energiza o cinábrio ao lado.
- **Uma câmera que tira fotos**: segure o botão direito para olhar pelo visor, clique esquerdo para
  fotografar; a foto é um instantâneo que todos veem como foi tirado, guardado num álbum (`/album`)
  e pendurado em molduras.
- **O museu da vila**: o curador fica com o primeiro de cada peixe, minério, gema e achado, e com
  fotos de lugares raros, e paga em rubis; o quadro da vila guarda um jornal do que os jogadores
  fizeram ali.
- **Um passaporte** com carimbos dos servidores amigos, e cartas e encomendas mandadas entre eles.
- **Depois do final**: quem derrotou l'Ospite encontra as ferramentas dos mundos no sandbox -- uma
  arma de corda, uma arma de portais e um kart -- numa versão menor.
- Uma casa maior por dentro (portas de bolso para salões próprios), um esconde-esconde de objetos
  em volta do gongo da arena, pistões desenhados como pistões e alcançando de duas a cinco células,
  pó de cinábrio desenhado ligado do jeito que conduz, chuva que se ouve.
- A comida também se come com um clique direito no ar.
- Para mods: `require` (um mod em vários arquivos), `core.sound_loop`, `core.body_block`,
  `core.body_glide`, `core.body_balloon`, `core.body_rope`, `core.render.camera`/`screen`/
  `camera_read`/`photo`, `core.render.fx` "streak", "aurora" e "smoke", `core.sky_eclipse`,
  `core.fed_send` com `core:arrived` e `core:fed_message`.
- **Protocolo 116**: não joga com servidores nem clientes 0.13.

## 0.13.0-alpha.1 — 2026-11-04

- Montar um pônei agora é cavalgar, não andar fantasiado de pônei: ele vai para onde você olha e A
  e D o viram, leva um momento para arrancar e para parar, anda de ré devagar, sobe um bloco no
  passo sem diminuir e pula dois, não sobe escadas nem voa. Ele fica virado para onde vai e vira no
  seu próprio ritmo em vez de girar com o mouse, para você e para todos que olham; num mundo jogado
  sozinho você o vê também da sela. Para mods: `core.body_mount`.
- F1 esconde as mãos, a hotbar, a mira e o que os mods desenham por cima da tela.
- Voar alto sobre as nuvens não deixa mais você congelado no ar, sem conseguir se mexer.
- Uma porta fechada em cima de você enquanto está no vão não te prende mais: você sai andando.
- Os tijolos das torres em ruínas são uma parede de tijolos, não um tijolo desenhado no preto.
- As poças dos pântanos são fechadas por todos os lados, e nenhuma árvore cresce de dentro de uma.
- **Protocolo 115**: não joga com servidores nem clientes 0.12.

## 0.12.0-alpha.1 — 2026-11-03

- O quanto o sandbox é difícil: pacífico, fácil, normal ou difícil. Escolhido quando um mundo é
  criado ("Monstros" na página de mundo novo), na página Editar de um mundo salvo, ou durante o jogo
  (`/difficulty`, `/peaceful`, ou o novo botão **Mundo** do menu de pausa); mantido depois de
  reiniciar. Pacífico não deixa sair nenhum monstro e manda embora os que estão por perto, e o que
  só os monstros deixam continua sendo encontrado: o gerador de uma masmorra, agora quieto, deixa de
  vez em quando o que as suas criaturas deixariam (ossos, carne, pólvora, flechas); o altar das
  profundezas dá o seu coração e fragmentos sem acordar o guardião; as vilas não têm cercos. Fácil
  corta pela metade o golpe de uma criatura, quantas saem e a fome; difícil aumenta cada um pela
  metade. Servidores: `--difficulty`, `--peaceful`.
- As regras do mundo (`/rule`, a página Mundo): manter a bolsa ao morrer num mundo de
  sobrevivência, o sol se movendo, o tempo mudando sozinho, a fome. A página Mundo também define o
  tempo, a hora e a estação para um admin.
- Objetivos: 21 coisas para fazer no sandbox, da primeira madeira e uma noite na cama até uma vila,
  uma relíquia, o coração do guardião e a chave de cada mundo, avisados com um sino quando são
  alcançados e listados com dicas em `/page goals`.
- Tempestades: o raio ilumina a terra, à noite também, com um clarão e uma tremulação; debaixo de
  uma tempestade as criaturas do escuro saem e não queimam.
- Agachado, você não cai mais de uma borda: para nela e desliza ao longo dela.
- Criaturas: atingidas e miradas no modelo inteiro (um guardião na cabeça); jogadores e criaturas
  empurram uns aos outros para o lado; uma que te persegue no sol procura primeiro a sombra, se
  houver.
- O mapa mostra a terra como está construída -- árvores, casas, estradas, água -- onde já foi vista,
  e pinta de novo o que está perto quando muda; relíquias e vilas são marcadas nele, como já eram os
  portais.
- Placas de pressão (ligadas debaixo de um jogador ou de uma criatura) e armadilhas de espinhos (de
  pé enquanto têm energia, um golpe duas vezes por segundo) para os circuitos de cinábrio; as portas
  também abrem com uma placa.
- Pôneis, na grama de dia: clique direito em um com uma sela (três couros e um lingote de ferro)
  para montá-lo -- mais rápido, a meio galope com o sprint, um pulo mais alto -- e Ctrl (ou um
  clique direito no chão) para descer. Você senta montado, pés nos estribos e mãos nas rédeas,
  subindo com o passo dele; as pernas dele andam e galopam embaixo de você, vistas também em
  primeira pessoa.
- Sela, turfa e ensopado de cogumelos têm modelos próprios na mão e figuras na bolsa.
- Duas terras raras nas planícies: pântanos de lama, poças, juncos e salgueiros sobre turfa (um
  combustível), e bosques de cogumelos gigantes sobre micélio com cogumelos luminosos sob os chapéus
  (uma luz); dois cogumelos marrons e um vermelho fazem um ensopado.
- Barracas de feira: abasteça uma, defina o preço e o lote, e outros jogadores compram dela enquanto
  você está fora; o dinheiro espera no caixa.
- A tradução italiana cobre também as mensagens dos mods (vilas, tempo, pedras de caminho, plantas,
  as provas dos mundos), e a inglesa os mundos escritos em italiano.
- Para mods: `core.difficulty`, `core.set_difficulty`, `core.peaceful`, `core.set_peaceful`,
  `core.rule`, `core.set_rule`, `core.day_cycle`, `core.set_day_cycle`, `core.mob_leave`,
  `core.sky_flash`; `core.overcast` num servidor escurece o dia que as criaturas leem.
- **Protocolo 114**: não joga com servidores nem clientes 113.
- O cubo de armazenamento das câmaras de testes agora é uma caixa de verdade: cai, desliza, tomba
  nas bordas, rola e quica, e fica do jeito que caiu; jogado ou empurrado, ele vira enquanto vai, e
  através de um portal continua virando. Os cubos batem uns nos outros e se empilham; você fica em
  pé neles, e um jogado em você te empurra para trás. Câmara de testes 14, os degraus: duas paredes
  altas demais para subir, um cubo levado ao pé de cada uma é o caminho para cima. Câmara de testes
  15, três cubos: uma parede meio nó mais alta, alcançada só de dois cubos empilhados, e esses dois
  só de um terceiro. Para mods: `core.mob_tumble(id)`, um worker transformado em caixa rígida
  (Jolt), desenhado virado como está em todo cliente.
- Cair num portal de piso e sair por um de parede não leva mais você um nó longe demais: você sai
  dentro da abertura da parede, onde a imagem mostrava, até onde um corpo cabe.
- Um bloco que um mod coloca e gira na hora, onde o mundo ainda não está carregado, mantém o giro:
  as portas das câmaras de testes às vezes ficavam de lado, e peças de pista podiam sair sem giro.
- O vidro é desenhado de novo onde estava faltando -- nas câmaras de testes ele não era desenhado
  de jeito nenhum, visto direto ou através de um portal: o renderizador perdia o shader quando a
  mistura mudava.
- Entre dois portais de piso ou de teto você mantém a direção em que ia: só a inclinação segue o
  portal (olhando para baixo ao entrar num, para cima ao sair do outro), e o horizonte inclinado
  volta suave ao nível.
- Um portal de teto te leva quando a sua cabeça chega nele, não os pés: pulando ou voando para
  dentro de um, a vista não entra mais no teto primeiro. (As paredes já iam pela cabeça, os pisos
  pelos pés, que chegam neles primeiro.)
- `tools/portallab.sh`: uma cena para conferir portais a olho -- vidro através de uma abertura de
  parede e de uma de piso, uma abertura de teto para pular dentro.
- Saindo de um portal, a vista não é mais girada: você olha para onde a travessia manda o seu
  olhar, e só um horizonte inclinado é posto de volta no nível, com uma transição suave entre um
  quarto e pouco mais de meio segundo. (A câmera de um kart ainda se nivela.)
- Câmaras de testes: um cubo carregado através do campo dissolvente não fica mais nas suas mãos; ele
  se desfaz ali, eletrificado (consumido com bordas azuis queimando, faíscas), e todos os outros o
  veem se desfazer onde estava. O tubo da saída de cubos não pisca mais visto de baixo.
- **Portais entre servidores construídos no mundo**: um anel de ouro de `portal:server_frame` (oito
  molduras de portal em volta de um rubi). O clique direito de um admin abre o seu menu: o nome, os
  servidores amigos para os quais ele está disponível, e os portais que eles oferecem a este
  servidor, um clique para conectar. O outro servidor aceita a ligação quando o seu portal é
  oferecido de volta; antes, os dois conferem que se ouvem, que falam um protocolo em comum e que
  os buracos têm o mesmo tamanho. Os servidores amigos compartilham isso como `portals` (ligado por
  padrão para amigos novos; acrescente de novo um antigo). Uma ligação feita dura mesmo com um dos
  servidores, ou os dois, desligados, e só acaba quando um admin a solta de um dos lados ou a chave
  de assinatura de um servidor muda. Três caixas no menu dispensam uma checagem cada (o outro
  servidor online, um protocolo em comum, buracos do mesmo tamanho), cada uma com o seu aviso.
- `/friend add` com um nome que já existe substitui aquele amigo: um servidor cuja chave mudou é
  acrescentado de novo com o mesmo nome.
- Câmaras de testes 11-13: a longa queda (num portal de piso no fundo de um poço, saindo por um no
  piso subindo, até uma saliência), três torretas sentinelas com desprezo de sobra (derrubadas por
  um cubo jogado ou levantadas por trás), e um campo dissolvente que fecha os seus portais e
  dissolve o que você carrega. Zonas de gatilho abrem portas e portais e acordam torretas.
- Parkour: a plataforma de impulso mira o lançamento no ponto de chegada, onde quer que você a
  toque.
- RPG de ação: você chega num salão com a câmera normal, e a vista de cima começa na porta dele; os
  caminhos para casa e de volta ficam virados para a câmera. Armaduras, elmos, luvas, botas e cintos
  servem no humano, no homem-toupeira e no homem-sapo.
- Os trilhos sobem e descem um degrau sozinhos, a partir dos trilhos ao lado.
- Assalto: um tanque custa 220 (eram 350), e um preso num buraco é erguido para fora.
- O prefeito de uma vila num mundo com praça diz o que a vila acha dos arcos.
- O texto do launcher é mais denso: letra menor, duas colunas, mais coisa numa tela.

## 0.11.0-alpha.1 — 2026-11-01

Protocolo 113: não joga com servidores nem clientes 0.10 (o launcher busca a versão que um servidor
quer). Lê os mundos da 0.10.

- **O launcher** (`cnp_launcher`), agora o que todo pacote abre, com o visual da tela de título:
  - **Versões**: a do pacote e outras baixadas da página de releases, lado a lado; uma é a padrão
    que Jogar inicia. Um download só roda se o `release.manifest` assinado da release o listar e o
    tamanho e o hash baterem; uma versão é removida com um segundo clique que diz quantos mundos ela
    criou.
  - **Mundos**: jogados direto da lista; um de outra linha de salvamento pergunta antes, e uma
    versão mais nova copia um mundo em `backups/` antes de abri-lo.
  - **Mods**: em todos os mundos ou por mundo, os do jogo (só leitura) e os seus; do que cada um
    precisa, o que pede, ligado e desligado, removido, e acrescentado soltando uma pasta ou um
    `.cnpmod` na janela.
  - **Seguir um servidor**: um servidor listado como "needs version X" (ou que recusa esta versão)
    manda o jogo de volta ao launcher, que inicia X, baixando se preciso, naquele servidor.
  - O jogo sozinho continua lá: `cnp_client`, ou `AppRun game` / `flatpak run ... game`.
- Os seus próprios mods: `~/.cubes-and-portals/mods` (Windows `%APPDATA%\cubes-and-portals\mods`)
  carrega no sandbox de todo mundo; a pasta `mods/` e o `mods.conf` (`off = a, b`) de um mundo
  acrescentam mods e os desligam ali. Servidores: `--user-mods DIR`, `--no-mod NOME`.
- Os pedestais mostram o que guardam, flutuando e girando sobre eles (`core.show_item`): a arma das
  câmaras, os fragmentos das ruínas, a próxima chave do altar das chaves.
- `/giveup`: abandona a prova de um mundo; você morre e volta ao sandbox do lado de fora do seu
  portão.
- Os comandos de chat são em inglês (`/journal`, `/giveup`, `/town`, `/pieces`, `/vehicle`), e todo
  comando está em `docs/COMMANDS.pt.md`.
- O status de um servidor diz quais protocolos ele fala.
- Parkour, a corda: o servidor fica sabendo onde ela se prende e quando é solta, e a descida de um
  balanço não é mais uma queda. Antes, cada balanço machucava, alguns matavam, e o renascimento
  deixava a corda presa, então o corpo voltava de repente para ela no meio do balanço.
- Parkour: o corpo e a vista giram em volta de um canto interno numa corrida na parede, o pulo de
  canto pode vir antes do canto, e as faixas de perigo continuam sem interrupção de bloco em bloco.

## 0.10.0-alpha.2 — 2026-11-01

Protocolo 111, como a 0.10.0-alpha.1: joga com os servidores e clientes dela e lê os mundos dela
(as suas esmeraldas carregam como rubis).

- `/time [day|noon|dusk|night|midnight|dawn|HH:MM]` (admin), `/kill [nome]` (você mesmo para
  qualquer um, outra pessoa para um moderador) e `/creative on|off`: modo criativo, voar sem o
  jetpack, sem dano, cavar rápido; livre num mundo só seu, de admin num servidor. Funciona também no
  mundo `--quick`, e a alquimia volta a carregar lá.
- A esmeralda agora é o rubi, em todo lugar: minério, gema, bloco de armazenamento, o dinheiro das
  vilas, pedras de caminho, marcos, saques. Carmesim, para não ser confundido com o cinábrio. Mundos
  salvos antes carregam as suas esmeraldas como rubis.
- O Flatpak se atualiza sozinho: instalado uma vez a partir do repositório
  (`flatpak install --user https://giacomo986.github.io/CubesAndPortals/cubes-and-portals.flatpakref`),
  `flatpak update` traz cada release nova. O repositório é assinado e fica no GitHub Pages; o
  `.flatpak` das releases ainda instala, mas um instalado a partir de um arquivo nunca é atualizado.
- As releases alfa e beta saem marcadas como pré-releases.
- A documentação e este changelog estão em inglês, cada um com o seu gêmeo italiano ao lado
  (`<nome>.it.md`).
- A lista de mundos salvos da tela de título marca os criados por uma versão anterior: uma moldura
  âmbar e a versão que os criou, ou "versão antiga" para um de antes de os mundos a guardarem (o
  novo arquivo `version` de um mundo, escrito uma vez quando ele é criado).
- Parkour: uma corrida na parede interrompida por uma parede atravessada no caminho (um canto
  interno) passa para essa parede com um pulo dentro de 0,4 s (`core.parkour`). O percurso de
  parkour tem mais duas estações antes da torre: duas vigas de corda seguidas, e uma corrida na
  parede em volta de um canto interno; depois duas vigas fazendo um canto, três em S e três em U.
- O Inferno: os seus drops têm usos (lingotes de ouro, resistência ao fogo no alambique, pó de
  cinza para um isqueiro e lanternas de brasa, blocos de quartzo e comparadores); campos de
  basalto, florestas de fungos e desertos de cinza; baús ao longo das fortalezas; bolas de fogo
  acendem incêndios curtos (novo evento `core:burst`); sons e cinza no ar; e uma moldura acesa no
  Inferno abre o caminho de volta para lá, com a gêmea construída oito vezes mais longe no mundo de
  cima.
- O Inferno junto a um local de sobrevivência é um nível próprio do local, separado do Inferno
  aberto: uma fortaleza de braseiros conforme d (túneis, uma ponte sobre a lava, uma galeria de
  espectros, colunas de basalto, um braseiro sob a lava, o escuro), o guardião no seu salão, e um
  caminho de volta ao começo. O coração da praça é sempre d 1. Num nível, uma moldura de obsidiana
  não leva a lugar nenhum e os baús dão menos.
- Líquidos: as suas superfícies ondulam (a onda sombreada pela inclinação, a lava mais devagar), e
  todas as faces deles balançam um pouco de lado, as colunas cheias também; um canto encostado num
  bloco fica parado, então o líquido gruda no bloco sem fresta nem sobreposição. A lava é um pouco
  transparente: o que está embaixo dela some na sua cor com a profundidade, quase desaparecendo a
  oito nós, e longe do olho ela é opaca. A opacidade de um líquido agora limita o alfa do arquivo de
  textura (as figuras das lavas eram opacas não importa o que o mod dissesse); as lavas estão em
  130.
- `tools/track.sh --fresh` faz do jogador o dono: o catálogo criativo (C) fica aberto.
- Melancias e abóboras são cultivadas: uma fatia é uma semente de melancia, uma abóbora dá quatro
  sementes de abóbora; semeada em terra arada, uma haste cresce em quatro estágios como o trigo e,
  crescida, põe um fruto numa célula livre ao lado, um de cada vez.

## 0.10.0-alpha.1 — 2026-10-31

Protocolo 111 (110 os trilhos, 111 até 24 bocas por sala): não joga com os servidores da 0.9, nem
os clientes deles com os seus. Lê os mundos da 0.9.0-alpha.4; um mundo novo pode ser sandbox ou
sobrevivência.

- Modo **sobrevivência** (menu de mundo novo, `--survival`): sem praça, um spawn aleatório em
  terra, os mundos temáticos atrás de portais espalhados com uma dificuldade de 1-10 que cresce com
  a distância, um nível para cada portal (parkour e backrooms gerados; arena, Inferno, front e
  assalto em escala), uma saída escondida que se abre quando o nível é vencido, um baú de recompensa
  na volta, chaves só a partir da dificuldade 4, o laboratório em algum lugar ao acaso. Boatos e
  direções (pagos, do prefeito) sobre portais por perto. Sem catálogo criativo e a bolsa num túmulo
  ao morrer. APIs novas `core.seed`, `core.plaza`, `core.bag_to_container`, o evento `core:died`;
  `/perm PERMISSÃO nobody`.
- O catálogo (C) pede a permissão `creative` (moderador para cima; no próprio mundo, o dono).
- Carrinhos nos trilhos: curvas, subidas e descidas, trilhos energizados por cinábrio (estações
  quando desligados), desvios. Os trilhos são percorridos nos dois sentidos e as curvas parecem
  curvas; as rodas batem nas juntas.
- Pedras de caminho: uma por vila ao lado do poço, fabricáveis para territórios; pagando uma
  esmeralda a cada 200 nós abre-se um portal de verdade para uma pedra já tocada, por cinco minutos.
- O painel criativo mostra de novo a mochila, o jetpack e a mochila de braços: um contêiner
  carregado recusava tudo o que se usa nas costas, os espaços do catálogo também.
- Caminhos entre vilas refeitos: uma rede (cada vila ligada às vizinhas), curvas suaves, um piso
  conforme o terreno (arenito, pedras, cascalho, terra batida) com bordas irregulares, placas de
  direção nas pontas, marcos com uma lanterna a cada 100 blocos. Mundos já explorados mantêm os
  caminhos antigos.
- Caravanas de vilas ainda não visitadas, com os produtos da sua terra, pelos caminhos; o mercador
  diz o caminho em voz alta e o marca no mapa. `/village incoming`.
- Eventos das vilas: caravanas de mercadores e de mulas entre as vilas (para comprar ou escoltar),
  cercos noturnos raros com aviso e recompensa, uma festa da colheita no fim do verão com barracas,
  música e fogos de artifício (novo `mods/fireworks`). Anúncios com o sino e no quadro; `/village`
  para os admins. `farm:season` traz também os dias que faltam.
- A mesa de xadrez também joga damas italianas e trilha; o gongo da arena começa rodadas de PvP de
  três minutos com placar e classificação. API nova `core.body_player`.
- Modelos na mão e ícones para todo item e bloco novo que não os tinha (as cadeiras no inventário
  não são mais quadrados de tábuas).
- Decoração: vitrais e painéis de vidro nas dezesseis cores, muretas, azulejos esmaltados,
  quadriculados e de pedra, cadeiras (você se senta nelas, e o corpo não gira com a câmera) e mesas
  de todas as madeiras, tapetes, lanternas de ferro e de vidro. Do carrinho também se desce com F.
  `core.player_clip` aceita uma direção para a qual o corpo fica virado.
- Ferrovias refeitas no Blender: dormentes e trilhos de verdade, curvas em arco, subidas sobre
  aterros, trilhos energizados que acendem, desvios com alavanca; um carrinho de mina de verdade; o
  jogador entra passando por cima da borda e viaja sentado; em primeira pessoa se veem o carrinho e
  as mãos nas bordas dele, e o carrinho segue a direção dos trilhos quando a vista gira. Um jogador
  agora também vê em si mesmo a pose e o que veste por causa de um mod do servidor (a mensagem
  OwnPose).
- Plantas: copiar uma construção, traçá-la como fantasma, mandar construí-la à mão, pelo construtor
  da vila ou por um robô. APIs novas: `core.held_id`, `core.held_set_id`, `id` em `core.give_item`,
  `core.ghost_cell`, `core.ghost_clear`, `core.node_colour`.
- Tarefas das vilas: seis tipos, um quadro ao lado do poço, pagas pelo tesouro, um desconto com o
  mercador depois de cinco.
- Criação de animais: ovelhas tingidas (lã colorida), leite (queijo e bolo; o balde fica), cochos
  que fazem um curral, a ordem "Cuide dos animais" do robô. APIs novas: `core.mob_kind`,
  `mob_love`, `mob_keep`, `mob_shear_now`, `mob_become`, `mob_voice`; `core:used_mob` nos animais
  também.
- Pesca conforme a água: percas, carpas, trutas, bacalhaus, enguias, atuns e um peixe-dourado raro;
  todos se cozinham e o pescador-sapo os compra.
- Alquimia: um alambique, frascos e cinco poções (visão noturna, rapidez, pele de pedra, cura,
  pressa, respirar debaixo d'água). Você se afoga: quinze segundos de ar debaixo d'água, depois
  perde vida. APIs novas: `core.night_sight`, e uma origem para `core.player_factors`, para que os
  efeitos de mods diferentes se somem.
- As Cavidades sob a rocha-mãe: cavernas luminosas, lava, cristais, espreitadores e um guardião. O
  mundo agora tem blocos até y -64.
- Um chunk embaixo do jogador só chega depois do de cima: acabaram as cavernas vistas de cima através
  de um chão que ainda não chegou.
- Estruturas raras com um enigma: templos no deserto, naufrágios, ruínas submersas.
- A bússola que aponta para casa e o mapa que se preenche enquanto você explora.
- Os portais não têm mais um máximo por mundo: o servidor mantém ativos os 32 mais perto dos
  jogadores. As molduras de portal não param mais em 16.
- O barco balança nas ondas. Barco e carrinho têm um ícone.
- Tijolos de pedra são feitos com pedra lisa: a receita antiga era a dos tijolos.
- O sandbox tem tempo: chuva, neve na tundra e nos picos, tempestades com raios e trovões. A chuva
  rega os campos.
- Estações: folhas que mudam de cor, campos parados no inverno sem lâmpadas, neve em todo lugar no
  inverno. Os mercadores dobram a comida no inverno e a madeira no outono.
- Barcos: cinco tábuas, clique direito na água para entrar, numa margem para sair. Vão ao dobro da
  velocidade do nado, viram com A e D, desviam das criaturas, batem uns nos outros e os remos se
  ouvem.

## 0.9.0-alpha.4 — 2026-10-27

Protocolo 109, como a 0.9.0-alpha.2: joga com os servidores dela e lê os mundos dela. A
0.9.0-alpha.3 não saiu: o pacote do macOS não compilava.

- Quem mantém um servidor tem uma administração completa: papéis, bans, kick, mute, uma lista de
  acesso, um console, RCON, reinícios agendados, manutenção, uma mensagem do dia, regras para
  aceitar, rollback de blocos, backups e webhooks. O manual é [docs/ADMIN.pt.md](docs/ADMIN.pt.md).
- Servidores amigos compartilham bans, presença, chat e a mochila: quem passa por um portal para um
  amigo leva a mesma mochila.
- A lista do Multijogador mostra os jogadores, a versão e a mensagem do dia de cada servidor.
- O título avisa quando há uma alfa mais nova no GitHub.
- O README também está em inglês.

## 0.9.0-alpha.2 — 2026-10-26

Protocolo 109, como a 0.9.0-alpha.1: joga com os servidores dela e lê os mundos dela.

- O jogo fala italiano e inglês: Options, Language escolhe o idioma (no começo, o do sistema). Um
  idioma novo é um arquivo em `assets/lang/`.
- No Multijogador a conta é do servidor: você escolhe o servidor, depois um nome e uma senha para
  ele.
- Na fuga do assalto os bots a bordo atiram nas armadilhas na hora certa.
- A praça do spawn é tranquila: acabou a lâmpada piscando e os faróis acendendo em turnos.
- Os pacotes não têm mais modelos de terceiros, e são feitos a partir de um clone do repositório; o
  código é público, sob a licença MIT, e as fontes Blender dos modelos estão em `assets/blender/`.

## 0.9.0-alpha.1 — 2026-10-25

Protocolo 109: não joga com os servidores da 0.8.0-alpha.1. Lê os mundos dela.

- Online, em mundos com muitos soldados (o assalto, o front), soldados e jogadores não pulam mais de
  um lugar para outro: os corpos viajam em pacotes pequenos, então um pacote perdido para poucos
  por um momento; quem sai da vista some na hora em vez de ficar parado; até 96 são vistos.
- O servidor escreve no log quando não dá conta, e por quê.

## 0.8.0-alpha.1 — 2026-10-25

Protocolo 108: não joga com os servidores da 0.7.0-alpha.5. Lê os mundos dela.

- Um mundo novo, **o assalto**, jogado junto contra Basoalth (pela praça do sandbox, em 90.5 21
  76.5): quatro fortalezas para tomar uma depois da outra, créditos e uma loja, aliados saindo dos
  quartéis, construtores que consertam, põem escadas nas muralhas e ninhos de sacos de areia, bots
  que explodem o portão da fortaleza; o salão do núcleo com os seus pilões; Basoalth crescendo até
  virar um gigante, o piso desabando e a fuga de jipe por um túnel de lava com curvas, subidas e
  descidas, válvulas, pedregulhos e tanques para acertar e atrasá-lo.
- O vale do assalto: um céu vermelho com nuvens em redemoinhos girando, tornados que sugam e
  arremessam, uma cadeia de vulcões com correntes de lava, um rio de lava com uma ponte e ilhotas.
- Quem morre volta com todas as armas e munição cheia.
- Nas máquinas: os degraus têm rampas mais longas e são subidos sem parar (em todos os mundos);
  como passageiro você atira com a sua própria arma e 1-9 a escolhe; sentado você fica sentado com
  a arma, numa torreta com as mãos na arma, virado para onde olha.
- Máquinas e objetivos não somem mais quando há muitos soldados por perto.
- Atrás do vidro você vê o que está lá (antes, os corpos passavam na frente dele); as nuvens não são
  mais pontilhadas.
- Karts: derrapagem com Ctrl, marcas de pneu na estrada, as chamas do nitro, e uma garagem para
  fazer o seu próprio kart.
- As ruínas: uma cidade, três zonas, classes e itens com propriedades; barris, caixotes e urnas para
  quebrar; feitiços e golpes com efeitos novos.
- Parkour: a corda vai para onde está a mira, em terceira pessoa também.
- Bolsa e criativo com espaços menores e a barra embaixo; um baú catálogo.
- O baralho ilustrado tem cartas novas de ouros, copas e espadas.
- Os passos são mais baixos.

## 0.7.0-alpha.5 — 2026-10-18

Protocolo 100: não joga com os servidores da 0.7.0-alpha.4. Lê os mundos dela.

- Você entra no front por uma galeria na crista, no meio do caminho entre as bases e bem acima do
  campo, com uma janela para assistir à batalha. Dois portais, um vermelho e um azul, levam ao
  bunker da base: o que você atravessa escolhe o time. Do bunker você volta à galeria.
- A batalha dos bots começa mesmo que só haja alguém assistindo da galeria.
- As máquinas são sólidas: duas máquinas colidem em vez de se atravessarem, quem está a pé não entra
  no casco, sobe nele e é carregado.
- Quem dirige vê a máquina se mover na hora, sem esperar o servidor: o cliente a simula e a alinha
  com a do servidor.
- Soldados em armas fixas (antiaérea, metralhadora, morteiro) olham para onde a arma aponta.
- Corrigido um crash do servidor quando duas máquinas nasciam no mesmo instante.
- O baralho ilustrado tem também o 2, o 3 e o 4 de ouros.

## 0.7.0-alpha.4 — 2026-10-18

Protocolo 99, como a 0.7.0-alpha.3: joga com os servidores dela e lê os mundos dela.

- No front você mira: com o botão direito a arma sobe para o meio da vista, os tiros vão para onde
  você olha e você anda devagar. Um novo fuzil de precisão com luneta. Do quadril os tiros se
  espalham. As armas são vistas seguradas, apontadas, disparadas e recarregadas de fora também.
- Em terceira pessoa os tiros vão para onde está a mira, mesmo com um soldado ou uma máquina na
  frente; passando da primeira para a terceira pessoa e de volta, a mira fica no mesmo ponto.
- Os soldados bots estão mais ao alcance: só veem para a frente, esperam um momento antes de
  atirar, atiram rajadas menos precisas e causam menos dano. Eles lutam de quatro jeitos (a
  sentinela com o fuzil de precisão, o de assalto com a metralhadora, o cauteloso que se protege, o
  antitanque com o lança-foguetes), mantêm a sua arma enquanto ela tem munição e ela é vista nas
  mãos deles.
- Os bots usam as máquinas: dirigem jipes e tanques, pilotam o helicóptero, o avião e o
  bombardeiro, ocupam ninhos, morteiros, obuses e antiaéreas, e entram em máquinas dirigidas pelos
  companheiros, jogadores também.
- Máquinas novas: o bombardeiro (uma fileira de seis bombas, com o ponto de impacto marcado, e uma
  torreta nas costas) e a antiaérea, que trava em helicópteros e aviões e lança mísseis que os
  perseguem.
- Nas máquinas: F põe você no lugar livre mais perto (por trás do jipe você entra atrás) e 1-9
  trocam de lugar sem trocar a arma na mão. As janelas das máquinas são transparentes.
- Os painéis de vidro são transparentes.
- O front vai ser o prêmio de quem vencer o Hóspede no laboratório; por enquanto continua aberto a
  todos.

## 0.7.0-alpha.3 — 2026-10-18

Protocolo 99: não joga com os servidores da 0.7.0-alpha.2.

- Um mundo novo, o front: um campo de batalha entre uma base vermelha e uma azul, com três pontos
  para capturar (fazenda, vila, bunker no morro) e 150 tickets por time. Você entra pela porta do
  bunker entre os sacos de areia na praça. Soldados bots completam os times até 8.
- No front: fuzil automático e metralhadora, pistola, granadas e lança-foguetes. As explosões cavam
  o chão e derrubam o que fica pendurado.
- Máquinas: jipes de quatro lugares com metralhadora, tanques que sobem degraus e atravessam
  paredes, helicópteros com foguetes, aviões com metralhadoras e bombas, ninhos de metralhadora,
  morteiros e obuses que atiram para onde a mira aponta. Você entra e sai com F, 1-9 trocam de
  lugar, o mouse mira e guia o que voa. Uma máquina destruída pega fogo, solta as peças e afunda.
- Sentado numa máquina o corpo fica preso a ela; ao volante do jipe as mãos ficam no volante, e em
  primeira pessoa só se veem os braços.
- Cartas: as cartas jogadas, recolhidas, compradas e distribuídas voam para a mesa uma de cada vez;
  o cursor sobre uma carta jogada a levanta e diz quem a jogou.
- Corrigido: depois de sair de uma máquina ou morrer nela, o servidor podia parar de aceitar o
  movimento (granadas que não saíam, tiros que não chegavam).

## 0.7.0-alpha.2 — 2026-10-17

Protocolo 97: não joga com os servidores da 0.7.0-alpha.1.

- Multijogador: os outros jogadores e as criaturas se movem suaves, não mais aos trancos,
  deslizando de uma posição recebida para a seguinte.
- Multijogador: quem voa é visto voando (não mais andando quando voa baixo).
- Voar: o corpo se inclina na direção em que voa e plana quando é solto.
- Pista: ícones para o jetpack e as ferramentas do cronômetro.
- Cartas: o naipe de paus, o verso e o ás de ouros ilustrados.

## 0.7.0-alpha.1 — 2026-10-17

Protocolo 96: não joga com os servidores da 0.6.0-alpha.2.

- Personagem: além do robô, um humano, um homem-toupeira e um homem-sapo, cada um com três estilos
  (cabelo curto, cabelo comprido, barba; lanterna de mineiro, óculos de proteção, óculos; manchas,
  coroa, vitória-régia), as três cores e os acessórios de antes. O esqueleto e os clipes do robô,
  braços próprios em primeira pessoa. Os acessórios foram refeitos para servir em cada espécie (o
  sapo usa o boné virado para trás); um acessório na cabeça toma o lugar da lanterna, da coroa ou
  da vitória-régia.
- Karts: mochila, armadura e a coisa na mão ficam no piloto, não mais no kart (o jetpack ia parar
  na cabeça e em primeira pessoa cobria a vista).
- Karts: você sai com F, não mais com o botão direito, que fazia sair sem querer.
- Personagens esculpidos no Blender: o humano, a toupeira e o sapo são superfícies contínuas e lisas
  (não mais primitivas coladas) que dobram nos cotovelos e nos joelhos; o cabelo cobre a nuca. A
  medalha e as pontas do cachecol balançam só para a frente, sem entrar no peito.
- Ferramentas: picareta, machado, pá, espada e enxada de madeira, pedra, ferro, ouro e diamante,
  cada uma com a sua forma; elas se gastam com o uso.
- Mochila de braços: uma mochila de metal com dois braços articulados, uma ferramenta em cada
  garra; ela cava com a melhor entre a mão e os braços e soma as espadas.
- Vilas: cada trabalhador pega dos baús a melhor ferramenta do seu ofício, o artesão faz ferramentas
  de ferro, e as de ferro e diamante são compradas.
- Editor de pistas: B (ou o botão "pick from afar") escolhe de longe; em vez da hotbar, uma grade
  com todas as peças, clique para testar uma, W para ficar com ela. A/D não travam mais ao remover
  curvas inclinadas e paredes, que antes continuavam colocadas.
- Peças de pista: elas são colocadas também dentro do terreno, que é removido (e 3 blocos acima da
  estrada); só outra peça as impede.
- Jetpack: o som é bem mais baixo e sobe um pouco enquanto você se move.
- ARPG: a sala de chegada é segura, as criaturas ficam de fora; W A S D interrompem a caminhada por
  clique.

## 0.6.0-alpha.2 — 2026-10-14

Joga com os servidores da 0.6.0-alpha.1 (protocolo 93); os mundos salvos são lidos.

- Pista: caixas de power-up depois de cada linha de chegada, como nos jogos de kart: turbo, turbo
  triplo, banana, casco, casco teleguiado, tinta, estrela, raio. Dá para guardar dois.
- Editor de pistas (em /pezzi): clique direito numa peça oferece as que encaixam na ponta aberta,
  Undo remove a última, Close circuit fecha o circuito sozinho, nivelando primeiro. As peças na mão
  e no chão são vistas em miniatura.
- Pista: a ferramenta das caixas, o painel criativo e o jetpack para voar por cima dela.
- Pista: loops duas vezes mais altos; o kart não tropeça mais ao sair dos loops.
- Luz contra o escuro: F8 mostra onde as criaturas saem, os robôs iluminam esses lugares, uma âncora
  os mantém trabalhando.
- Ajudantes: uma pedreira com a sua escada para sair, e Close no painel do programa.
- Portais: entre salas do mesmo servidor não há tíquete, e um tíquete vencido é renovado em vez de
  recusado.

## 0.6.0-alpha.1 — 2026-10-12

Não joga com os servidores da 0.5 (protocolo 93); os mundos salvos continuam sendo lidos.

- O robô é personalizado: Robot, no menu inicial ou na pausa, escolhe uma cor primária, uma
  secundária e uma de destaque (16 cores) e um acessório na cabeça (boné, cartola, fones de ouvido)
  e um no pescoço (cachecol, gravata-borboleta, medalha), com o robô girando ao lado. Os outros
  jogadores o veem, também quando ele é mudado no meio do jogo.
- Luz nova: escuridão de verdade, luz suave, lâmpadas iluminando as faces viradas para elas, sol e
  lua com sombras. A luz de antes continua nas opções (Light: classic), e as sombras podem ser
  reduzidas ou desligadas.
- Tela cheia: F11 ou nas opções.
- Cartas: à mesa o robô se senta e segura as cartas na mão.

## 0.5.0-alpha.1 — 2026-10-11

Não joga com os servidores da 0.4 (protocolo 91); os mundos salvos continuam sendo lidos, e as
áreas já geradas mantêm o terreno que tinham.

- Mundo: oceanos, mares, praias, rios, lagos de altitude e gargantas, com pontes onde os caminhos
  os atravessam; debaixo da terra, minas abandonadas e criptas.
- As saídas dos mundos ficam seladas em vermelho até aquele mundo ser vencido; os arcos na praça
  levam à saída de cada mundo.
- O Inferno carrega os mods de jogo do sandbox: o que você leva na bolsa vale lá também; fome,
  experiência e chaves são as mesmas em todas as salas.
- Pista: uma pista nova com três loops, a câmera seguindo o kart nos loops, e nenhum clarão verde
  ao atravessar o Inferno.
- A pista do Inferno: uma moldura de obsidiana a mais removida do portal de saída; descendo e
  subindo de novo no kart no Inferno você volta à pista desenhada direito, e a vista escolhida
  continua.
- Xadrez: oito variantes. Parkour: sempre correndo, e você se agarra nas bordas.
- Um mundo novo começa em cerca de 25 segundos (eram 80), com uma barra no título.

## 0.4.0-alpha.1 — 2026-10-10

A primeira alfa distribuída.

- Pacotes: Linux (tarball, AppImage, Flatpak), Windows (zip), macOS (`.dmg`, nunca testado num Mac).
- A versão está escrita no canto inferior direito do menu: informe-a ao relatar um bug.
- No Windows: sem stack dump num crash, e voltando ao menu a senha tem de ser digitada de novo.
