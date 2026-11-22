# As terras devastadas (arpg)

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

Um RPG de ação isométrico de apontar e clicar: uma cidade, três zonas de áreas e corredores, a
cripta sob os túmulos. Os mods:

- `arpg_core`: blocos, o corpo do jogador, as criaturas (o Açougueiro entre elas), o povo da
  cidade, o que um personagem veste e empunha (`gear_*.gltf`).
- `arpg_game`: o mapa, classes, habilidades, itens, mercadores, missões, materiais, a interface.
- `arpg_crypt`: os três pisos da cripta, o Guardião dos túmulos, a saída.
- `arpg_icons`: os ícones do inventário (servidos como ficheiros de um modelo, ver abaixo).

`tools/arpg.sh --fresh` inicia um sandbox e este mundo num servidor; entra-se pelo arco de pedra.

## Jogar

Botão esquerdo: andar, golpear, falar, apanhar. Botão direito: a habilidade escolhida com 1-4.
**Q** poção de vida, **R** poção de mana, **B** portal da cidade, **I** bolsa, **C** personagem,
**K** habilidades.

- **O cinto**: três casas junto aos globos, cada uma com a sua tecla e quantas restam, que acende
  ao ser usada; a da vida pulsa abaixo de um terço da vida. Poções e ouro apanham-se passando por
  cima (ou clicando o nome da poção); com o cinto cheio (8 de cada) uma mensagem avisa.
- **Itens**: cada um tem um ícone pintado, mais rico quanto mais raro: normal simples, mágico
  brilho azul, raro brilho amarelo e faísca, único moldura dourada com gemas vermelhas, conjunto
  moldura verde.
- **Materiais**: minério de ferro, couro, osso, pó arcano, rubi, fragmento de alma, essência de
  brasa, escama de dragão. Caem das criaturas, de barris, caixotes e urnas, dos baús, dos campeões
  (rubis), dos que têm nome e dos senhores (fragmentos de alma; as escamas de Asa-de-brasa),
  apanham-se passando por cima e ficam na bolsinha sob a bolsa (99 de cada). Hoje pedem-nos as
  missões; criar, encantar e melhorar virão.
- **Os mercadores** têm três separadores: COMPRAR, RECOMPRAR (as últimas doze coisas vendidas, ao
  preço recebido) e MISSÃO.
- **Missões**: o guarda do portão, o ferreiro, o joalheiro e o alquimista têm cada um uma cadeia,
  uma de cada vez, oferecida quando o personagem tem o nível: matar tantos de um tipo (ou
  campeões, ou os que têm nome), juntar tanto de um material (entregue), ou o Açougueiro. Um `!`
  sobre alguém é uma missão a aceitar, `?` uma a entregar; as aceites aparecem à esquerda. Uma
  missão paga ouro, experiência e um item pelo menos mágico ou raro.
- **Contornos**: a criatura sob o ponteiro tem uma borda vermelha (o povo da cidade, dourado
  pálido), a que está a ser combatida uma borda dourada que pulsa (`core.outline`,
  `core.cursor_creature`).
- **Missões de matar** contam todos os nomes de um tipo (um esqueleto é um arqueiro de ossos mais
  adiante); o painel lista-os (CONTAM). Um senhor já morto quando a sua missão é aceite volta ao
  covil.
- **O Açougueiro**: o senhor do Ermo cinzento antes do seu senhor, no seu covil (área b1, nível
  3), com quatro mortos: enorme, rápido, um cutelo que bate forte. Ao entrar no covil com ele
  vivo, um muro de luz vermelha fecha o caminho de volta (nem portal da cidade) até ele cair.
- **Armadilhas**: quase todas as áreas depois da primeira escondem uma, que dispara ao passar
  perto, uma vez até o mundo esvaziar: explosão de fogo, chão elétrico em manchas, onda de
  espigões em linha, névoa gelada (abranda a 40% por 4 s), nuvem de veneno, ou uma horda de
  quatro campeões. Nenhuma no covil de um senhor.
- **A cripta**: três pisos descendo escadas (meios degraus), cada um selado até os seus mortos
  caírem. O Guardião espera no terceiro; quando cai, quebra-se o selo a leste do piso e um
  corredor leva a uma sala com teto onde a câmara desce à altura dos olhos, com o portal para casa
  na parede leste.

## Modelos e ícones, e como refazê-los

Tudo do Meshy, depois Blender (`flatpak run org.blender.Blender --background --python ...`). As
fontes ficam em `assets/meshy/`. Ferramentas: `tools/make_arpg_icons.py` (ícones),
`tools/make_arpg_gear_meshy.py` (armas, escudos, orbes, o cutelo), `tools/make_arpg_armor_strips.py`
e `tools/make_arpg_armor_meshy.py` (peças vestidas, robô e cada espécie, ajustadas sobre
`assets/meshy/arpg/fit_ref/`), `tools/make_arpg_chests.py` (os baús das zonas e o do depósito), `tools/make_meshy_boss.py -- butcher|custode` (os chefes). Os
detalhes estão no [README em inglês](README.md).

## Limites conhecidos

Em [TODO.md](../../TODO.md) (Mods, "Arpg").
