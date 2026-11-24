# Os backrooms

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

Um labirinto sem fim de salas que ninguém construiu, em três níveis empilhados, com portas raras
que levam de um nível a outro. O mundo é `mods/backrooms/init.luau` (servidor), mais
`backrooms_exit` (a saída) e `backrooms_hand` (a lanterna, servidor e cliente).

## Como chegar lá

`tools/track.sh` (ou o menu de título) inicia o sandbox com suas salas; o sexto portão da praça,
uma parede de papel de parede amarelo com uma lâmpada fluorescente em cima, fica em 16.5 21 28.5
(`mods/gates`). Você sai na sala da origem do Level 0, quatro passos à frente do portal de volta.
Um segundo portal de volta fica na mesma sala das Poolrooms, virado para +x: só quem subiu até lá
o encontra.

## Os três níveis

O mundo tem 64 células de altura; os níveis ficam um em cima do outro. F é a primeira célula de ar
acima do piso.

| nível | F | ar | materiais | luzes |
|---|---|---|---|---|
| Level 1 | 5 | 6 | concreto, lajes, poças | 2 lâmpadas quentes por sala, 25% das salas no escuro |
| Level 0 | 21 | 5 | papel de parede amarelo, carpete, forro rebaixado | 4 lâmpadas fluorescentes por sala, 16% das salas no escuro |
| Poolrooms | 37 | 9 | azulejos brancos, piscinas azuis | 5 lâmpadas por sala, nunca no escuro |

Os átrios sobem no vazio acima do seu nível: teto a 19 no Level 1, a 33 no Level 0, a 62 nas
Poolrooms.

O nome do nível aparece na tela quando você entra nele (`level_at`: pelos pés, y ≥ 33 Poolrooms,
≥ 17 Level 0, abaixo disso Level 1). Nada se cava e nada se constrói.

## Os níveis novos, por distrito

O mundo é dividido em distritos de 128 x 128 células. Em três distritos de cada dez um nível novo
fica no lugar de um dos três, na mesma altura, então escadas, buracos e portais o encontram do
mesmo jeito (`theme` em `mods/backrooms/init.luau`). Os distritos em volta da entrada e da saída,
e os locais de sobrevivência, têm sempre os níveis antigos.

| nível | no lugar de | como é |
|---|---|---|
| O hotel | Level 0 | carpete vermelho, damasco creme, uma lâmpada quente por sala, corredores longos cheios de portas que não abrem |
| A festa | Level 1 | paredes rosa, chão de confete, luzes rosa, nunca escuro, balões sob o teto, bolos nas mesas |

O nome aparece na tela quando você entra. Ninguém vive ainda nos níveis novos: as criaturas saem
só no carpete do Level 0 e nas lajes do Level 1.

## As backrooms profundas

Um mundo à parte (sala `backrooms_deep`, os mesmos mods sem `keys` e `backrooms_exit`): a porta
fica na sala (-6, 5) do Level 0, a 50 células da entrada, um vão vestido do rosa da festa (a
terceira abertura deste mundo, `--portal-to @backrooms_deep/0`). Os dois vãos da sala na origem
das backrooms profundas levam de volta.

Lá dois distritos de cada três são um nível novo, há mais portais (uma coluna de chunks em duas) e
eles não levam para cima ou para baixo: a cada dez minutos os locais de cada bloco de 8 x 8 regiões
(256 células de lado) são embaralhados e emparelhados, e os dois vãos de um local levam aos dois do
outro, cruzados. Passando por um vão você vai parar até 256 células adiante, em outro nível, e o
mesmo vão mais tarde leva a outro lugar.

## Como é gerado

Um estágio do mapgen (`backrooms:rooms`) por coluna de chunks (32×32). A planta é uma função só de
x e z, não da semente: os backrooms são os mesmos para todos, e o tick encontra os portais sem ter
visto o mapgen rodar (um mundo carregado do arquivo não roda nenhum).

