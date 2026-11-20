# O assalto

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

O assalto final à dimensão de Basoalth, para jogar junto. Dois mods: `mods/assault_core` (só
servidor: o vale, as fortalezas, o salão do núcleo, o túnel da fuga) e `mods/assault_game`
(`side = both`: as fases, os créditos e a loja, o salão, Basoalth, a fuga no servidor; o HUD e os
efeitos no cliente). Armas, granadas, bots e veículos são os do front (`games/front`,
`front_game`), copiados e adaptados a um jogo contra o computador.

## Como chegar lá

`tools/track.sh` (ou o menu de título) inicia o sandbox com suas salas; a porta do assalto, um
portão de basalto preto com duas brasas violeta, fica em 90.5 21 76.5 (`mods/gates`, a décima
sétima boca do sandbox). Como o front, no jogo final ela só abre para quem venceu o Hóspede no
laboratório (`FRONT_OPEN = true` em `mods/keys/init.luau` abre os dois, para testar; `false` é o
jogo como ele é distribuído). Num mundo de sobrevivência o assalto fica atrás de portais
espalhados além de 1500 do spawn, fechados até o final; cada guarnição tem 10% a mais a cada passo
de dificuldade do local, e a vitória é o nível concluído.

O mundo não é salvo (sem `--world-file`): quando ninguém passa por lá por meio minuto, suas
colunas voltam a ser o que o mapgen faz, e a dimensão fica inteira de novo. O estado da partida é
zerado 20 s depois que o último jogador sai.

**Testar só a fuga**: `tools/escape.sh` inicia um servidor só com o mundo do assalto e um cliente;
três segundos depois que você entra, `tools/assault_escape` pula as fortalezas e o salão (evento
`assault:to_escape`): você está no salão, o núcleo se quebra, Basoalth se ergue, o piso desaba e
os jipes esperam no túnel. Recomeça toda vez que alguém volta a um mundo vazio.

## O vale

128 blocos de largura, 1080 para o norte; a leste, uma fenda de magma ao longo da qual corre o
túnel da fuga. Em volta, até 110 blocos para fora, uma cadeia de montanhas que sobe à medida que
se afasta (até 48 acima do chão) e nove vulcões: um cone de 66 a 90 de altura, uma cratera cheia
de magma e três correntes que descem serpenteando em direção ao vale (`VOLCANOES` em
`assault_core`). Um rio de lava atravessa o vale entre o bastião e a bateria (z 590-610,
serpenteando): ele desce por um desfiladeiro nas montanhas a oeste, corre três blocos abaixo do
chão entre margens de basalto e cai na fenda a leste. Atravessa-se pela ponte da estrada (9 de
largura, parapeitos e lâmpadas) ou, nos lados (x 22 e 104), pulando por três ilhotas de basalto a
um ou dois blocos uma da outra. A lava (`assault_core:lava`) mata jogadores e soldados e destrói
veículos; os bots ao volante a evitam. O céu é vermelho (`core.world_storm`, protocolo 108): o sol
continua lá, mas as nuvens têm a cor do céu e giram em vórtices aqui e ali: cada um gira inteiro
(uma volta a cada meio minuto, mais ou menos) envolto em dois braços em espiral, com um olho limpo
no meio. Alguns ameaçam um tornado: uma parede escura no meio e, embaixo dela, um funil que desce
em direção ao vale e volta a subir (até 44, nunca até o chão), a cada minuto e meio, mais ou
menos. Custo: com llvmpipe no Xephyr o acampamento cai de 25.7 para 21 FPS.

