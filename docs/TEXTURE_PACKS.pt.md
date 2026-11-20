# Pacotes de texturas

*[Read in English](TEXTURE_PACKS.md) · [Leggi in italiano](TEXTURE_PACKS.it.md)*

Um pacote é **uma pasta de imagens** e, quando preciso, uma ficha que diz qual imagem cada bloco
usa.

```sh
./build/src/client/cnp_client --textures assets/packs/kenney
```

Mais de um ao mesmo tempo, e **F7** passa para o próximo enquanto o jogo roda:

```sh
./build/src/client/cnp_client --textures assets/textures --textures assets/packs/kenney
```

`--no-textures` não seleciona nenhum: cada bloco fica com a textura provisória que o motor gera a
partir de `colour`/`checker`/`opacity`. É o estado contra o qual o teste mede todo o resto.

---

## Sem ficha

Uma pasta sem `pack.txt` é um pacote do mesmo jeito: seus arquivos têm o nome dos blocos, com os
dois-pontos trocados por um sublinhado.

    core:stone   ->  core_stone.png
    beacons:beacon -> beacons_beacon.png

É assim que é feita `assets/textures`, a que o repositório distribui, e ela nunca precisou de
ficha. **Uma tabela de apelidos vazia tem de se comportar como a regra antiga, não como um erro.**

## Uma face de cada vez

Um bloco pode usar **uma** imagem, ou uma em cima / uma embaixo / uma dos lados, ou seis
diferentes. Quem decide são **os arquivos que existem**, não uma declaração:

| arquivo | quem usa |
|---|---|
| `core_stone.png` | as seis faces |
| `core_grass_top.png` | a face de cima (+Y) |
| `core_grass_bottom.png` | a de baixo (−Y) |
| `core_grass_side.png` | as quatro verticais |
| `core_forno_front.png` | +Z |
| `core_forno_back.png` | −Z |
| `core_forno_right.png` | +X |
| `core_forno_left.png` | −X |

Cada face procura **o próprio arquivo, depois o do seu grupo, depois o arquivo simples do bloco**.
Então:

- um pacote que não acrescenta nada se comporta exatamente como antes;
- um pacote que acrescenta `core_grass_top.png` divide o bloco em dois sem que ninguém tenha
  declarado que a grama tem um topo;
- uma face sem o próprio arquivo e sem o do grupo fica com o arquivo simples, não com a textura
  provisória.

Uma declaração seria um segundo lugar onde a verdade mora, e os dois discordariam na primeira vez
que um pacote trouxesse um arquivo de que a tabela de nós não sabe nada.

**Um tile por imagem distinta, não por bloco.** Dois blocos que pedem o mesmo arquivo dividem um
tile; um bloco com topo, base e lados custa três. O teto é **256**, porque o índice do tile ocupa
oito bits no vértice: imagens que não cabem são contadas e avisadas, nunca dão a volta no índice
em silêncio.

Na ficha, a face é a terceira parte da chave:

```ini
core:grass:top    = grass_top.png
core:grass:side   = dirt_grass.png
core:grass:bottom = dirt.png
```

As palavras aceitas são `top`, `bottom`, `side`, `front`, `back`, `left`, `right` e `all` (que é o
arquivo simples). Uma chave é lida como face **só se ainda sobra um nome de bloco na frente dela**:
um mod que registra `mod:top` não tem o seu bloco lido como o topo de um bloco chamado `mod`.

## Como uma textura vai parar numa face

Uma textura tem um alto e uma esquerda; uma fatia de chunk não. A regra, para as seis faces:

> **o u da imagem vai para a direita olhando a face de fora, o v vai para baixo.**

Para baixo e não para cima, porque `v = 0` é a primeira linha na memória, que é a linha de cima do
arquivo.

| face | direita | baixo |
|---|---|---|
| +X | −Z | −Y |
| −X | +Z | −Y |
| +Z | +X | −Y |
| −Z | −X | −Y |
| +Y | +X | +Z |
| −Y | +X | −Z |

As quatro primeiras linhas são `cross(forward, up)` com `up` o do mundo: deduzidas, não escolhidas.
As duas últimas não têm um alto próprio, e a convenção é que as duas se leem com **+X à direita**.

O teste `pack_draw` põe um bloco sozinho no ar com um tile de quatro cantos de cores diferentes e o
fotografa pelas seis faces: vermelho em cima à esquerda, verde em cima à direita, azul embaixo à
esquerda, amarelo embaixo à direita. Cada um dos três jeitos de errar move um par diferente --
virada de cabeça para baixo troca vermelho e azul, espelhada troca vermelho e verde, girada 90°
manda o vermelho para um canto qualquer.

---

## Blocos direcionais