- **Salas.** Uma grade de salas de 8 células. A parede entre duas salas existe ou não segundo um
  hash (Level 0 76%, Level 1 42%, Poolrooms 36%); se existe, sempre tem uma porta de 2 de largura,
  então toda sala alcança todas as outras.
- **Corredores.** Fileiras de 4 salas ao longo de x (ou de z) viram um corredor de 3 de largura e
  32 de comprimento, com os lados maciços, uma porta em cada lado onde a parede da grade tem a
  sua, uma luz no meio (Level 0 22%, Level 1 30%, Poolrooms 11%, lá um canal alagado).
- **Átrios.** De vez em quando um bloco de 2×2 salas é um espaço único tão alto quanto o teto dos
  átrios (Level 0 9%, Level 1 9%, Poolrooms 12% dos blocos): galerias de 3 de largura ao longo das
  paredes, um andar sobre o outro (um no Level 0 e no Level 1, dois nas Poolrooms), um parapeito na
  borda de dentro, um lance de escada ao longo de +x para subir a cada galeria (o primeiro do chão
  ao longo da galeria sul, o segundo da galeria oeste ao longo da norte), lâmpadas no teto e nos
  pisos das galerias, iluminando em cima e embaixo. Nas Poolrooms há uma piscina no meio. Nunca
  sobre salas com escadas, buracos ou portais.
- **Coisas numa sala.** Level 0: pilares; Level 1: poças e pilares; Poolrooms: piscinas e
  escorregadores.
- **Piscinas.** Quadradas (2 de profundidade), do tamanho da sala ou uma raia do comprimento da
  sala (3), canais nos corredores (2), e as que ficam sob os buracos. Cada uma tem um casco, uma
  escadinha de aço para sair no lado -x e, nas Poolrooms, uma borda de azulejos largos e luzes nas
  paredes debaixo d'água.
- **Escorregadores.** Um canal azul que desce célula a célula, feito de peças retas
  (`slide.gltf`) e curvas de um quarto de volta (`slide_turn_a.gltf` entrando pelo lado +x,
  `slide_turn_b.gltf` pelo lado -x; saem para a frente). O mod os coloca como uma lista de direções
  (`lay`): onde a direção muda ele põe uma curva, e devolve a célula onde o corpo é jogado na água.
  - Nas salas: uma torre de azulejos com a escadinha e quatro células de escorregador, reto até uma
    piscina ao longo do lado baixo da sala, ou com uma curva até uma piscina ao longo do lado oeste
    (metade e metade).
  - Nos átrios das Poolrooms: da primeira galeria, de uma plataforma ao lado da galeria leste, dez
    células para baixo ao longo do lado leste, uma curva, ao longo do lado sul, uma curva e para
    dentro da piscina do meio.

  Um escorregador é uma esteira rápida descendo (`core.node_conveyor` 5, 1.25, -1), não sólida: o
  motor leva o corpo para baixo dentro dela, sentado sobre a linha do leito (`chute_at`,
  `src/core/physics/aabb.cpp`), reto ou pelo quarto de círculo da curva (reconhecida pelo
  escorregador uma célula acima que entra pelo lado), de 2 m/s até 5, trazendo-o de volta ao meio
  do canal. A física do cliente faz isso como qualquer passo, então sem trancos; no fim do canal o
  corpo voa adiante com a velocidade que tem e cai na piscina, e o mod toca o tchibum. Pular faz
  sair. Uma queda dentro de um escorregador não conta. Na curva, o corpo e a vista giram com o
  canal; os outros veem quem desce sentado de frente ao longo do escorregador, para onde quer que
  olhe (o servidor o manda com a direção do canal). O jogador que desce faz o clipe `slide` do
  robô (sentado, pernas para a frente, braços para cima) e fica com o olho mais baixo, descendo
  devagar.