| fase | onde (x, z) | o que é | o terreno em volta | guarnição (1 jogador, +2 para cada jogador a mais) |
|---|---|---|---|---|
| acampamento | 64, 22 | um pátio de concreto, a porta de chegada, as caixas | campo | -- |
| 1 o posto avançado | 64, 250 | uma paliçada de troncos, duas torres, uma cabana | pântano morto: lama, poças, juncos, árvores secas | 8, duas metralhadoras |
| 2 o bastião | 64, 480 | muralhas de pedra, torres, uma torre de menagem | floresta queimada: cinza, troncos carbonizados, brasas | 11, duas metralhadoras, dois morteiros, um tanque |
| 3 a bateria | 64, 710 | três bunkers, trincheiras, dois obuses, antiaérea | um campo de cristais violeta luminosos | 14, duas metralhadoras, um helicóptero |
| 4 a fortaleza | 64, 905 | muralhas de basalto de 12 de altura (indestrutíveis), o portão para explodir | basalto rachado pelo magma, costelas de osso | 18, dois tanques, morteiros, antiaérea, um helicóptero |
| o salão | 64, 976 | atrás da fortaleza, afundado 10 abaixo do pátio: quatro pilares, o núcleo, o trono | -- | ondas que saem de portas nas paredes laterais |

A legião tem três espécies, escolhidas pelo jeito de lutar: **zumbis** (atrás de abrigos, lentos,
28 de vida), **demônios** (no ataque, rápidos, 18) e **ciborgues** (de vigia com o fuzil de
precisão e o lança-foguetes, 26).

## Como se joga

* **Uma fortaleza** é tomada ficando junto da sua bandeira (10 blocos) sem ninguém da legião a
  menos de 20: a captura sobe 10 por segundo para cada aliado, até três. Tomada, a bandeira fica
  azul, o que resta da guarnição foge, a base se muda para lá (você renasce lá, as caixas
  recarregam) e a próxima se arma. O portão de uma fortaleza tem de ser explodido com granadas,
  foguetes ou um tanque.
* **Créditos**: um caixa só para todos. Um inimigo morto por um jogador vale 10, por um bot aliado
  5, uma fortaleza 150, um pilar 100; e um suprimento lento, 5 a cada 5 segundos enquanto há
  combate (fortalezas e salão).
* **A base** fica no terreno aberto ao norte da última fortaleza tomada, na estrada para a
  próxima: lá você renasce, as caixas recarregam, os soldados aliados caídos voltam em ondas a
  cada 15 s e os veículos aliados (os comprados também) têm cada um sua vaga no estacionamento. Um
  veículo aliado sem jogadores a bordo, parado por 40 s longe do estacionamento (preso, num
  buraco), volta para ele. O posto avançado e o bastião também têm uma passagem ao norte, e as
  trincheiras da bateria uma na estrada.
* **Loja** (B a menos de 30 blocos da base): cinco soldados a mais (150, até 100), um construtor
  (120, até 6), a metralhadora leve (120), o fuzil de precisão (120) e o lança-foguetes (200) para
  todos; melhorias em níveis, cada nível mais caro que o anterior: armas +15% de dano (150, x1.6,
  até 8), veículos +25% de resistência (180, x1.6, até 6), blindagem contra explosões -15% (140,
  x1.7, até 4); um jipe (100), um tanque (220), um helicóptero (500). Você começa com fuzil,
  pistola, granadas, quatro soldados e dois construtores.
* **Zonas bônus**: nos lados de cada fortaleza (x 10 e 118), uma clareira com um farol, guardada
  por três da legião; o feixe de luz dele é visto de longe, violeta ou azul. Tomada (ficando junto
  do farol sem inimigos a menos de 14), +50 créditos e +3 em cada suprimento. Um aliado em cada
  três vai tomá-las por conta própria.
* **Contra-ataques**: a cada 100-140 s, enquanto se luta por uma fortaleza, sai dela um jipe (dois
  com mais jogadores, ou às vezes) cheio de soldados novos rumo à sua base; ele os deixa lá e eles
  lutam.
* **Construtores**: aliados desarmados e mais resistentes (50 de vida), com a chave inglesa: vão
  às crateras atrás da linha de frente (bases tomadas, a estrada) e as deixam como eram. Dois no
  começo, mais na loja (até 6). Metade deles cuida primeiro de construir para a fortaleza a tomar:
  uma escada na face sul do que fica a 12 e a 22 blocos do centro (um muro, uma torre ou um bunker
  de 3 a 16 de altura com espaço em cima) e quatro ninhos de sacos de areia 25 blocos antes da
  área. Um construtor parado por 3 s dá um passo para o lado; um trabalho que ele não alcança em
  25 s fica largado por 45 s; o trabalho de um construtor morto fica livre de novo. Testado: no
  posto avançado, duas escadas e quatro ninhos.
