# As ilhas suspensas

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

Seis ilhas sobre um mar de nuvens, pontes entre elas, e a Arraia das Nuvens: um chefe para um
jogador ou vários, e a asa-delta da tempestade para quem estiver lá quando ela cair, para levar
embora. Um mod, `mods/skies` (`layout.luau` as ilhas e as pontes, `init.luau` o terreno e a luta).
Os mods do sandbox estão ligados (robô, ferramentas, arco, armaduras, asa-delta...), então sua bolsa
e armas vêm com você.

## Como chegar

O portão fica na borda sul da praça, em 69.5 21 26.5: um arco branco como uma nuvem com tábuas na
frente (`mods/gates`). Como o jogo de plataforma, está no arquivo de salas: começa quando alguém se
aproxima do portão e para um minuto depois que o último saiu. Você sai por uma porta na ilha de
casa, olhando para o norte, para as outras.

## As ilhas

| ilha | onde (x y z) | o quê |
|---|---|---|
| casa | 0 40 -92 | a porta, um suporte de asas-delta |
| sul | 0 40 -46 | um para-raios |
| centro | 0 40 0 | a maior (13), um suporte; onde se luta com a Arraia |
| norte, leste, oeste | 0 42 46, 46 38 0, -46 44 0 | um para-raios cada |

Pontes de tábuas, com três de largura, com postes de tronco a cada quatro: de casa ao sul, e do
centro a cada uma das quatro; as do norte e do oeste têm um buraco de dois para pular. Embaixo, a
14, as nuvens; um corpo que cai abaixo de 20 é posto de volta onde estava por último (na ilha mais
perto se aquela ponte não existe mais) e perde 4. Os suportes dão uma asa-delta (`mods/glider`) a
quem não tem.

## A Arraia das Nuvens

12 m de uma ponta da asa à outra; 400 de vida, e 60% a mais para cada jogador a mais.

1. **No alto**: circula a 58 em volta das ilhas, fora de alcance. A cada 6 s sopra uma rajada em
   alguém numa ponte (avisa um instante antes): empurrado a 12 m/s para longe dela. Um clique
   direito num para-raios chama um raio contra ela (12% da vida) e dois redemoinhos (um a mais por
   jogador); ela desce 3 a cada para-raios. O primeiro para-raios começa a luta.
2. **Embaixo**: com os quatro para-raios atingidos, ela cai na ilha central e mergulha em volta
   dela de 3 a 8 acima da grama. Agora pode ser atingida com armas; ficar embaixo dela custa 3 por
   segundo e um empurrão. A cada 12 s chama redemoinhos (um, e mais um a cada dois jogadores).
3. **A queda**: com um terço da vida ela sobe de novo, os para-raios se acendem de novo e as pontes
   caem em uns 20 s. Quem não tem asa-delta recebe uma: dois para-raios atingidos, alcançados
   planando do centro, acabam com ela (17% cada).

Quando ela cai, todos no mundo recebem a **asa-delta da tempestade** (`glider:storm_glider`): uma
asa-delta como a outra, e a cada 1,5 s no ar uma rajada a levanta (`core.player_launch`, 6 m/s para
cima), então ela desce bem menos e vai bem mais longe. Funciona em todo mundo que tem
`mods/glider`. Um minuto depois as pontes e os para-raios voltam e a Arraia circula de novo.
`/manta` diz quantas vezes você a derrotou. Ninguém aqui por 30 s: tudo volta ao começo.

## Como foi feito

A Arraia e o redemoinho vêm do Meshy (uma imagem de teste, imagem em 3D com meshy-6,
`tools/meshy_prop.py`); `tools/make_sky_creatures.py` dá a cada um três ossos (corpo e duas asas,
cada vértice ligado a uma asa conforme a distância do meio) e as animações: idle, walk e run (as
asas batendo), attack e shoot (um mergulho), death (as asas se fecham e ela vira).
`tools/make_sky_sounds.py` faz a rajada, os chamados da Arraia e o estalo dos redemoinhos; o trovão
é o de `mods/weather`.

A Arraia é um trabalhador que voa: o mod a põe onde deve estar a cada tick (`core.mob_place`), e os
golpes que ela leva fora da fase baixa são devolvidos.

O mundo tem 64 de altura como toda sala: ilhas, nuvens e a Arraia ficam todas abaixo disso.

## Limites

* Não jogado à mão: as rajadas, os buracos nas pontes, o voo na terceira fase e o empurrão da
  asa-delta da tempestade não foram testados com teclas de verdade (o empurrão é do servidor num
  corpo que o cliente move).
* Uma tábua tirada de uma ponte que cai some na hora; não se vê ela caindo.