- **Escadas.** Uma caixa de escada sobe 16 células do Level 1 ao Level 0 e do Level 0 às
  Poolrooms: dois lances lado a lado ao longo de z (x 1..2 subindo para +z, x 5..6 para -z) com um
  patamar em cada ponta, degraus de meia célula que se sobem andando, o teto aberto e o poço
  murado até o piso de cima, lâmpadas nas paredes do poço a cada 4 células. O último lance sai por
  uma abertura de 2×4 no piso da sala de cima. Dá para andar por baixo dos lances.
- **Buracos.** Um buraco de 2×2 no piso das Poolrooms (para o Level 0) ou do Level 0 (para o
  Level 1), um poço murado, e embaixo uma piscina de 2 de profundidade: a água anula a queda
  (`session.cpp`, `fall_reset` num líquido), você chega com a vida cheia.
- **Portais.** Em uma coluna de chunks a cada três (`PORTAL_CHANCE`) uma sala tem uma porta
  iluminada com um portal de verdade, e a mesma sala de outro nível tem a outra ponta: você sai
  acima ou abaixo de onde entrou. A moldura é decorada como o nível para onde leva. Só abrem os
  pares a menos de 40 células de um jogador (`core.portal_open`, `core.portal_link`), no máximo 7:
  as aberturas são 16 e duas são do mundo.

Mais ou menos uma sala em doze tem escadas ou um buraco. A sala da origem e as salas dos portais
continuam sempre salas comuns. Escadas e patamares são do nível de onde partem: carpete com borda
de latão no Level 0, azulejos com borda azul nas Poolrooms, concreto com uma faixa amarela no
Level 1.

**Uma regra do estágio:** uma coluna escreve só dentro de si mesma, e as paredes nos lados altos
de um átrio ou de uma caixa de escada (x0 + 16, x0 + 8...) podem estar na coluna vizinha. Quem as
levanta é a sala vizinha, de quem elas são os lados baixos: a primeira versão dos átrios não tinha
a parede alta, e do átrio se via o vazio entre os níveis.

## Luz

A luz dos blocos vai de 15 na lâmpada e cai 1 por bloco; o motor a desenha linear, com um mínimo
de 15%. O mod:

- coloca uma lâmpada fluorescente ou uma lâmpada **apagada** onde uma sala está no escuro (dá
  para vê-las, mas não iluminam);
- faz as lâmpadas fluorescentes do Level 0 e as lâmpadas do Level 1 **piscarem** em volta de quem
  está lá: a cada 0.3 s, para cada jogador, com 35% de chance uma lâmpada acesa de uma sala próxima
  se apaga por 0.08-0.38 s, uma ou duas vezes. Cada mudança refaz a luz da célula
  (`core.set_block` com o mundo rodando): poucas por segundo;
- ilumina os corredores no meio, os poços das escadas e dos buracos, os átrios pelo teto e pelos
  pisos das galerias, as piscinas por debaixo d'água.

As criaturas só saem onde a luz é 7 ou menos, ou seja, nas salas no escuro.

## Quem vive lá

Duas criaturas, `core.register_mob`, saem onde a luz é 7 ou menos:

| criatura | onde | vida | velocidade | golpe | como morre |
|---|---|---|---|---|---|
| Bacteria | Level 0, no carpete | 30 | 3.4 | 4 | cambaleia e cai dura para a frente |
| Smiler | Level 1, nas lajes | 20 | 2.6 | 3 | fecha o sorriso e afunda no chão girando |

Clipes: idle, walk, run, attack, death. Nas Poolrooms não há ninguém. Ao morrer, uma Bacteria às
vezes deixa um filamento, um Smiler um dente: as peças da bússola da saída (veja abaixo).

## Sons

ElevenLabs, texto para som (`eleven_text_to_sound_v2`, 50 créditos cada, flow
`2zuOw1wXkyasR0eBFnRn`), convertidos em ogg mono com ffmpeg. O motor toca os sons das criaturas
pelo nome (`<criatura>_say`, `_hurt`, `_death`, `_attack`); o mod toca o zumbido e o tchibum
(`core.play_sound`).