* **Quartéis**: soldados aliados e construtores saem de um quartel de tábuas 21 blocos a oeste da
  base, com a porta a leste. O do acampamento está sempre lá; os outros aparecem quando a base se
  muda para lá (`assault:barracks`), e a partir daí o mapgen e os reparos os contam como parte da
  dimensão. Em aberto: não testado se um cliente que entrou depois vê os das bases tomadas.
* **O portão**: diante do portão da fortaleza, a menos de 30 blocos, um aliado a pé de cada vez
  chega a 7 blocos e detona uma granada na célula do portão mais próxima que resta no meio, a cada
  3.5 s; se não conseguir em 15 s, outro assume. Testado: aberto em uns 45 s. `ponytail`: a
  granada é colocada no portão, não lançada (lançada, ela quicava de volta; deixada aos pés, caía
  na cratera).
* **Renascer**: quem morre (baleado, explodido, numa queda, no magma) volta com todas as armas
  compradas e com pentes, reservas e granadas cheios. As mortes dadas pelo motor são reconhecidas
  pelo salto até o ponto de renascimento.
* **Tornados**: enquanto se luta por uma fortaleza, a cada 45-85 s (o primeiro depois de 30) desce
  um, no máximo dois ao mesmo tempo, no terreno aberto entre a base e a fortaleza; ele vaga a
  1.5-2.5 m/s por 45-75 s. A menos de 14 blocos, suga e gira jogadores, soldados dos dois lados e
  veículos (tanques mal se mexem); no centro (3.5), joga para cima e para fora e tira 7, a cada
  2 s. Testado: um jogador sobe 4.4 blocos, é arremessado a 10, sugado de novo e morto no terceiro
  arremesso; quatro soldados mortos em poucos segundos. O arremesso de um jogador fica abaixo de
  16 m/s por eixo (`net::Push::kMax`). O passe das nuvens o desenha (`core.storm_funnel`): um funil
  da barriga das nuvens até o chão, poeira na base. Custo: com llvmpipe no Xephyr, dois tornados
  perto levam o acampamento a 11 FPS (21 sem eles).
* **Como passageiro** (um assento sem a arma do veículo) você atira com a sua própria arma e as
  teclas 1-9 a escolhem; elas só trocam de assento no volante ou numa arma do veículo. No começo de
  cada tentativa de fuga as armas de todos ficam cheias de novo (na estrada não há caixas).
* **Créditos de teste**: `tools/track.sh --fresh` coloca também `tools/assault_test` na sala, e
  cada partida começa com 100000 créditos.
* **Nos veículos**: quem não dirige e não está numa arma do veículo atira com a sua (jogadores e
  bots), e mira com o botão direito. Os bots sentados são levados pelo motor (`core.mob_ride`)
  junto com o veículo.
* **A descida**: o salão é coberto e afundado, com o piso 10 abaixo do pátio, então nada lá dentro
  pode ser atingido de fora. Tomado o pátio, abre-se a porta no fundo de um fosso encostado na
  parede sul do salão (x 59..69, z 928..940); os construtores vão até a borda sul do fosso e baixam
  três tirolesas, 22 s cada (`assault_core:zipline`, um escorregador do motor desenhado como uma
  corda: você vai sentado na linha dela e preso a ela, então a descida não tira vida -- entra-se
  andando a partir da borda). Enquanto não há nenhuma, metade de cada onda da guarda sai no fosso
  para atirar nos construtores. Os bots aliados esperam na borda e depois descem (o servidor os
  leva pela linha, 5 células por segundo). Os mortos voltam à base do pátio e descem de novo.
  Pular sem tirolesa é uma queda de 10 blocos. Testado headless: duas linhas prontas 24 s depois
  do pátio, a terceira em ~50 s; um jogador entrou pela borda e estava no fundo em 1,6 s com 20 de
  20 de vida; aliados no salão.
