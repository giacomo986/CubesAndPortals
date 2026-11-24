# O tower defense

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

Monstros dos outros mundos saem de uma fenda e andam até um cristal; você os detém com torres
feitas do que minerou, barricadas que alongam o caminho deles e as suas próprias armas. Sozinho ou
junto. Dois mods: `mods/defense_core` (o campo, a fenda, o cristal, as bancadas, a porta de casa) e
`mods/defense_game` (as ondas, as torres, as barricadas, os recordes). Os mods do sandbox estão
ligados (robô, ferramentas, minérios, arco, armaduras, asa-delta...), então sua bolsa, armas e
armadura vêm com você, e o que as ondas dão volta para casa com você.

## Como chegar

O portão fica na borda sul da praça, em 25.5 21 26.5: um trecho de muro de pedra com a luz de um
cristal sobre a passagem (`mods/gates`). Como o jogo de plataforma e o shooter, está no arquivo de
salas: começa quando alguém se aproxima do portão e para um minuto depois que o último saiu. Você
sai por uma porta no muro sul do campo, ao lado do cristal, olhando o campo na direção da fenda.

## O campo

49 de largura e 73 de comprimento, cercado por muros de tijolos de pedra com cinco de altura. Na
ponta norte a fenda, uma moldura de pedra preta com luz violeta dentro, por onde saem os monstros;
na ponta sul o cristal no pedestal, duas bancadas dos lados da porta. Sempre de dia: o relógio está
parado ao meio-dia.

## Como se joga

* **Início**: clique direito no cristal. Cada um recebe 24 barricadas (e na primeira vez de todas
  tábuas, ferro e pedregulhos, o bastante para uma torre de flechas), e a primeira onda vem em 30 s
  (um clique direito no cristal a manda na hora).
* **Ondas**: 5 + 2 por onda, metade a mais para cada jogador a mais, cada um 12% mais resistente que
  os da onda anterior, e 35% para cada jogador a mais. Zumbis e demônios; a partir da quarta onda
  também ciborgues. A cada quinta onda um chefe: o Guardião da arena, depois o Açougueiro da cripta,
  alternando. Um monstro no cristal tira 1 (um ciborgue 2, um chefe 10) dos seus 20; em 0 a partida
  acaba e o campo é esvaziado. Um monstro com um jogador a menos de 3,5 luta com ele por até 10
  s, e volta ao caminho quando ele está a 8; perto do cristal não luta com ninguém e vai direto
  para o cristal.
* **Entre as ondas**: 25 s, ou um clique direito no cristal. Cada um recebe ferro, tábuas,
  pedregulhos e 8 barricadas; a partir da segunda onda gelo, da terceira bastões de brasa, da
  quarta ouro e (uma onda sim, uma não) rubis, a cada quinta diamantes. São seus.
* **Barricadas**: colocadas na grama do campo entre o cristal e a fenda, com duas de altura; os
  monstros dão a volta. Uma que fecharia o caminho volta para a bolsa. Quebrada, devolve a si mesma.
* **Torres**: clique direito numa bancada, escolha uma; vai para a bolsa, e é colocada no campo
  como um bloco. Um clique direito numa mostra o nível e a melhora (até o 3). Quebrada, volta ao
  nível 1.
* **Mira**: as torres de flechas, raio e portal escolhem um monstro. No painel delas se escolhe
  qual: o mais perto (no começo), o mais longe, o mais adiantado rumo ao cristal, o mais atrasado,
  o mais forte ou o mais fraco (o com mais ou menos vida restante, contada pelo jogo a cada golpe);
  entre iguais, o mais perto. Braseiro e gelo atingem todos os monstros em volta.

| torre | custa | melhoria (para 2, para 3) | o que faz (níveis 1/2/3) |
|---|---|---|---|
| flechas | 4 tábuas, 2 ferro, 4 pedregulhos | 2 ferro, 4 ferro | o monstro mais perto a até 10/11/12: 5/7/10 a cada 0,8/0,65/0,5 s |
| braseiro | 6 pedregulhos, 2 bastões de brasa, 2 ferro | 2 bastões, 3 | todo monstro a até 3,5/4/4,5: 3/5/7 por segundo |
| gelo | 4 gelo, 2 ferro | 3 gelo, 5 | deixa todo monstro a até 6/7/8 a 55/45/35% do passo |
| raio | 2 ouro, 1 rubi, 2 ferro | 1 rubi, 2 | 9/12/16 a até 9/10/11, e em seguida em mais 3/4/5 a até 4 um do outro, três quartos a cada vez, a cada 1,6/1,4/1,2 s |
| portal | 1 diamante, 1 ouro, 4 pedregulhos | 1 diamante, 2 | manda de volta à fenda o monstro mais perto a até 5/6/7, a cada 10/8/6 s; nunca um chefe |

* **Recordes**: a melhor onda de cada jogador, e a do campo com quem a segurou. `/defense` ou um
  clique direito no cristal durante uma partida os mostra.
* Ninguém aqui por 20 s: a partida acaba e o campo é esvaziado.

## O que a máquina do jogador desenha

`defense_game` roda dos dois lados (`side = both`). O bloco sob uma torre é transparente
(`core.node_glass`): só para os corpos e recebe os cliques. A máquina de cada jogador desenha nele
o modelo da torre (`core.render.model`), um pouco maior a cada nível. Cada torre mira no monstro
mais perto dela. As duas que têm uma frente ficam sobre uma base que não se mexe: a besta e o anel
do portal giram sobre ela para o alvo, suavemente, a besta com um recuo quando atira e olhando em
volta devagar quando ninguém aparece por um tempo (`tools/make_turret_parts.py` dividiu os modelos
delas em dois). Braseiro, gelo e raio atingem tudo em volta e ficam parados. Cada uma tem uma luz sua: a chama do braseiro, o
brilho do cristal de gelo, a esfera vermelha do raio estalando, o redemoinho do portal. O que
atiram se vê chegando (`core.render.fx`): o risco de uma flecha e faíscas onde acerta, um anel de
fogo e chamas em cada monstro queimado, o gelo se espalhando no chão, o raio saltando de monstro em
monstro, um redemoinho onde um monstro é pego e outro na fenda por onde ele sai. O servidor informa
cada torre (`T`), cada uma tirada (`R`), o campo esvaziado (`C`) e cada disparo (`F`, a torre e os
pontos atingidos).

## Como foi feito

As torres vêm do Meshy: uma imagem de teste cada (texto em imagem), imagem em 3D (meshy-6), depois
`tools/meshy_prop.py --height 1.4`. Sobre cada torre uma célula sólida invisível, para nenhum
monstro subir nela e passar por cima de uma barricada. Os monstros são os do assalto (zumbi,
demônio, ciborgue), o Guardião da arena e o Açougueiro da cripta, copiados com seus sons; os sons
das torres são os do shooter.

Um monstro mandado ao fim do campo de uma vez (`core.mob_goto`) esgota a busca num campo cheio de
barricadas e para no meio; por isso o caminho é calculado uma vez (`core.find_path`, sempre que uma
barricada ou uma torre é posta ou tirada) como paradas a cada seis células, e cada monstro vai de
parada em parada.

## Limites

* Torres e barricadas vão com a partida: uma derrota esvazia o campo.
* A bancada de trabalho do sandbox (`mods/craft`) fica no campo em 39 20 47, afundada na grama.
