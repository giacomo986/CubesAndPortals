# O shooter de fliperama

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

Um shooter visto de cima, num caça, à maneira dos fliperamas dos anos oitenta: uma tela com o mar
passando embaixo, duas fases de dez ondas de aviões, power-ups, uma fortaleza voadora no fim da
primeira e um porta-aviões no fim da segunda, três vidas, uma pontuação e um recorde. Dois mods:
`mods/shmup_core` (só servidor: o mar, a ilha com o hangar, as paredes da tela, os modelos, e o
corpo -- o robô do sandbox, a pé) e `mods/shmup` (`side = both`: a partida no servidor; a
decolagem, a câmera, a paisagem passando, os tiros e os textos da tela na máquina do jogador).

## Como chegar lá

No jogo (o menu de título, ou `tools/track.sh`) o portão dele fica na borda sul da praça, em
56.5 21 26.5: um gabinete de fliperama preto (`mods/gates`). O mundo não é um dos que o servidor
inicia junto com ele: está no arquivo de salas, começa quando alguém chega perto do portão e para
um minuto depois que o último saiu; o portão é um portal entre salas que pega uma boca emprestada.
`tools/shmup.sh` inicia o sandbox só com este mundo, atrás da sua primeira abertura
(`tools/shmup.sh -- --room shmup` nasce dentro dele). De um jeito ou de outro você sai por uma
porta na parede do fundo de um hangar, a pé, olhando ao longo dele para quatro caças estacionados e
o mar além da frente aberta. A porta leva de volta.

Chegue perto de um caça e aperte **F**: ele rola pela pista, decola e sobe, e a câmera gira de
trás dele para cima. O primeiro caça no ar começa a partida; quem embarca durante uma partida
entra nela.

## A partida

| tecla | |
|---|---|
| F | ao lado de um caça estacionado: decolar. Voando: volta ao hangar (fora desta partida); E faz o mesmo |
| W, A, S, D | deslizam o caça para cima, para a esquerda, para baixo, para a direita: ele sempre aponta para o alto da tela |
| espaço | segurado, atira |

Os dardos, verde-oliva, descem retos (100 pontos). Os ziguezagueadores, laranja, balançam de um
lado para o outro e de vez em quando miram um tiro (150). Os ases, pretos com faixas vermelhas,
param no meio do caminho e atiram três de cada vez (400). A onda 5 é um bombardeiro cinza que
aguenta quarenta tiros (3000). Os aviões ficam nos 48 blocos do meio de uma tela de 80 de largura,
então um caça consegue alcançar todos eles.

Mais quatro tipos chegam a partir da onda 3. Os **blindados**, azul-aço com placas escuras (300,
seis tiros cada), vêm em fila de cinco, a meio segundo um do outro, por um de quatro trajetos:
balançando tela abaixo, descendo por um lado e subindo pelo outro, em laços, ou de um canto ao
outro; cada um mira um tiro de vez em quando, e a fila inteira abatida deixa um power-up. Os
**varredores**, verde-azulados (250, a partir da onda 4), cruzam de um lado ao outro soltando
tiros retos para baixo. Os **mergulhadores**, carmim (200, a partir da onda 6), descem um pouco e
depois vão para cima do caça mais próximo. As **canhoneiras**, um bombardeiro roxo (800, catorze
tiros, a partir da onda 7), param e lançam quatro tiros que giram.

De vez em quando, a partir da onda 2, passa uma ilhota com um **tanque** em cima (300, e um
power-up uma vez em duas): ele se vira para o caça mais próximo e atira; ele está no chão, então
voar por cima dele não custa nada.

Um avião abatido -- os deles, ou o seu e seus alas na sua última vida -- cai no mar girando e
soltando fumaça, e deixa um anel onde toca a água.

A onda 10 é a **fortaleza voadora** (20000): 40 de largura, seis torres (500 cada) que se viram
para o caça mais próximo, e um escudo -- o anel de luzes em volta dela. Enquanto o escudo está de
pé, tudo o que se atira nele é gasto nele; derrubado (a barra azul no alto da tela), ele fica
abaixado dez segundos, durante os quais as torres e o casco (a barra vermelha) podem ser
atingidos. Na metade, o casco também lança anéis de tiros. Ela cai, e cinco segundos depois começa
a segunda fase.

## A segunda fase

Ondas 11 a 20, mantendo power-ups, vidas e pontuação: quatro aviões a mais por onda que na
primeira fase, todos os tipos desde o começo, duas filas de blindados por onda e mais
canhoneiras; a onda 15 é um bombardeiro com duas filas de seis.

A onda 20, a última, é o **porta-aviões** (30000): um navio de 46 de comprimento no mar, com a
proa para o alto da tela, então voar por cima dele não custa nada. Ele não para de lançar aviões,
um a cada 1,7 segundo enquanto houver menos de doze no ar: cada um corre pelo convés, decola da
proa, sai pelo alto da tela e volta como qualquer outro avião do seu tipo (dá para atingi-lo no
convés). Quatro torres (500 cada) se viram para o caça mais próximo e a ilha dele atira cinco de
cada vez. Ele aguenta 500 tiros; na metade, lança um avião por segundo e atira sete. Ele afunda
pela popa, em chamas, e a partida está ganha: os caças saem pelo alto da tela mais rápido que
qualquer coisa nela, descem do mar para a pista e rolam de volta às suas vagas, e os pilotos estão
a pé no hangar de novo.