* **O salão**: os quatro pilares alimentam o escudo do núcleo; derrubados, o núcleo fica exposto
  por 18 s, depois os pilares se acendem de novo, mais fortes. Basoalth lança esferas do trono, que
  um tiro apaga.
* **A ascensão**: quebrado o núcleo, Basoalth toma a energia dele, cresce até 34 metros e arrebenta
  o teto; então o piso do salão desaba e todos caem (sem dano: a vida volta cheia antes e depois)
  entre montes de entulho, no fundo do túnel da fuga.
* **A fuga**: três jipes no fundo do túnel, um dirige e os outros atiram; sozinho, um bot dirige ou
  fica na metralhadora. O túnel (`assault_core`, `TUBE`; `A.PATH` em `assault_game`) tem 24 de
  largura e 44 de altura, porque o gigante anda nele em pé: para o norte sob a garagem, para o leste
  sob a crista, depois descendo pela fenda em curvas para a direita e para a esquerda (15 curvas em
  ângulo reto, planas por 12 blocos em volta de cada canto), subidas e descidas entre -2 e 10
  (nunca mais de um bloco a cada 8), até a rocha com a porta de saída, ao ar livre. Lâmpadas ao
  longo das paredes. É um canal de escoamento da lava: a estrada tem 13 de largura no meio, e nos
  lados, até as paredes, o magma corre dois blocos mais baixo (as curvas são estrada de parede a
  parede, então quem corta a curva não cai); quem cai nele, a pé ou de jipe, está perdido. Depois
  de 14 s o gigante se move e o piso desaba atrás dele (9 m/s, até 14, +6 quando alguém está mais
  de 50 m à frente); a cada poucos segundos um soco abre um buraco num lado (marcado em vermelho
  1.6 s antes), e as esferas dele perseguem os jipes. Cada tiro nele o freia (uma metralhadora que
  o mantém sob fogo o leva a uns três quartos do passo, duas a menos de dois terços, nunca abaixo
  de 40%; a cabeça conta em dobro) e tiros suficientes na cabeça o fazem cambalear por 3.5 s. Todos
  pegos: só o piso é refeito (em mais ou menos um segundo), os jipes voltam 2 s depois e você tenta
  de novo do fundo do túnel. Um jipe na rocha do fim, e a porta de casa se abre para todos.
* **Armadilhas no túnel**: a cada 110 m, mais ou menos, em rodízio, um alvo para atingir, refeito
  a cada tentativa (os bots a bordo também atiram neles, na hora certa: as pedras logo, a válvula
  com ele a menos de 150 m, o tanque de gás a menos de 55): a **válvula** de magma numa parede (40
  de vida) abre um jato de lava através do túnel por 25 s, e Basoalth, passando por ele, anda a 45%
  por 5 s; o **cacho de pedras** sob o teto (60) cai nos canais, e ele tropeça nelas e cambaleia por
  4 s; o **tanque de gás** num canal (50) explode na hora, e se ele estiver a menos de 40 m o freia
  muito e o faz cambalear por 2.5 s (cedo demais, é desperdiçado). Para cada um Basoalth tem a sua
  animação (`tools/make_assault_models.py -- --giant`): na lava ele bate os pés e bate nas pernas
  com a cabeça jogada para trás (burn), nas pedras ele se curva para a frente até o chão e se
  levanta de novo (trip), o tanque o joga para trás e ele ergue os braços diante do rosto (blast).
  Testado atingindo cada um a 25 m: com um bot dirigindo chega-se a uns 900 m em vez de 500.
  Testado com um bot dirigindo e ninguém atirando: uns 1100 dos 1440 m antes de ser pego (500 antes
  de os degraus ganharem rampas de três células, `core/physics/craft.cpp`).

## Os modelos

`tools/make_assault_models.py` (Blender): zumbis, demônios e ciborgues, Basoalth e o gigante, o
núcleo, os pilares. O resto é do front (`tools/make_front_models.py`) e do laboratório (esferas e
escudo, `tools/make_ospite.py`).
