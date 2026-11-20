# O percurso de parkour

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

Um percurso suspenso no céu, dez estações em fila ao longo de -x, cada uma um movimento do parkour
do motor (`core.parkour`, `core.rope`). Dois mods: `mods/parkour_core` (só servidor: o robô e como
ele se move) e `mods/parkour` (`side = both`: o percurso no servidor, a arma de corda no cliente).

## Como chegar lá

`tools/track.sh` (ou o menu de título) inicia o sandbox com suas salas; o sétimo portão da praça,
um andaime listrado de laranja e preto com uma escada de blocos cinza e uma corda pendurada, fica
em 16.5 21 52.5 (`mods/gates`). Você sai na praça do percurso, na frente da porta de volta; outra
porta de volta fica no alto da torre. Ao chegar, você recebe a arma de corda (uma só, não importa
quantas você tinha). Num mundo de sobrevivência, cada local de parkour tem um percurso próprio
(veja abaixo).

## As estações

Cada estação começa com uma plataforma laranja: quem fica em cima dela lê o que fazer, e quem cai
no vazio (abaixo de y 12) volta para a última plataforma tocada.

| | onde (x) | o quê | como |
|---|---|---|---|
| 1 | -22 .. -15 | um fosso de 8 de largura, um muro de tijolo à direita | corra, pule ao lado do muro, segure para a frente: 5 blocos pelo muro (um pulo correndo faz 5) |
| 2 | -35 .. -32 | um muro com um buraco de 1 de altura | corra e agache: um deslize |
| 3 | -50 .. -41 | um degrau de 2 de altura | de costas para o degrau, para trás + pulo: um mortal para trás |
| 4 | -60 .. -51 | dois muros a 2 um do outro, de 10 e 7 de altura | pule contra um muro e pule de novo: quiques até o alto do muro mais baixo, a 30 |
| 5 | -88 .. -71 | um fosso de 18 de largura sob uma viga de aço a 42 | arma de corda (direito) na viga, para a frente para balançar, pule para soltar do outro lado |
| 6 | -140 .. -103 | um fosso de 37 de largura sob duas vigas a 42, a 19 uma da outra | prenda a primeira, solte no alto do balanço, prenda a segunda antes de cair, solte sobre o ponto de pouso |
| 7 | -163 .. -153 | um muro de tijolo à direita e um atravessado no fim dele, sobre um fosso | corra pelo muro da direita; onde ele encontra o atravessado, pule: a corrida vira para aquele (um canto interno) e pousa depois do canto, o caminho segue 5 mais para +z |
| 8 | -217 .. -175 | duas vigas em ângulo: a primeira atravessada no caminho, a segunda ao longo dele, à esquerda | da primeira, olhe para a segunda; o balanço dela vira a linha para +z sobre o ponto de pouso, o caminho segue 26 mais para +z |
| 9 | -282 .. -218 | três vigas em S: atravessada, ao longo à esquerda, atravessada de novo | olhe cada vez para a próxima viga; pouso 21 mais para +z |
| 10 | -310 .. -275 | três vigas em U: atravessada, ao longo à esquerda, atravessada à direita | olhe cada vez para a próxima viga; a última leva você de volta para +x sobre o ponto de pouso, e o caminho segue para o oeste a partir dele, sob as vigas, 25 mais para +z |

A estação 6 foi testada na física do motor, buscando entre os tempos (corda depois do pulo,
segurar, soltar, segunda corda, segurar): 6011 de 22218 combinações pousam, todas usando as duas
vigas. Estação 7: pulando da borda de saída ou dois blocos antes, a corrida encontra o canto dentro
dos seus cinco blocos, vira no pulo e pousa em z +6; sem o pulo o corpo cai. O pulo pode vir até
1,5 bloco antes do canto (guardado por 0,4 s): o segundo toque pousa ao longo de 0,5 s de tempos,
137 de 400, onde antes tinha de vir depois que a corrida parava (0,33 s, 88 de 400; antes disso era
um chute para longe do muro da direita). A corrida só começa colada ao muro da direita. As estações
8 e 9 foram testadas do mesmo jeito, com o corpo olhando para a próxima viga (as teclas empurram o
balanço para onde se olha): 1392 de 8464 tempos pousam na 8, 337 de 4096 na 9 e 358 de 4096 na 10,
e nenhum deixando uma viga de fora.

## A torre e a Chave do Cume

A partir do caminho depois do U (plataforma 11) começa a torre: os cinco movimentos de novo, um
depois do outro e subindo, com as mesmas células das estações acima (então os mesmos pulos). Cada
estação tem a sua plataforma, e quem cai volta para a última tocada.

| | estação | piso |
|---|---|---|
| 1 | chaminé | 29 → 36 |
| 2 | corrida na parede | 36 |
| 3 | mortal para trás sobre o degrau | 36 → 38 |
| 4 | chaminé | 38 → 45 |
| 5 | deslize | 45 |
| 6 | corda, viga a 58 | 45 |
| 7 | chaminé | 45 → 52 |

No alto (x -452 .. -442, z 79), a plataforma do cume dá a **Chave do Cume** (`keys:parkour`,
mods/keys) a quem ainda não a carrega, e a porta de volta ao sandbox fica lá (o segundo portal do
mundo, trazido do ponto de pouso da corda).

## O percurso de um local de sobrevivência

Atrás de um local de sobrevivência (o `keys:level` de mods/keys) o percurso é do próprio local, no
espaço dele: as estações acima como peças, mais um pulo no plano, um pulo um bloco acima, três
pilares, uma plataforma de quique e lajes que cedem, embaralhados pela semente do local, 3 + d
delas, com vãos medidos pelo pulo do corpo (60% dele em d 1, 85% em d 10). A plataforma do cume
diz que o nível está concluído, e uma porta depois dela leva de volta ao começo, onde a saída
escondida se abriu. As duas vigas, o canto e as vigas em ângulo, em S e em U entram a partir de
d 7, com todas as peças.

## A arma de corda

Botão direito: atira a corda para onde você olha (até 40 blocos) e pendura você nela. Para a
frente e para trás impulsionam o balanço, agachar solta corda, correr recolhe, pular ou o botão
esquerdo soltam. A corda é desenhada como uma fileira de cubinhos laranja das mãos até o gancho.

## Como é feito

- O percurso é uma lista de caixas de células (`box`), escrita por um estágio do mapgen
  (`parkour:course`) coluna por coluna, cortada nas bordas de cada uma; nenhum terreno do motor
  (`core.mapgen_core(false)`).
- Texturas: `tools/make_parkour_textures.py`. A arma de corda na mão e o seu ícone:
  `tools/make_rope_gun.py` (Blender, assets/blender/rope_gun.blend).
- Os clipes do robô (corridas na parede para a direita e para a esquerda, o chute num muro, o
  deslize, o mortal para trás, o balanço na corda) estão em assets/blender/robot18.blend e foram
  acrescentados a `mods/robot/robot.gltf` com `tools/gltf_add_clips.py`.