Um tiro ou um avião que toca você custa uma vida, e por dois segundos e meio depois nada toca você
(a ponta das asas pisca em azul-claro). Sem vidas, você volta para o hangar; quando todos estão
lá, a partida acaba e o recorde é guardado (`core.storage_set`).

## Vários jogadores

Todos voam as mesmas ondas, cada um com suas vidas, sua pontuação e seus power-ups. Para começar
juntos, cada um embarca num caça: a partida começa quando o primeiro está no ar, e o primeiro avião
dela chega um segundo e meio depois -- até lá os outros já subiram. Quem embarca depois entra na
partida como ela está. Quem é abatido espera no hangar até a partida acabar. Com outros voando,
uma luz azul-clara na cauda marca o seu próprio caça, e `PILOTOS n` aparece embaixo do número da
onda.

A partida fica mais difícil para cada piloto no ar além do primeiro:

| | cada piloto a mais | dois | três | quatro |
|---|---|---|---|---|
| quanto aguentam um avião, uma torre, o casco e o escudo (um dardo continua aguentando um tiro) | +60% | x1,6 | x2,2 | x2,8 |
| aviões numa onda, no mesmo tempo | +50% | x1,5 | x2 | x2,5 |
| a cadência de tiro de aviões e torres | +25% | x1,25 | x1,5 | x1,75 |

Cada vez, menos do que vale um piloto a mais: dois caças têm o dobro de canhões, mas o mesmo céu
para desviar. Uma onda é dimensionada quando começa e um avião quando entra, então alguém que
entra ou é abatido muda as próximas; a cadência de tiro acompanha quem está voando agora. Mais
aviões também quer dizer mais power-ups, que são de quem os alcança primeiro.

## Power-ups

São poucos: um avião abatido em quinze, uma fila inteira de blindados, uma canhoneira, um tanque
sim e outro não e o bombardeiro (dois) deixam um, que desce pela tela: voe por cima dele. O que
eles dão dura a partida toda, menos os três que piscam, que acabam.

| cor | | |
|---|---|---|
| verde | VELOCIDADE | 5 m/s mais rápido, três vezes |
| verde, piscando | TURBO | 10 m/s mais rápido por dez segundos |
| vermelho | POTÊNCIA | dois canhões, depois três, depois três atirando mais rápido |
| vermelho, piscando | TIRO RÁPIDO | tudo atira duas vezes mais rápido por dez segundos |
| roxo | MÍSSIL TELEGUIADO | um míssil a mais por salva (a cada 1,2 s), até quatro; cada um tira o equivalente a três tiros do avião mais próximo dele |
| amarelo | TIRO DIAGONAL | dois tiros inclinados, depois quatro |
| ciano, piscando | LASER | dez segundos (até trinta) de um raio no lugar dos canhões, atravessando todo avião acima do caça |
| branco | ALA | um aviãozinho ao lado que também atira, até dois |

Um que o caça já tem no máximo vale 500 pontos.

## Como é feito

O servidor guarda tudo o que decide: onde estão os aviões e os tiros, quem foi atingido. A máquina
do jogador fica sabendo de cada tiro uma vez, quando ele é disparado, e o faz voar sozinha até
saber que ele sumiu; mísseis, raios e a fortaleza, que não vão em linha reta, chegam a ela dez
vezes por segundo. Aviões, torres e alas são entidades do mod (`core.ecs.visible`); os modelos são
o avião e o bombardeiro do front com pinturas próprias e uma fortaleza feita para este jogo
(`tools/make_shmup_models.py`, Blender), e as imagens e os sons são de
`tools/make_shmup_assets.py`.

O campo fica parado sobre um mar vazio de blocos; o que passa embaixo dele -- cristas de onda,
ilhas, nuvens -- é desenhado pela máquina do jogador (`core.render.pass`), sem fim, cada ilha feita
a partir do seu número. A única ilha feita de blocos é a base, ao sul da borda de baixo da imagem.
A decolagem também é da máquina do jogador: o caça é um modelo desenhado ao longo de um trajeto e a
câmera é `core.fixed_view` pedida de novo a cada quadro, terminando exatamente onde
`core.top_view_at` a segura depois; só então o servidor transforma o corpo num caça
(`core.body_model`), dá a ele o voo (`core.body_movement`, que um power-up de velocidade muda) e o
põe sobre o campo. O alto da imagem é +z, então a direita é -x.

Para testar, `TEST_WAVE` e `TEST_DROPS` no começo da metade servidor de `mods/shmup` começam uma
partida na fortaleza (9) ou no porta-aviões (19) com trinta vidas e fazem todo avião deixar um
power-up.

## O que ainda não existe

Veja `TODO.md`, "Platform game and arcade shooter (drafts)".
