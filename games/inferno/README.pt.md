# O Inferno

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

Um mundo de cavernas entre um piso e um teto de rocha-mãe, mares de lava no fundo, pedra luminosa
pendurada no teto e fortalezas de tijolo cujas pistas fazem de ponte sobre as cavernas. Sem sol: um
céu vermelho, e a única luz é a da lava e da pedra luminosa. A sala `inferno` de `tools/track.sh`,
alcançada pela moldura de obsidiana da praça ou por uma que um jogador constrói.

Os mods dele: `inferno` (o terreno, a lava, as criaturas, tudo o que está abaixo),
`inferno_heart` (a prova da Chave do Fogo), `inferno_track` (a estrada dos karts), `inferno_ash`
(`side = client`: cinza e brasas no ar) e `infernoportal` (em comum com o sandbox: as molduras). Os
mods de jogo do sandbox também estão ligados aqui, então o que um jogador carrega vale alguma coisa
aqui também.

## O terreno

| região | onde | o quê |
|---|---|---|
| escória | em todo o resto | cavernas de escória, minério de quartzo em uma célula a cada 90, areia de cinzas nas margens |
| campos de basalto | um ruído sobre x e z | basalto no lugar da escória, colunas do piso até o teto (até 30) |
| florestas de fungos | o mesmo ruído, a outra ponta | caules de 4 a 7 de altura sob chapéus luminosos de cinco de largura |
| desertos de cinza | um segundo ruído | areia de cinzas sobre todo piso, pedra luminosa metade das vezes |

Lava abaixo de y 12; ela escorre (uma fonte e quatro níveis) e queima 3 a cada meio segundo. De vez
em quando, uma cascata de lava saindo de uma parede. Fortalezas: a cada 96 blocos, uma pista de
tijolo de cinco de largura ao longo de x ou de z, dois trechos de 64 construídos a cada três, um baú
a cada 64 encostado no parapeito.

## O que ele dá

| de | dá | para |
|---|---|---|
| brutamontes | pepita de ouro | nove fazem um lingote (`furnace:gold_ingot`, as ferramentas dele) |
| espectro | lágrima de espectro | a resistência ao fogo do alambique (`mods/alchemy`): 3 minutos, lava e fogo não queimam |
| brasa | bastão de brasa | combustível; moído em 2 pós de brasa: com um lingote de ferro, um isqueiro; com um lingote de ouro e vidro, 2 lanternas de brasa (luz 15) |
| minério de quartzo | quartzo | quatro fazem um bloco de quartzo; o centro de um comparador (`mods/cinnabar`) |
| caule de fungo | 4 tábuas de fungo | duas tábuas, 4 gravetos |
| um baú de fortaleza | ouro, bastões, quartzo, lágrimas, obsidiana, ferro, às vezes lingotes de ouro, uma resistência ao fogo, um diamante | enchido na primeira vez que é aberto; um colocado por um jogador continua sendo dele |

Para destilar é preciso um frasco de água, e o Inferno não tem água: traga de cima.

## O fogo

Uma bola de fogo de uma criatura que estoura num bloco (evento do motor `core:burst`) acende um
fogo na célula na frente da face atingida: 3 s na rocha, 8 na madeira, nos fungos e coisas do
tipo; ele nunca se espalha, e queima 1 a cada meio segundo quem está nele. A resistência ao fogo o
mantém longe.

## Dois Infernos: o aberto e os níveis dos locais

O Inferno aberto é aquele para onde levam uma moldura de obsidiana e a praça: um mundo só para
todos, as cavernas, as fortalezas e o caminho 1:8, e o coração perto da chegada como a prova da
praça (sempre d 1). Um jogador que chega por um local de sobrevivência ganha, em vez disso, um
nível próprio, no espaço daquele local além de x 8192 (mods/keys), então dois locais nunca se
encontram e nenhum deles encontra o mundo aberto. As peças vêm da semente do local, uma ideia nova
de cada vez, a mais nova primeiro:

| d | as peças | braseiros |
|---|---|---|
| 1-2 | túneis simples | 2 |
| 3-4 | + uma ponte sobre um lago de lava, brutamontes nela, vãos de dois a partir de d 4 | 2 |
| 5-6 | + uma galeria de espectros em saliências; colunas de basalto subindo e descendo sobre a lava | 3 |
| 7-8 | + um braseiro no fundo de um poço de lava de seis de profundidade: precisa de resistência ao fogo | 3 |
| 9-10 | qualquer uma delas, e nada de pedra luminosa: só os braseiros iluminam o caminho | 4 |

Uma sala de partida com a saída escondida na parede oeste (uma lâmpada apagada em cima), as peças
para o leste, cada uma terminando num braseiro, depois o portão e o salão. Com todos os braseiros
acesos, o portão se abre; quem entra no salão volta para lá depois de morrer, e o guardião se
ergue, 15% mais resistente a cada passo de d, com d - 1 brutamontes e espectros. A morte dele é o
nível concluído para quem entrou no salão: a chave a partir de d 4, a saída se abre, e uma célula
do piso do salão se acende e os leva de volta à partida. Um jogador que chega a um nível onde não
há ninguém o encontra novo: braseiros apagados, portão fechado.

Num nível, uma moldura de obsidiana acende e não leva a lugar nenhum (além de x 8000 aqui, 64000 lá
em cima), e um baú de fortaleza dá menos: metade da chance, no máximo metade da quantidade, nunca
as coisas raras.

## O caminho de ida e o de volta (1:8)

Um bloco aqui vale oito no mundo de cima. Uma moldura de obsidiana acesa com o isqueiro no sandbox
muda para lá o caminho da praça, e o Inferno constrói a gêmea a um oitavo da distância, na altura
onde ela quebra menos. Uma moldura acesa aqui (a obsidiana do próprio Inferno também vale) muda
para ela o caminho de volta, e o sandbox constrói a gêmea oito vezes mais longe, no chão. Quebrada
uma moldura acesa, o lado dela volta para casa (a praça, ou a câmara de chegada em 32 30 32) e o
outro vai atrás. Cada lado ignora a abertura distante que volta para onde ele já corresponde.

## Sons e ar

Em volta de cada jogador, a cada 6 a 14 s, um ronco, lava borbulhando embaixo ou um gemido ao
longe; um fogo pegando (`tools/make_inferno_sounds.py`). Na máquina de cada jogador, 160 flocos de
cinza descem em volta do olho, e um em nove é uma brasa subindo.

## Arquivos

- `tools/make_inferno_textures.py`: as imagens dos blocos (`games/inferno/textures`), entre elas o
  caminho de volta dos níveis
- `tools/make_inferno_sounds.py`: os quatro sons
- `tools/make_held_models.py` (`ONLY=inferno:quartz,...`): os itens na mão e seus ícones

## Testado (2026-10-31)

Headless e no Xephyr: as três regiões vistas; um baú de fortaleza em 104 30 53 virado para a pista,
enchido com 9 pepitas, 2 bastões, 6 quartzos, obsidiana e ferro; um fogo numa ponte; 4 s na lava
com 20 de 20 sob resistência ao fogo; uma bola de fogo contra uma parede avisada uma vez com a sua
face (um teste unitário do motor); uma moldura acesa em 36 30 32 dando uma gêmea em 292 27 260, e
as duas em casa depois de ela ser quebrada. Não visto: uma bola de fogo de espectro acendendo um
fogo em jogo, uma moldura acesa no sandbox desde a mudança, os sons ouvidos.

Os níveis (mesmo dia), headless: d 1, 3, 5, 7 e 9 construídos, cada braseiro, portão, saída
escondida e caminho de volta onde planejado, o portão aberto assim que os braseiros ardem. No
Xephyr, um nível d 7 percorrido por script (o poço, a ponte, a galeria), o guardião erguido com a
escolta, morto pelo script, o piso aceso e pisado, e de volta à partida. Não testado: um nível
combatido à mão, uma moldura acesa num nível, um baú aberto num nível.