Seis faces diferentes não bastam sozinhas: um bloco cuja frente é sempre +Z dá as costas a três
jogadores em cada quatro. Um bloco declarado **`rotatable`** é girado ao ser colocado, e assim sua
frente fica virada para quem o colocou.

```lua
core.register_node("mymod:forno", 150, 110, 60, true, 0, true)
--                                                        ^ rotatable
```

No pacote:

```ini
mymod:forno        = pietra.png
mymod:forno:front  = bocca.png
```

**Um bloco com frente é um bloco só.** O giro é um byte na **célula** -- `Chunk` guarda um plano de
orientações ao lado do plano dos blocos e do da luz, ele viaja no mesmo frame comprimido e vai parar
no mesmo arquivo do mundo -- e o mesher o lê quando decide qual imagem vai em qual face. O pacote
não sabe de nada: pinta as faces pelo nome e só.

Até pouco tempo eram **quatro ids**, um por quarto de volta, e os três extras usavam as imagens do
primeiro permutadas. Não custavam tiles nem bytes na rede, mas custavam uma regra para cada quem
chamava: quem perguntava "que bloco é este" tinha de pedir a base. Agora o id no mundo é sempre o
que `register_node` devolveu, e `core.find_nodes` procura um nome e acha as células para onde quer
que estejam viradas.

O giro é decidido pela **posição** de quem coloca, não por onde está olhando: o servidor tem a
primeira e não a segunda, e uma regra contada de dois jeitos são dois jogos. A frente não pode
olhar para cima nem para baixo: quatro giros em torno da vertical, não vinte e quatro -- mas isso
agora é um limite do que a colocação escreve, não da tabela de blocos, e é por isso que valeu a
pena mudar.

---

## Com ficha: `pack.txt`

Ela é necessária no momento em que a arte é de outra pessoa. Nenhum pacote baixado chama os seus
arquivos de `core_stone.png`, e renomear oitenta e cinco arquivos antes de poder testá-los é o
motivo de ninguém testar nada.

```ini
# comentário; ; também serve
name = Kenney Voxel Pack

core:stone = stone.png
core:grass = grass_top.png
core:dirt  = dirt.png
```

- A chave **com dois-pontos** é um bloco -- ou um **item**: vale o mesmo nome com namespace, e a
  ficha não aprende que itens não são blocos. Sem dois-pontos é uma propriedade do pacote: hoje
  só `name` é lido, as outras são ignoradas.
- Um bloco que a ficha não nomeia fica com o próprio nome de arquivo. As duas regras convivem: dá
  para nomear metade de um pacote e deixar a outra metade se nomear sozinha.
- Dois blocos podem usar a mesma imagem.
- Uma linha recusada **não derruba as linhas depois dela**.

### O destino é um nome de arquivo, e só

