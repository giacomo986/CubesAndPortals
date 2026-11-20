# O jogo de plataforma

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

Um jogo de plataforma visto de lado, à maneira dos jogos dos anos oitenta: três fases, uma sala
bônus, um chefe, power-ups, suas regras e a câmera. Dois mods: `mods/platformer_core` (só
servidor: o robô, como ele pula, os modelos dos canos, dos power-ups e das criaturas) e
`mods/platformer` (`side = both`: a sala, as fases e as regras no servidor; a câmera e a contagem
na máquina do jogador).

## Como chegar lá

No jogo (o menu de título, ou `tools/track.sh`) o portão dele fica na borda sul da praça, em
40.5 21 26.5: um cano verde num muro de tijolos sob um bloco de prêmio (`mods/gates`). O mundo não
é um dos que o servidor inicia junto com ele: está no arquivo de salas, começa quando alguém chega
perto do portão e para um minuto depois que o último saiu; o portão é um portal entre salas que
pega uma boca emprestada. `tools/platformer.sh` inicia o sandbox só com este mundo, atrás da sua
primeira abertura (`tools/platformer.sh -- --room platformer` nasce dentro dele). De um jeito ou de
outro você sai numa **sala** cuja parede do fundo é de vidro: do outro lado, visto de lado, está a
primeira fase. A porta da esquerda dá para uma ponte com um buraco, cercado de blocos de prêmio, e
para um cano verde. **O buraco é a fase 1-1**: é um portal que muda de escala
(`core.portal_scale`), de dois por dois, para um de um por um no céu sobre o começo da fase --
olhando dentro dele da ponte você vê a fase de cima. Quem pula dentro cai daquele céu com metade
da altura, como o jogo quer, e a máquina dele gira a câmera de trás dele até o lado enquanto ele
cai. O cano, sob uma verga azul, leva à fase mais adiantada que está aberta para você: a 1-2
depois de terminar a 1-1, a 1-3 depois de terminar a 1-2 (guardado pelo nome no armazenamento do
mundo).

## Numa fase

A câmera fica de lado (`core.side_view`: `docs/MODDING.pt.md`) e acompanha como a tela de um jogo
de plataforma: você se move um pouco para um lado ou para o outro antes de a imagem rolar, depois
ela rola suave; um pulo a move menos que uma queda. As teclas são as de um jogo de plataforma:

| tecla | |
|---|---|
| A, D | andar para a esquerda e para a direita |
| W ou espaço | pular (2,4 para cima; 4,4 para a frente andando, 7,2 correndo) |
| S | agachar; em cima do cano alto da 1-1, descer por ele |
| Shift | correr |
| F ou botão esquerdo | lançar uma bola de fogo, com a flor |
| E | desistir: volta à sala |

**Pequeno, grande, fogo.** Você começa pequeno: metade da altura do robô, e com números duas vezes
maiores (`core.body_movement`, que encolhem com o corpo) para correr e pular tão longe quanto no
tamanho normal. Um bloco de prêmio com power-up dá um cogumelo a um corpo pequeno e uma flor a um
grande: o cogumelo sai do bloco, desliza para a frente e cai das bordas, e quando tocado deixa
você grande (o corpo pisca entre os dois tamanhos e depois cresce); a flor fica no bloco e dá
bolas de fogo, duas por vez, que quicam pelo chão e matam qualquer criatura; enquanto você as tem,
uma aura de chama vermelha arde em volta do corpo (o efeito `"aura"` do motor,
`core.render.fx`). Atingido com o fogo, você o perde e continua grande; atingido grande, você volta
a ser pequeno e nada te atinge por dois segundos; atingido pequeno, uma vida.

Três vidas. Uma queda ou um golpe quando pequeno custa uma e põe você de volta, pequeno, na última
bandeira por onde passou (o começo, ou a azul do meio); sem vidas, você volta para a sala.

| criatura | | quando você pula em cima |
|---|---|---|
| broto (sprout) | um cacto de dois pés, saltitando no seu trecho | morre, e você quica |
| zumbidor (buzzer) | um inseto roxo que voa para cima e para baixo enquanto avança | morre, e você quica |
| casco de espinhos (spikeshell) | uma bola de espinhos vermelhos, rastejando | um golpe: pule por cima dela, ou queime-a |

