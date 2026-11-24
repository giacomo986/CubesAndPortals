# O mundo das nuvens

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

Ilhas sem fim sobre um mar de nuvens: aldeias de homens-coruja, cristais do céu, correntes
ascendentes, e no fim da estrada do céu a Arraia das Nuvens, um chefe para um jogador ou vários.
Quem estiver lá quando ela cair recebe a asa-delta da tempestade, para levar embora. Um mod,
`mods/skies` (`side = both`): `layout.luau` o mundo, `init.luau` o terreno e o jogo no servidor,
e na máquina do jogador o mar de nuvens, as correntes, os golpes e a vida da Arraia. Os mods do
sandbox estão ligados (robô, ferramentas, arco, armaduras, asa-delta...), então sua bolsa e armas
vêm com você.

## Como chegar

O portão fica na borda sul da praça, em 69.5 21 26.5: um arco branco como uma nuvem com tábuas na
frente (`mods/gates`). Como o jogo de plataforma, está no arquivo de salas: começa quando alguém se
aproxima do portão e para um minuto depois que o último saiu. Você sai por uma porta na ilha de
casa, na origem, olhando para o norte ao longo da estrada do céu.

## O mundo

Uma célula de 56 por 56 por vez, cada uma função da semente e da célula (`layout.luau`):

* **Ilhas** na maioria das células, de 5 a 11 de largura, de 30 a 42 de altura; algumas ligadas por
  uma ponte à vizinha a leste ou ao norte (tábuas com três de largura, postes de tronco). Algumas
  têm uma árvore; muitas têm **cristais do céu** na grama, colhidos à mão.
* **A estrada do céu**: uma ilha em cada célula bem ao norte de casa, uma ponte de cada uma à
  seguinte, até as ilhas da Arraia, 420 ao norte.
* **Aldeias**: uma em cada três das ilhas largas tem duas cabanas (tábuas, troncos, telhado de lã)
  e três homens-coruja.
* **Correntes ascendentes** em boa parte das células, sobre os vãos, nunca sobre uma ilha: cada uma
  vaga devagar, 9 blocos em volta do seu lugar. Aparecem como baforadas azul-claras subindo em
  espiral, riscos de luz correndo para cima e um anel nas nuvens. Quem entra numa (a 3,2 do
  centro) é lançado para cima a 15,5 m/s até 58: de 24 a 58 em 3,5 s. Um jeito de voltar de uma
  queda, e com a asa-delta um jeito de subir.
* **O mar de nuvens** de 6 a 22: as nuvens do céu desenhadas uma segunda vez, mais baixas e quase
  inteiras (`core.cloud_sea`, nova no motor). Um corpo que cai abaixo de 18 é posto de volta onde
  estava por último (na ilha mais perto se ali não há mais nada) e perde 4.

Aqui não se cava nem se constrói; um cristal do céu é a única coisa que se pega.

## Os homens-coruja

Do Meshy (uma imagem de teste aprovada, imagem em 3D, caminhada e corrida do rig automático;
`tools/make_meshy_boss.py owlman` faz o resto: parado com a cabeça girando, aceno, golpe de asa,
planeio, ferido, morte). Andam pela sua ilha e de vez em quando planam até outra a até 75
(a animação "glide", num arco), e vivem lá por um tempo. Um clique direito num deles: ele se vira e
acena, diz a que distância estão as ilhas da Arraia e como as correntes ajudam; e troca:

| dá | recebe |
|---|---|
| 2 cristais do céu | uma asa-delta |
| 6 lingotes de ferro | 3 cristais do céu |
| 5 cristais do céu | um diamante |
| 3 cristais do céu | 16 tábuas |

Uma aldeia se enche quando alguém chega a até 80 e se esvazia quando ninguém está a até 130.

## A Arraia das Nuvens

12 m de uma ponta da asa à outra; 400 de vida, e 60% a mais para cada jogador a mais perto das suas
ilhas. Durante a luta, a vida dela é uma barra no alto da tela de quem estiver a até 160.

1. **No alto**: circula a 58 em volta das suas cinco ilhas, fora de alcance. A cada 6 s sopra uma
   rajada em alguém numa ponte (avisa um instante antes): uma bola de vento que se vê voando dela,
   e quem ainda estiver a até 3,5 de onde ela chega é empurrado a 12 m/s. Um clique direito num
   para-raios chama um raio sobre o para-raios e do para-raios para dentro dela (visível, com um
   clarão no céu): 12% da vida, e redemoinhos. Ela desce 3 a cada para-raios. O primeiro
   para-raios começa a luta.
2. **Embaixo**: com os quatro para-raios atingidos, ela cai na ilha central e mergulha em volta
   dela de 3 a 8 acima da grama. Agora as armas contam; ficar embaixo dela custa 3 por segundo e um
   empurrão (um anel de vento). A cada 12 s chama redemoinhos.
3. **A queda**: com um terço da vida ela sobe de novo, os para-raios se acendem de novo e as pontes
   caem em uns 20 s. Quem não tem asa-delta recebe uma: dois para-raios atingidos, alcançados
   planando do centro (as correntes entre as ilhas ajudam), acabam com ela.

Durante a luta **três homens-coruja chegam voando** e circulam as ilhas, atingindo o redemoinho
mais perto (um risco dourado) ou, quando ela está embaixo, bicando a Arraia.

Quando ela cai, todos por perto recebem a **asa-delta da tempestade**
(`glider:storm_glider`, `mods/glider`): uma asa-delta como a outra, e a cada 1,5 s no ar uma
rajada a levanta, então ela desce bem menos e vai bem mais longe. Um minuto depois as pontes e os
para-raios voltam. `/manta` diz quantas vezes você a derrotou. Ninguém a até 220 das suas ilhas por
um minuto: tudo volta ao começo.

## Como foi feito

A Arraia e o redemoinho vêm do Meshy (`tools/meshy_prop.py`, depois `tools/make_sky_creatures.py`:
três ossos, as asas batendo, um mergulho, a morte). No começo voavam com a cauda para a frente: o
importador deixa o objeto em quatérnions, onde uma rotação escrita em `rotation_euler` não é lida;
agora o script gira os próprios vértices. `tools/make_sky_sounds.py` faz a rajada, os chamados da
Arraia e o estalo dos redemoinhos; o trovão é o de `mods/weather`.

O mundo tem 64 de altura como toda sala: ilhas, nuvens e a Arraia ficam todas abaixo disso.

## Limites

* Não jogado à mão: as rajadas numa ponte, a luta embaixo, o voo na terceira fase, o empurrão da
  asa-delta da tempestade, o painel e as trocas de um homem-coruja com cliques de verdade.
* Uma tábua tirada de uma ponte que cai some na hora; não se vê ela caindo.
* `core.player_launch` aceita no máximo 16 m/s por eixo: uma corrente levanta a 15,5.