Nada de `/`, nada de `\`, nada de `..`, nada de ponto no começo, e no máximo 128 caracteres. Uma
linha que quebra a regra é **contada e descartada**, e o bloco fica com o próprio nome -- não com
meio caminho.

Não é comodidade. Um pacote é conteúdo, conteúdo aqui também chega **pela rede**, e uma ficha que
pudesse nomear `../../../etc/passwd` é uma ficha que lê tudo o que o jogador pode ler. O teste
`texture_pack` tenta oito jeitos diferentes de sair da pasta, um por verificação, porque um parser
que para em `/` pega quatro e deixa os outros passarem.

---

## Vestir um ITEM

Uma ferramenta não coloca nada, então não tem um bloco do qual pegar uma imagem emprestada: sem uma
linha na ficha ela fica um quadradinho cinza na bolsa. A linha é como todas as outras.

```ini
tools:pick   = pick_iron.png
tools:shovel = shovel_iron.png
```

O item pede **um arquivo só**, com o próprio nome (`tools_pick.png` para `tools:pick`): não tem
faces, nem topo nem lado, e nenhuma cadeia para onde recuar. Se ninguém o desenhou, não recebe
nada -- e então a bolsa recorre ao **bloco que o item coloca**, que é o que faz uma pilha de pedra
parecer pedra sem ninguém desenhar um ícone para ela.

Os itens entram no atlas **depois** de todos os blocos, e isso não é um detalhe: querer uma imagem
para um item não pode mudar o tile de um bloco, porque os números dos tiles estão dentro de
vértices já construídos.

E dividem o mesmo limite de 256 e a mesma deduplicação: um item pintado com um arquivo que um bloco
já usa **não custa um segundo tile**.

## Um tile por IMAGEM, não por nome

Se a ficha manda seis blocos para o mesmo `flat.png`, o atlas pede seis nomes diferentes
(`core_stone.png`, `core_grass.png`, …) e recebe a mesma imagem seis vezes: custam **um tile**. A
deduplicação é sobre o conteúdo decodificado, não sobre o nome do arquivo, então também pega dois
arquivos diferentes com a mesma imagem.

Importa porque o atlas tem **256 tiles** -- o índice de um tile ocupa oito bits no vértice -- e
trinta e nove blocos vezes seis faces já são 234. Medido: o pacote do teste passou de seis tiles
para um.

---

## O tamanho do tile é do pacote

O atlas tem **um** tamanho de tile, e quem decide é o pacote: as imagens são decodificadas **antes**
de o atlas existir.

- Ganha o tamanho em que a maioria dos arquivos concorda; no empate, o maior. Não "o primeiro":
  qual arquivo vem primeiro é a ordem de registro, então com essa regra acrescentar qualquer mod
  mudaria a resolução do mundo inteiro.
- Um arquivo num tamanho que o resto do pacote não usa é **pulado, não redimensionado**, e o seu
  bloco volta à textura provisória. Isso é dito na linha de erro.
- Os tiles têm de ser quadrados, entre 4 e 512 px, e o atlas inteiro não passa de 4096 px: acima
  disso, volta para 32 px e avisa.
- As texturas provisórias também são geradas no tamanho escolhido, para que um pacote de 128 px não
  tenha meio mundo em 32.

Kenney em 128 px sobre 39 blocos:

```
textures: 'Kenney Voxel Pack' (1 of 1) from assets/packs/kenney -- 19 name(s) mapped, 0 refused
atlas: 8 x 8 tiles at 128px (1024px square), 19 from the pack, 20 generated
```

Os vinte gerados eram os `palette:hue*`, amostras de cor lisa que a ficha deixava de propósito como
estavam (desde 2026-10-30 eles têm imagens próprias em `assets/textures`).

---

## Pela rede

Um servidor pode usar um pacote (`cnp_server --textures <pasta>`) e a ficha é resolvida **do lado
que tem a pasta**. O nome que o cliente pede continua sempre o do bloco, `core_stone.png`: o
protocolo não aprende que pacotes existem.

Pelo mesmo motivo **F7 não mexe num mundo remoto**. Um mundo que outra pessoa está servindo é
desenhado com o que *aquele* servidor mandou; vesti-lo com um pacote desta máquina seria a imagem
do jogo errado, e pareceria certa.

### O pacote também viaja como pacote

Além das imagens com os nomes do motor, um servidor que usa um pacote serve **o próprio pacote**: a
ficha como `pack.manifest`, e cada arquivo que a ficha nomeia como `pack.file.<seu nome>`. O
cliente reconstrói a pasta no cache de mídia, carrega e a coloca na rotação do F7 -- assim você
pode **guardar** a arte de um mundo, usá-la em outro lugar ou tirá-la. O cliente avisa quando isso
acontece:

    pack: this world's 'a pack over the wire' kept -- 1 file(s), 2 name(s), F7 to wear it here or anywhere

A pasta leva o nome do **conteúdo da ficha**: o mesmo pacote vindo de dois mundos é uma pasta só, e
um pacote alterado é uma pasta nova em vez de metade de uma velha. Não custa nenhum blob -- a mídia
é endereçada pelo conteúdo, então um segundo nome para bytes que já estão lá é uma linha numa
tabela -- e não custa nenhuma versão de protocolo: um mundo sem pacote não oferece esse nome, e a
negociação para por aí.

Um arquivo que a ficha nomeia e que o servidor não consegue ler é contado e não é um erro: o atlas
do servidor também recorre ao nome seguinte, então um cliente sem esse arquivo se comporta do mesmo
jeito. Essa equivalência é a promessa.

---

## O que ainda não faz

- **Só um bloco distribuído é direcional.** `chest:box` é `rotatable`, e a ficha do Kenney pinta a
  sua frente (`chest:box:front = oven.png`). Todos os outros blocos do jogo padrão têm seis faces
  iguais ou no máximo um topo, uma base e um lado.
- **Só quatro giros, em torno da vertical.** Um bloco não pode olhar para cima nem para baixo. O
  byte na célula guardaria vinte e quatro: é a colocação que escreve só quatro.
- **Sem mipmaps e sem filtro linear.** O atlas é `GFX_FILTER_NEAREST`, e em 128 px isso aparece de
  longe mais do que aparecia em 32.
- **Sem animações de textura, sem variantes aleatórias por bloco.**
- **O pacote padrão não veste as ferramentas.** `assets/textures` é a arte gerada deste motor, e
  não tem `tools_pick.png` nem `tools_shovel.png`: com o pacote padrão as duas ferramentas são
  quadradinhos. O pacote Kenney as veste, porque a pasta `PNG/Items` do seu arquivo é desenhada em
  128 px como os tiles -- o mesmo tamanho, nada para redimensionar.