Os tijolos suspensos (as fileiras ao lado dos blocos de prêmio, as plataformas -- não os muros e
os castelos) quebram: batidos por baixo por um corpo grande, ou quando um cogumelo faz um corpo
pequeno crescer embaixo deles, voam em quatro pedaços; um corpo pequeno só os faz sacudir. Um bloco
de prêmio batido por baixo com a cabeça dá uma moeda, ou o seu power-up, uma vez só -- um power-up
para cada jogador na fase, saindo um depois do outro, cada um cogumelo ou flor conforme o que o
jogador dele receberia; um que não mudaria nada para você não é pego, e segue para outra pessoa;
eles têm um só cubo de profundidade, no plano da própria fase. As moedas são contadas. **A sala
bônus**: na 1-1, o cano de três de altura com um degrau na frente leva para baixo -- fique em cima
dele e aperte S. Você afunda na boca dele e cai pelo cano, que continua pelo chão (com a frente
cortada, para a câmera seguir você para baixo) e fica pendurado no teto de uma sala de tijolos
azuis com três fileiras de moedas. Entre no cano deitado no fundo da sala e você sobe pela outra
metade dele, pelo chão de novo, e sai pelo cano seguinte da fase. O mastro no fim de uma fase para
o relógio e mostra o tempo e as moedas: o corpo se pendura nele, escorrega até a base, desce atrás
da base e anda sozinho (`core.walk`) para o fundo da imagem, até a porta do castelo que fica atrás
da passarela. A porta é um portal virado para a câmera: ela leva a uma porta no muro atrás do
começo da fase seguinte, de onde o corpo anda para a frente até o plano da fase, com as vidas, as
moedas e o tamanho que tem; a porta do último castelo leva de volta à sala. Uma fase fica nova
sempre que não há ninguém dentro dela.

A 1-2 é subterrânea: tijolos azuis embaixo dos pés e sobre a cabeça, com lâmpadas no teto, um muro
escuro atrás, fossos com plataformas dentro, um trecho baixo, três plataformas subindo, e para fora
ao ar livre para o mastro.

A 1-3 é um castelo: pedra cinza, lava nos fossos (tocá-la custa uma vida), e o que queima -- um
golpe, como o de uma criatura, que nenhuma bola de fogo apaga:

| | |
|---|---|
| brasa | salta da lava a cada 2,6 s, quase três blocos acima do piso |
| barra de fogo | quatro ou seis bolas em fila girando em volta de um bloco: passe quando ela apontar para outro lado |
| carvão | o que o chefe cospe: cai onde você estava |

No fim do castelo, depois da bandeira do meio e de um bloco de power-up, fica o salão da
**FORNALHA**: um fogão de ferro de três blocos de altura. Ela acorda quando alguém entra, anda na
direção do corpo mais próximo e cospe três carvões; depois o fogo dela se apaga por dois segundos e
meio (ela fica cinza-azulada) e ela fica parada. Tocá-la é um golpe e joga você para longe, a não
ser que você pouse na cabeça dela enquanto está apagada: isso tira 4 dos seus 16 de calor, joga
você para longe e a acende de novo, um pouco mais rápida. Uma bola de fogo tira 1, a qualquer
momento. Apagada de vez, ela vira fumaça e o portão atrás dela se abre para o último mastro.

## Como é feito

As fases ficam no plano z = 0,5 (com três células de profundidade, para parecerem sólidas), indo
para -x: a câmera fica do lado -z delas olhando para +z, o que põe o caminho à direita da imagem, e
a sala fica atrás da câmera, onde nunca aparece na imagem. A 1-2 começa 200 mais adiante no mesmo
plano. Tudo é uma lista de caixas em `mods/platformer/init.luau` (`ground`, `pipe`, `prizes`,
`stairs`, `coins`, `foe`, `finish`), escrita por um estágio do mapgen. Os blocos das fases dão um
pouco de luz própria: a câmera olha para as faces que o sol nunca alcança.

- **Os canos** são modelos feitos no Blender (`tools/make_platformer_models.py`): um cano é
  redondo e tem duas células de largura, então cada peça é o modelo de uma célula
  (`core.node_model`) construído em volta da parede +x daquela célula, e as outras células do cano
  têm um bloco que detém um corpo e não é visto. Cada peça tem uma gêmea que não detém nada: um
  poço por onde um corpo passa é feito delas enquanto ele está dentro. Um corpo afundando num cano
  é segurado por um lançamento a cada tick (`core.player_launch` nunca empurra um corpo para baixo:
  ele põe a velocidade em 0, no mínimo), depois cai; subindo, ele é lançado a cada tick.
- **O cogumelo, a flor e a bola de fogo** também são do Blender, no mesmo script.
- **As criaturas** são do Meshy: `tools/meshy_generate.py` (texto para 3D pela API do Meshy: uma
  prévia, um refinamento, o .glb em `assets/meshy/platformer`, com a linha que o pediu ao lado) e
  depois `tools/meshy_prop.py`, que dimensiona, simplifica e assa a textura de cada uma em
  `platformer_core`. Elas são rígidas -- sem clipes -- e são entidades do mod
  (`core.ecs.visible`), movidas pelo servidor; tocá-las e pousar nelas também é decidido lá, a
  partir das posições que os jogadores informam.
- **Imagens e sons** são escritos por `tools/make_platformer_assets.py`.

## O que ainda não existe

Veja `TODO.md`, "Platform game and arcade shooter (drafts)".