| arquivo | prompt |
|---|---|
| `hum.ogg` | Fluorescent ceiling lights buzzing, steady electrical 60 Hz hum, empty office room, constant drone -- saiu com 1 s, repetido até 8 s com ffmpeg; toca sobre cada jogador do Level 0 a cada 8 s |
| `splash.ogg` | Person jumping into an indoor swimming pool, big splash, tiled hall echo |
| `bacteria_say.ogg` | Creepy creature clicking and wet gurgling breath, distorted, close-mic, short |
| `bacteria_hurt.ogg` | Monster pained screech, short sharp distorted shriek |
| `bacteria_death.ogg` | Creature dying, long fading distorted wail ending in a dry rattle, then a body thud on carpet |
| `bacteria_attack.ogg` | Fast whoosh of a lashing claw swipe with a raspy hiss |
| `smiler_say.ogg` | Low eerie whispering giggle in a dark concrete room, reverb, unsettling |
| `smiler_hurt.ogg` | Short distorted gasp with electrical crackle, glitchy |
| `smiler_death.ogg` | Ghostly laugh slowing down and pitching down into silence, electrical fizzle out |

## Refazer os assets

- **Texturas:** `tools/make_backrooms_textures.py` (PIL) escreve `textures/backrooms_*.png`.
- **Criaturas:** `tools/make_backrooms_creatures.py`, caixas no Blender com armadura e clipes,
  exportadas para `mods/backrooms/{bacteria,smiler}.gltf` e salvas em
  `assets/blender/backrooms.blend`. Do terminal:
  `flatpak run org.blender.Blender --background --python "$PWD/tools/make_backrooms_creatures.py"`
  (o caminho absoluto: o flatpak não parte da pasta do projeto). Num Blender aberto, pelo servidor
  MCP: rode o script sem `__main__` e chame `bacteria()` e `smiler()` depois de esvaziar a cena à
  mão (`read_homefile` na janela aberta perde o contexto).
- **Escorregador e escadinha:** `tools/make_backrooms_props.py`, em
  `mods/backrooms/{slide,ladder}.gltf` e `assets/blender/backrooms_props.blend`. A escadinha não é
  metálica: sem nada para refletir, o motor a desenhava preta.
- **Jogador:** o robô do sandbox, um link para `mods/robot`. O clipe `slide` está em
  `assets/blender/robot18.blend` (ação `slide`, a partir da pose de `drive_idle`), exportado sozinho
  e acrescentado a `mods/robot/robot.gltf` com `tools/gltf_add_clips.py`. O cliente o escolhe
  quando o corpo está dentro de um escorregador (`slides` em `src/client/main.cpp`, com
  `chute_at`), para si e para os outros.
- **Bússolas, filamento, dente:** `tools/make_held_models.py`
  (`ONLY="backrooms:compass_in,backrooms:compass_out,backrooms:filament,backrooms:smiler_tooth"`),
  modelos na mão em `mods/backrooms/*_held.gltf` e ícones em `textures/backrooms_*.png`. A caixa é
  um link para `mods/chest/chest.gltf`.

## O que falta

- As caixas só existem nas partes dos backrooms geradas daqui em diante: num mundo já salvo, as
  salas já visitadas continuam sem elas.
- As bússolas só são lidas clicando num bloco ao alcance (um clique com uma bússola na mão tem de
  poder abrir uma caixa); a lanterna não.
- A lanterna não tem som.
- Os escorregadores não são sólidos: no chão, você atravessa a lateral da última peça deles.
- Nenhum guarda-corpo em volta dos buracos e da abertura das escadas (dentro da abertura você cai
  no máximo 3, abaixo do limite de dano).
- As galerias dos átrios não têm portas para as salas vizinhas: só se sobe a elas pelas escadas de
  dentro.
- No Xephyr o cliente roda a 4-7 FPS e tudo fica em câmera lenta (andando a 1.3 m/s em vez de
  4.8): o escorregador parece lento pelo mesmo motivo.
- Só se volta ao sandbox pelas salas da origem do Level 0 e das Poolrooms.
- O chamado do Smiler dura meio segundo; nas Poolrooms não há som.

## Créditos

Código, blocos, texturas, modelos e animações: este projeto. Sons: gerados com ElevenLabs para
este projeto. O robô: `mods/robot`, deste projeto.

## As bússolas

Duas bússolas, e ninguém as dá de presente:

- **A da entrada** (`backrooms:compass_in`, mostrador amarelo): achada por acaso nas caixas. Uma
  caixa (`backrooms:chest`, o modelo de `mods/chest`) fica em mais ou menos uma sala vazia em
  quarenta (25 por mil das salas sem nada dentro, nos três níveis), encostada no canto +x +z. Ela
  é enchida na primeira vez que alguém a abre, segundo um hash de onde ela está: a bússola em duas
  caixas em cinco, a lanterna em uma em oito, e um ou dois filamentos (Level 0), dentes (Level 1)
  ou um ou outro (Poolrooms).
- **A da saída** (`backrooms:compass_out`, mostrador verde): feita na grade da bolsa com dois
  **filamentos** (`backrooms:filament`) e dois **dentes de Smiler** (`backrooms:smiler_tooth`), em
  qualquer ordem. Uma Bacteria deixa um filamento uma vez em duas, um Smiler um dente
  (`core.mob_drop`, 0 ou 1).

## A lanterna

`backrooms:torch`, rara: em uma caixa em oito. Clique direito com a lanterna na mão, para onde
quer que você olhe: um feixe de luz ao longo do olhar, até 14 blocos, atordoa por 4 s as Bacteria
e os Smilers que pega (`core.slow_mobs` a 5%: mal andam e não golpeiam); depois ela recarrega por
6 s. Um clique no vazio não chega ao servidor como `core:used`, então quem o pega é o mod
`backrooms_hand` (`side = both`): a metade dele no cliente reivindica o clique direito com a
lanterna na mão (`core:input`) e o manda ao servidor, que acende a lanterna e diz a todos os
clientes onde desenhar o feixe. O modelo é o da lanterna do laboratório.

## Ler as bússolas

Clique direito com uma delas na mão, em qualquer bloco (`core:used`), e uma frase diz onde fica a
entrada (a sala da origem do Level 0, na frente do portal) ou a saída (a sala de `backrooms_exit`
no Level 1): metros e o lado em relação a para onde você olha ("23 m, em frente à direita"). De
outro nível, ela diz quantos níveis acima ou abaixo e aponta o caminho mais próximo a até 16 salas:
escadas para subir, escadas ou um buraco para descer. Ao entrar, uma frase dá uma pista sobre as
caixas e as criaturas.

## A saída e a Chave da Saída

`mods/backrooms_exit`: no Level 1, na sala (12, -9) -- x 96..104, z -72..-64, a mais de cem blocos
da origem, a mesma para todos -- fica a saída que o engenheiro encontrou: uma sala com uma porta em
cada parede, placas verdes sobre as portas e, num pedestal no meio, a **Chave da Saída**
(`keys:backrooms`, mods/keys). Nada aponta para ela, só um rádio: para quem está no Level 1 ele
chia da direção da saída (`radio.ogg`, `tools/make_radio_sound.py`), mais vezes quanto mais perto,
e a cada 25 s uma mensagem diz se o sinal está fraco, claro ou forte. Descer ao Level 1 é um
checkpoint: quem morre lá volta para onde desceu.

## O nível de um local de sobrevivência

Atrás de um local de sobrevivência (o `keys:level` de mods/keys) os backrooms são os mesmos, mas o
nível é do local, no espaço dele: uma sala de partida no Level 0 com a saída escondida na parede
oeste sob uma lâmpada apagada, e uma sala de saída como a do mundo a 48 + 24 d de distância, no
Level 0 até d 3 e no escuro do Level 1 a partir de d 4. O rádio leva até lá; chegando, uma porta
verde leva de volta à sala de partida, onde a parede se abriu. A partir de d 5 as luzes em volta
do jogador se apagam de vez em quando por oito segundos.
