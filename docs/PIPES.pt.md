# Tubos

*[Read in English](PIPES.md) · [Leggi in italiano](PIPES.it.md)*

O guia completo do mod `pipes` (`mods/pipes/`): tubos que levam itens de um contêiner a outro, e
redes roteadas que os separam, guardam e entregam sob pedido. O modelo é o módulo Transportation
do clássico mod de tecnologia
([projectredwiki.com/wiki/Transportation](https://projectredwiki.com/wiki/Transportation)),
reduzido ao que importa.

A primeira parte é para quem joga, a segunda ([Como funciona por dentro](#como-funciona-por-dentro))
para quem quer saber exatamente o que acontece, ou escrever um mod que converse com ele.

---

## Em resumo

- Os **tubos** se ligam sozinhos aos tubos dos seis lados. Os itens viajam dentro deles como um
  cubinho luminoso.
- O **tubo de itens** é simples: uma pilha que tem para onde ir passa por ele no seu caminho; uma
  pilha perdida, onde três ou mais tubos se encontram, escolhe ao acaso.
- O **tubo de junção** é esperto: manda cada pilha pelo caminho mais curto até onde ela vai.
- O **tubo de interface** se liga a um contêiner (baú, fornalha…) e leva até quatro **chips**, que
  decidem o que sai e o que entra.
- O **tubo de pedidos** mostra numa grade de ícones o que a rede tem e o que ela sabe fabricar, e
  manda para você o que você pedir.
- A **bancada de trabalho automática** e a **fornalha automática**, blocos de nível 2 ao lado dos
  tubos, fabricam e fundem para a rede, em cadeia também.
- A **chave inglesa** gira uma interface ou um tubo de pedidos para o contêiner seguinte, como a
  chave de fenda.
- O **tubo firewall** não deixa passar os itens colocados nele.
- O **tubo de latência** deixa as coisas lentas e os caminhos o evitam; o **tubo válvula** só deixa
  passar enquanto o cinábrio o alimenta.
- O **tubo de fabricação** (e os seus **tubos de extensão**) dá a uma máquina receitas escolhidas
  num painel, sem precisar de um exemplar para nomeá-las.
- O **roteador de tubos** mostra como é uma rede.
- Mais chips iguais numa casa dão a uma interface uma **prioridade** maior.
- A cor do cubinho diz aonde ele vai: **azul** para um receptor, **roxo** para um terminador,
  **laranja** para um transbordo, **branco** pedido, **verde** perdido.

---

## Os tubos

Todo tubo é um núcleo com um braço para cada tubo vizinho (e, no tubo de interface e no de
pedidos, para o seu contêiner). A forma se atualiza sozinha quando um tubo ou um contêiner é
colocado ou tirado ao lado. Quebrar um tubo, seja qual for a forma, sempre devolve o mesmo item.
Eles quebram à mão em 0,3 segundo.

### São sólidos

Os tubos param os corpos: um jogador ou uma criatura bate num tubo em vez de atravessá-lo. Um tubo
tem 0,625 de bloco de altura (o núcleo vai de 0,375 a 0,625), mais que os 0,55 que se sobem
andando, então **um tubo no chão se pula**. Dá para ficar em pé em cima.

Só as caixas do tubo são sólidas: o resto da célula é ar, há espaço para passar ao lado, e **a luz
atravessa** (no motor só um cubo inteiro é opaco). Um item largado num tubo fica em cima dele; a
mira e o contorno seguem a forma do tubo.

### Tubo de itens

Nenhuma lógica. Leva pilhas de um tubo ao seguinte e nunca toca num contêiner. Uma pilha que está
indo para algum lugar **segue o seu caminho** também pelos cruzamentos de tubos de itens: o caminho
mais curto é calculado inteiro quando ela parte. Só uma pilha **perdida** (verde), onde três ou
mais tubos se encontram, segue por qualquer lado menos aquele de onde veio, ao acaso; num beco sem
saída, ela volta.

Serve em qualquer lugar. Uma junção é para onde uma pilha precisa poder **mudar de ideia**: achar o
caminho de novo se o antigo se rompeu, ou procurar um destino quando não tem nenhum.

### Tubo de junção

O tubo roteado. Uma pilha que chega a ele:

1. se já tem um destino que ainda alcança, parte pelo caminho mais curto até lá;
2. se não, procura um novo (veja [Aonde vai uma pilha](#aonde-vai-uma-pilha));
3. se não acha nenhum, fica **verde** e vaga.

É onde as pilhas perdidas acham um destino de novo.

### Tubo de interface

Uma junção ligada a **um** contêiner ao lado: o primeiro achado entre os seis lados, na ordem +x,
−x, +y, −y, +z, −z. Um **clique direito com a chave de fenda ou a chave inglesa** a gira para o
lado seguinte que tem um contêiner; o braço do tubo se vira para o escolhido. Se o contêiner sai,
o tubo passa para o primeiro outro que achar.

Um clique direito (sem um tubo na mão) abre as suas **quatro fileiras de nove casas**:

| casa 1 | casas 2–9 |
|---|---|
| o **chip** | o seu **filtro**: até oito itens, um por casa |

As quatro fileiras funcionam ao mesmo tempo e cada uma por conta própria. Uma fileira sem chip na
primeira casa não faz nada. Num filtro só conta *qual* item está lá, não quantos.

Quebrar a interface derruba os chips e os filtros no chão. As casas dela são "matéria-prima",
então nenhum extrator ou funil de outro mod tira os chips.

Um contêiner vizinho não pode ser um tubo: a interface e o firewall têm casas próprias, mas não
contam como contêiner de outra interface.

### Tubo de pedidos

Um clique direito abre o painel do **Tubo de pedidos**. Ele mostra tudo o que os **anunciadores**
da rede oferecem, o total de cada item somado em todos os contêineres, e tudo o que a rede **pode
fazer** com o que tem.

```
┌──────────────────────────────────────────────┐
│ Tubo de pedidos: 36 tipos, 391 no total      │
│ ┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐         │
│ │64││64││48││20││ 4││ 1││ 1││  ││  │  ← 9 × 4 │
│ └──┘└──┘└──┘└──┘└──┘└─*┘└──┘└─*┘└─*┘         │
│  …                                           │
│ [<] 1 / 4 [>]  [Por quantidade]  [Todos]     │
│ Buscar: [________________________] [Limpar]  │
│ Pedir: 8       [1] [8] [Pilha] [____]        │
└──────────────────────────────────────────────┘
```

`*` = a chave inglesa: dá para fabricar.

- **A grade**, no alto: **9 × 4** ícones como os da bolsa. O **nome** aparece embaixo do ponteiro.
  O número no canto de cima à esquerda é **quantos há** no estoque. Uma pequena **chave inglesa no
  canto de baixo à direita** quer dizer que a rede **sabe fabricar** aquilo com o que tem e com as
  suas [máquinas](#as-máquinas): sem máquinas não há nenhuma. Uma coisa que só pode ser fabricada
  não tem número. **Um clique** no ícone (ou na chave) a pede.
- **‹ / ›** e o número da página percorrem a grade, 36 por vez.
- **Por nome / Por quantidade** muda a ordem (por quantidade: primeiro o que você tem mais).
- **Todos / Em estoque / Fabricáveis** escolhe o que mostrar.
- **Buscar**: escreva parte do nome ou do id (`sand`, `ore:`, `planks`…) e aperte Enter;
  maiúsculas e minúsculas não importam. **Limpar** tira a busca.
- **Pedir**: quanto um clique pede. **1**, **8**, **Pilha** (uma pilha inteira daquela coisa: 64,
  16 ou 1, conforme o que for), ou um número escrito na caixa e confirmado com Enter (até 1024).

A ordem e a quantidade ficam guardadas quando você passa para outro tubo de pedidos também. O
painel fecha com Esc, ou sozinho se você se afastar mais de 8 blocos ou o tubo for quebrado.

**O que um clique faz.**

1. O que foi pedido é tirado dos anunciadores: primeiro do mais próximo, depois do seguinte.
2. Se o estoque não tem o bastante, o que falta **é feito pelas [máquinas](#as-máquinas) da
   rede**, em cadeia também (troncos em tábuas em gravetos). Uma receita que faz mais de um por
   vez pode mandar alguns a mais do que o pedido. Se não dá para fazer tudo, faz-se o máximo
   possível.

Tudo chega como um cubinho **branco**:

- no contêiner ao lado do tubo, se houver um (escolhido como na interface, com a chave de fenda ou
  a chave inglesa também);
- se não, cai **em cima do tubo**, para ser pego.

Se esse contêiner está cheio, o que não cabe parte de novo como qualquer outra pilha, para um
receptor, um terminador ou um transbordo. O aviso diz quantos partiram (**"N a caminho"**, **"N a
caminho, M sendo feitos"**) ou **"Nada para enviar"**. O que precisa ser feito chega quando as
máquinas terminam.

### Tubo firewall

Um clique direito abre **nove casas**: os itens postos ali **não podem entrar** no firewall. Todo o
resto passa por ele como por uma junção. Os caminhos são calculados em volta dele: uma pilha de um
item barrado dá a volta por outro caminho, se houver; se não, aquela parte da rede não existe para
ela. Os pedidos e os anunciadores também obedecem. Quebrá-lo derruba o que está nele.

---

### Tubo de latência

Um tubo de itens **lento**: uma pilha passa por ele a um terço da sua velocidade, e no cálculo dos
caminhos ele conta como **8 células**. Os caminhos o evitam quando há outro não muito mais longo:
um jeito de empurrar o tráfego para outro ramo, ou de deixar um trecho lento de propósito.

### Tubo válvula

Um tubo **aberto pelo cinábrio**: aberto enquanto algo de `mods/cinnabar` dá energia ao lado dele
(uma alavanca ligada, um botão apertado, uma tocha acesa, pó aceso, um repetidor ou comparador
ligado, um bloco de cinábrio), fechado se não.

- **Fechado**, nada passa, e os caminhos o tratam como uma parede: dão a volta, e um extrator não
  manda o que só poderia passar por ele.
- Uma pilha **já a caminho** através dele **para na frente** e espera que abra.
- **Aberto**, é um tubo como outro qualquer.

Com um relógio de cinábrio, é uma trava com tempo; com um comparador lendo um baú, uma torneira que
fecha quando o baú está cheio.

---

## As máquinas

Uma rede fabrica coisas **só com as suas máquinas**, dois blocos de **nível 2**: custam diamantes,
tubos de interface e um chip, e só se cavam com uma picareta de ferro ou melhor. Elas vão **ao lado
de qualquer tubo** da rede: o tubo estica um braço até a porta de latão da máquina.

| máquina | o que faz | quanto demora |
|---|---|---|
| **Bancada de trabalho automática** | as receitas da bancada de trabalho | 1 s por receita |
| **Fornalha automática** | o que uma fornalha funde, queimando combustível da rede (um carvão a cada 8 fundições, como a fornalha) | 2 s por coisa |

**Chips de receita.** Uma máquina **não faz nada sozinha**: ela faz só o que os **chips de
receita** ao lado dela dizem.

- **Onde vão:** numa fileira de um **tubo de interface que toca a máquina**. As coisas no filtro
  dessa fileira são as receitas dela: para a bancada, o que ela deve fazer; para a fornalha, o que
  ela deve obter fundindo (lingotes de ferro, vidro…).
- **Quantas receitas:** até 8 por fileira e 4 fileiras por interface, e uma máquina pode ter várias
  interfaces em volta.
- **O filtro:** cada receita pede um exemplar da coisa, como qualquer filtro.
- **Em cadeia:** **cada passo precisa do seu chip**. Para uma picareta de ferro feita a partir de
  troncos, a bancada precisa de tábuas, gravetos e a picareta, e a fornalha de lingotes de ferro.
  Sem tábuas no chip, não dá para fazer gravetos, nem a picareta.

A interface com os chips de receita costuma ser também o tubo que leva os ingredientes à máquina.

**Tubos de fabricação e de extensão.** No lugar de uma interface com chips de receita, um **tubo de
fabricação** pode ir ao lado da máquina:

- **As receitas dele:** um clique direito abre o seu painel, com as suas **9 receitas** no alto (um
  clique tira uma) e embaixo uma busca e uma grade do que a máquina sabe fazer (um clique adiciona).
  **Não é preciso nenhum exemplar para nomear uma receita**: ela é escolhida da lista.
- **Mais receitas:** cada **tubo de extensão** ligado ao tubo de fabricação, ou a uma extensão já
  ligada, adiciona mais 9, até 16 extensões. Um clique direito numa extensão abre o mesmo painel,
  com as receitas dela.
- **Como qualquer tubo**, o tubo de fabricação também leva os ingredientes.

**Recursivo.** O que você pede é planejado até chegar ao que o estoque tem, através de até **6
receitas em cadeia**. Para uma picareta de ferro, com um tronco, três ferros brutos e um carvão no
estoque, o plano é:

1. a fornalha funde **3 lingotes de ferro**, queimando o carvão;
2. a bancada faz **4 tábuas** com o tronco;
3. depois **4 gravetos** com 2 tábuas;
4. depois a **picareta** com 3 lingotes e 2 gravetos.

**Elas trabalham de verdade.**

- **Ingredientes:** saem dos anunciadores como cubinhos **brancos** e viajam pelos tubos até a
  máquina que os usa.
- **Trabalhando:** enquanto trabalha, uma máquina **se acende**: a lâmpada da bancada fica verde, a
  fornalha mostra o fogo e uma lâmpada âmbar.
- **Produtos intermediários:** vão para a máquina do passo seguinte (os lingotes da fornalha para a
  bancada). O produto final vai para o tubo de pedidos.
- **Conteúdo:** um clique direito numa máquina mostra o que chegou a ela e ainda não foi usado.

**Sobras.** As receitas muitas vezes fazem mais do que o necessário: um tronco dá 4 tábuas, mas só
2 são necessárias. Quando uma máquina fica parada por 5 segundos, o que sobrou nela volta para a
rede: para um receptor, um terminador ou um transbordo que aceite, se não para o estoque do
anunciador mais próximo.

**Várias máquinas.** Cada passo vai para a máquina mais próxima do tubo de pedidos que tem o chip
de receita daquele produto, contando o trabalho que ela já tem na fila. Cada máquina faz um passo
por vez, o primeiro da fila que já tem tudo. Duas máquinas do mesmo tipo dividem o trabalho de
vários pedidos.

**Quando dá errado.** Se um passo que não espera mais nada de outros passos fica sem os seus
ingredientes por **90 segundos**, com a máquina parada, o trabalho é abandonado e quem pediu é
avisado. O mesmo acontece se uma máquina é retirada. O que já tinha chegado às máquinas volta para
a rede como sobra.

---

## Os chips

Eles vão na primeira casa de uma fileira de um tubo de interface. Empilham até 16.

**Prioridade.** Quantos chips iguais há na casa é a **prioridade** daquela interface para ele.
Entre dois receptores (ou terminadores, ou transbordos) que aceitam uma coisa, vence o de
prioridade maior, mesmo que mais longe; com prioridade igual, o mais próximo. Os anunciadores
também: tira-se primeiro do que tem mais chips.

| chip | cor | o que faz | usa o filtro? |
|---|---|---|---|
| **Extrator** | — | uma vez por segundo manda até **8** do primeiro item do contêiner que tem para onde ir | sim: só esses itens; vazio = tudo |
| **Receptor** | azul, rápido | aceita os itens do seu filtro | sim: vazio = nada |
| **Receptor dinâmico** | azul, rápido | aceita os itens que o contêiner **já tem** | não |
| **Terminador** | roxo, lento | aceita os itens do seu filtro, mas só quando **nenhum receptor** tem espaço | sim: vazio = nada |
| **Transbordo** | laranja, o mais lento | aceita **qualquer coisa** que ninguém mais aceita | não |
| **Anunciador** | — | oferece o conteúdo do contêiner aos tubos de pedidos e aos chips de estoque | não |
| **Estoque** | branco (chegando) | mantém no contêiner **uma pilha cheia** de cada item do filtro, pedida aos anunciadores | sim: é a lista |
| **Receita** | — | numa interface que toca uma [máquina](#as-máquinas): o que essa máquina sabe fazer | sim: são as receitas |

O **chip vazio** não faz nada: é dele que todos os outros são feitos.

### Extrator

A cada segundo ele olha as casas do contêiner em ordem (até 54) e, para o primeiro item que passa
no filtro **e tem um destino** na rede, tira até 8 e os manda. Um item sem para onde ir é pulado,
então um baú misturado não trava numa coisa que ninguém quer. Ele nunca manda para o próprio
contêiner.

Ele só tira das casas de "estoque" e de "produto": tudo de um baú, só o produto pronto de uma
fornalha (nem o combustível, nem o que está no fogo). Várias fileiras de extrator numa interface
mandam várias pilhas por segundo.

### Receptores, terminador, transbordo

São as três **classes** de destino, em ordem de prioridade: 1 receptor (o dinâmico também),
2 terminador, 3 transbordo. Uma interface só aceita coisas enquanto o seu contêiner **não está
cheio**; com vários chips, vale a melhor classe entre os que aceitam o item.

O **receptor dinâmico** olha o contêiner inteiro: assim que há um de um item, o resto vem para ele.
Prático para um estoque que se organiza sozinho: ponha o primeiro à mão.

### Anunciador

Faz do contêiner um estoque que a rede enxerga. Ele oferece o que está nas casas de "estoque" e de
"produto" (de uma fornalha, o produto pronto). Com vários anunciadores, tira-se primeiro do mais
próximo de quem pede.

### Estoque

Uma vez por segundo, para cada item do filtro: se o contêiner tem menos de uma pilha cheia dele
(contando o que já está a caminho), pede a diferença aos anunciadores. Com areia no filtro, por
exemplo, o baú sempre tem 64 de areia enquanto a rede tiver.

---

## O roteador de tubos

Uma ferramenta (vidro, cinábrio, ouro, um chip vazio e ferro). Um **clique direito** com o
roteador num tubo ou numa máquina abre uma página sobre a sua rede:

- quantos tubos, nós e ligações ela tem, e de que tipos são os tubos;
- os chips nas interfaces;
- as válvulas abertas e fechadas, e os firewalls;
- cada máquina, com quantas receitas conhece e quantos passos tem para fazer;
- quantas coisas e quantas pilhas estão a caminho.

---

## Aonde vai uma pilha

Quando uma pilha procura um destino (ao sair de um extrator, ou numa junção quando não tem
nenhum), as interfaces da rede são olhadas **da mais próxima à mais distante** (os caminhos mais
curtos a partir dali já estão calculados, veja [O grafo](#o-grafo)). Vence:

1. o **receptor mais próximo** que aceita o item;
2. se não há nenhum, o **terminador mais próximo**;
3. se não há nenhum, o **transbordo mais próximo**;
4. se não, nada: a pilha fica **verde** e vaga.

"Mais próximo" é em tubos percorridos, não em linha reta, e os firewalls que barram o item são
paredes.

A pilha segue o caminho achado. Se na chegada o contêiner afinal não tem espaço (outra coisa o
encheu nesse meio-tempo), o que sobrou **procura de novo a partir dali**, deixando de fora as
interfaces que já a recusaram. Se o caminho se rompe (um tubo tirado, um firewall novo), a pilha
segue como pode, e na próxima junção calcula de novo um caminho até o mesmo destino.

Uma pilha que chega à borda de uma parte do mundo que não está carregada **espera ali** até ela
voltar à memória, em vez de achar que o ar é o fim do tubo e cair.

### Pilhas perdidas

Uma pilha verde vaga ao acaso, mas em cada tubo roteado tenta de novo achar um destino: assim que
alguém tem espaço, ela vai para lá. Uma pilha que está viajando há mais de **um minuto** e se vê
sem caminho **cai** onde está (em cima do tubo). Uma pilha cujo tubo à frente foi quebrado cai na
célula de onde veio.

### Velocidades

Em blocos por segundo: receptor **6**, pedido **4**, terminador **2,5**, perdido **2**, transbordo
**1,2**. A cor e a velocidade mudam quando o destino muda.

---

## Receitas

Cinábrio no lugar da redstone do original, corantes no lugar do illumar colorido dele.

| item | quantos | receita (fileiras da grade 3×3) |
|---|---|---|
| Tubo de itens | 16 | pedra · vidro · pedra |
| Tubo de junção | 16 | corante vermelho · cinábrio · corante verde / diamante · vidro · diamante / corante verde · cinábrio · corante vermelho |
| Tubo de interface | 1 | cinábrio · ouro · cinábrio / ouro · tubo de junção · ouro / cinábrio · ouro · cinábrio |
| Tubo de pedidos | 1 | cinábrio · tubo de junção · diamante |
| Tubo firewall | 1 | pedregulho · tubo de junção · cinábrio |
| Chip vazio | 1 | lingote de ouro · papel · cinábrio |
| Bancada de trabalho automática | 1 | diamante · chip de estoque · diamante / tubo de interface · bancada de trabalho · tubo de interface / ferro · ouro · ferro |
| Fornalha automática | 1 | diamante · chip extrator · diamante / tubo de interface · fornalha · tubo de interface / ferro · cinábrio · ferro |
| Tubo de latência | 8 | oito tubos de itens em volta de areia |
| Tubo válvula | 1 | — · cinábrio · — / ferro · tubo de itens · ferro |
| Tubo de fabricação | 1 | cinábrio · bancada de trabalho · cinábrio / ouro · tubo de junção · ouro / cinábrio · ouro · cinábrio |
| Tubo de extensão | 1 | papel · tubo de junção · papel |
| Roteador de tubos | 1 | cinábrio · vidro · cinábrio / ouro · chip vazio · ouro / — · ferro · — |
| Chave inglesa | 1 | ferro · — · ferro / — · ferro · — / — · ferro · — (lingotes de ferro) |

Todos os outros chips: **corante · cinábrio · corante / X · chip vazio · X / corante · cinábrio ·
corante**, com:

| chip | corante | X |
|---|---|---|
| Receptor | laranja | lingote de ferro |
| Receptor dinâmico | ciano | corante laranja |
| Transbordo | verde | lingote de ferro |
| Terminador | roxo | corante cinza |
| Extrator | ciano | lingote de ferro |
| Anunciador | magenta | lingote de ouro |
| Estoque | azul | diamante |
| Receita | amarelo | bancada de trabalho |

---

## Montagens prontas

**Esvaziar um baú em outro.** Baú A — interface com um *extrator* — tubos — interface com um
*transbordo* — baú B.

**Separar.** Um baú de entrada com um *extrator*; ao longo do tubo, um baú para cada material com
um *receptor* que tem esse material no filtro; no fim um *transbordo* para o resto. Junções em
cada cruzamento.

**Um estoque a que se pede.** Vários baús, cada um com uma interface *anunciador* (e um *receptor
dinâmico*, para o que entra voltar aonde já está). Um *tubo de pedidos* ao lado do baú de onde você
trabalha.

**Abastecimento automático.** Ao lado da fornalha ou da bancada, um baú com uma interface de
*estoque* filtrada em carvão, areia… A rede o mantém cheio a partir dos anunciadores.

**Fornalha automática.** Uma interface com um *extrator* na fornalha leva embora só o produto
pronto e o manda para os receptores.

### A demonstração na praça

Na praça, a leste do spawn (x 65–77, z 45–67), há três redes funcionando com placas que explicam
cada parte:

- **Separar com tubos**: pedregulho para um receptor (azul), obrigado a dar a volta por um firewall
  que o nomeia; terra para um terminador (roxo); tábuas, que ninguém pede, para um transbordo
  (laranja).
- **Pedidos com tubos**: um baú com um anunciador, um tubo de pedidos, um chip de estoque mantendo
  uma pilha de areia.
- **Fabricar com máquinas** (z 60–67): um estoque de troncos, ferro bruto, carvão, pedregulho e
  areia; uma bancada de trabalho automática com um chip de receita para tábuas, gravetos, picareta
  de ferro e bancada de trabalho; uma fornalha automática com um chip de receita para lingotes de
  ferro, vidro, pedra e carvão vegetal. Peça uma picareta de ferro ao tubo de pedidos: a fornalha
  funde os lingotes enquanto a bancada faz tábuas e gravetos, depois a picareta chega ao baú ao
  lado do tubo. As máquinas estão viradas para o spawn, então dá para vê-las se acendendo.

A cada 15 segundos os suprimentos são completados e os baús finais esvaziados. Uma placa de pressão
monta tudo de novo como estava.

---

## Limites

- **As pilhas em trânsito sobrevivem a um reinício**: são salvas a cada 3 segundos e postas de
  volta nos tubos onde estavam. O que partiu de um contêiner nos últimos 3 segundos antes da parada
  ainda estava nesse contêiner quando o mundo foi salvo.
- **Os trabalhos das máquinas não continuam** depois de um reinício, mas **nada se perde**: o que já
  tinha sido levado às máquinas volta para o estoque assim que elas se veem paradas, e os produtos
  já a caminho chegam. O pedido precisa ser feito de novo.
- Uma rede tem no máximo **65536 tubos** no seu grafo: numa maior, o que está além não é achado.
- Uma rede funciona onde o mundo está carregado. Onde há pilhas a caminho ou máquinas com trabalho
  a fazer, o mundo **fica carregado** até terminarem, sem ninguém por perto. Mas uma rede parada
  longe de todos não acorda sozinha: os chips são um ABM, como toda máquina do jogo.
- As coisas só são feitas com máquinas, só com o que os anunciadores oferecem e com até 6 receitas
  em cadeia. Um ingrediente que deixa algo para trás (o balde de um balde de leite) o deixa na
  máquina, que o manda de volta para o estoque.

---

## Como funciona por dentro

### Blocos e formas

Cada um dos nove tipos é registrado como **64 blocos**, um por combinação dos seis braços:
`pipes:<tipo>` (nenhum braço) e `pipes:<tipo>_<máscara>`, com o bit `d−1` da máscara para a
direção `d` na ordem +x, −x, +y, −y, +z, −z. As formas são `core.node_box`: o núcleo
`[0,375 … 0,625]³` mais um braço até a borda da célula para cada bit. Toda forma faz `node_drop`
como a forma 0.

Os blocos são `solid = true`: o motor para os corpos nas caixas e não na célula, e um bloco com
caixas nunca é opaco, então a luz atravessa. As pilhas em trânsito são entidades ECS do próprio
mod, movidas à mão: a solidez não as afeta.

`refresh(célula)` calcula a máscara de novo (tubos vizinhos, mais o lado do contêiner no tubo de
interface e no de pedidos) e troca o bloco se precisar; `core:placed` e `core:broken` o chamam na
célula e nas seis vizinhas. O lado do contêiner fica no meta `side` da célula.

### Uma pilha em trânsito

Uma pilha (`Stack`) tem o item, quantos, a célula de onde vem e a para onde vai, o progresso `t`
entre as duas, a velocidade, o caminho (uma lista de células) ou nenhum, o destino como uma chave
`"x,y,z"`, a idade e as interfaces que já a recusaram. A cada `core:tick` ela anda
`velocidade × dt` células; cada vez que `t` chega a 1 ela entra na célula seguinte (`arrive`), até
4 células por tick.

Ao chegar numa célula:

1. ela não é mais um tubo → a pilha cai na célula anterior;
2. a pilha tem mais de 60 s e nenhum caminho → cai ali;
3. é o destino → `deliver`: `container_add` no contêiner da interface (ou, num pedido sem
   contêiner, `drop_item`); o resto marca a célula como recusada e procura de novo;
4. a pilha tem um caminho e a célula seguinte ainda é passável → segue por ele, também pelos
   cruzamentos de tubos de itens;
5. se não: num tubo roteado `reroute` (primeiro para o destino antigo, depois um novo com `route`,
   depois verde), num tubo de itens `wander`.

### O grafo

Uma rede não é percorrida célula por célula cada vez que uma pilha precisa de um caminho: ela é
reduzida uma vez a um **grafo**.

- **Nós**: as células onde algo é decidido. São todos os tubos roteados (junção, interface,
  pedidos, firewall) e todo tubo de itens que não está no meio de um trecho: três vizinhos ou
  mais, ou um (uma ponta), ou nenhum.
- **Arestas**: os trechos de tubo de itens entre dois nós, com as suas células e o comprimento.
  Uma rede de mil tubos vira algumas dezenas de nós.

`build` o monta com uma busca em largura (até `MAX_NET` = 65536 células).
`ways_of(rede, nó, item)` roda **Dijkstra** a partir de um nó no grafo, com o peso de uma aresta
igual ao seu comprimento em células. Ele dá a distância, a aresta e o nó anterior de cada nó, e as
interfaces em ordem de distância. O resultado fica **guardado** na rede, uma vez para cada
conjunto de firewalls que barram o item (em geral nenhum, então um por nó de partida; no máximo
`MAX_ROUTES` = 4096, depois recomeça). `way` transforma o resultado guardado na lista de células
que a pilha percorre.

`route` percorre as interfaces em ordem de distância: para no primeiro receptor e lembra o
primeiro terminador e o primeiro transbordo. `class_of` dá a classe de uma interface para um item
(nil se o contêiner está cheio, `container_fill ≥ 1`). A escolha entre interfaces é feita na hora,
porque chips, filtros e espaço livre mudam o tempo todo. O grafo guarda só o que muda quando os
tubos mudam.

**Quando é atualizado.** Toda célula da rede, e toda célula ao lado de uma, é observada com
`core.watch_cell`, que avisa qualquer escrita (de um jogador, de `core.set_block` de um mod, de um
pistão…) como `core:watched`. Se uma célula da rede muda de *tipo* de tubo ou deixa de ser um tubo,
ou se aparece um tubo numa célula ao lado, o grafo é **jogado fora**. Um tubo que só muda de forma
é o mesmo tubo e não conta. O grafo novo é montado quando for pedido de novo, não na hora, então
cem mudanças seguidas custam uma montagem. `core:placed`, `core:broken` e `pipes:refresh` o jogam
fora na hora, sem esperar o evento. Uma parte do mundo carregada de volta, que o `watch_cell` não
vê, joga fora os grafos que passam por ela assim que volta à memória: dezoito LBMs
(`pipes:loaded_1` … `_18`, um para cada 32 das 576 formas dos nove tubos) chamam `touched` em todo
tubo carregado. Um grafo com mais de `STALE_AFTER` (30 s) é montado de novo de qualquer jeito, como
rede de segurança. As células observadas nunca são largadas, porque a observação é um conjunto só
para todos os mods e um observador de `mods/cinnabar` pode estar olhando a mesma célula.

**Caches dos contêineres.** As fileiras de chips de uma interface, os itens de um firewall e o que
o contêiner de um anunciador oferece só são lidos de novo quando o `core.container_revision` desse
contêiner muda.

**Quanto vale.** Numa grade de 30×30 tubos com 20 extratores em baús de coisas que ninguém quer (o
pior caso: a cada segundo cada extrator procura um caminho para 20 itens), 60 segundos de jogo
custavam 28,3 s de ABM com a busca célula por célula e custam 0,69 s com o grafo: de 23,9 para
0,9 ms por tick.

### Os chips

Dois ABMs (`pipes:chips` e `pipes:chips_2`, já que um ABM nomeia no máximo 32 blocos) passam por
toda interface a cada segundo e, para cada fileira, rodam o extrator e o chip de estoque. Os
outros chips são passivos: `class_of` os lê quando algo procura um destino, `broadcasters` quando
algo pede.

`broadcasters(para, item)` pega do Dijkstra guardado de quem pede as interfaces com um anunciador,
a mais próxima primeiro (um contêiner com dois anunciadores conta uma vez). Para cada uma, o
caminho de volta só é calculado se algo de fato parte dali. `request` tira de cada uma (com
`container_take` em "store", depois "product") uma pilha inteira por vez até ter o número pedido,
e manda pilhas brancas com o destino já definido. Os chips de estoque contam o que pediram e ainda
não chegou em `pending[célula][item]`, que diminui na entrega ou quando a pilha muda de destino.

### O painel de pedidos

Um painel do motor (`core.ui_define`, `ui_item`, `ui_button`, `ui_field`): o cliente desenha os
ícones com o nome como dica, o mod só diz o que está onde. Um painel tem no máximo **128
elementos** e o servidor guarda **64** para todos os mods juntos. Então:

- a grade é **9 × 4** e cada casa custa até três elementos (ícone, número, chave inglesa): 108,
  mais 15 de controles, dá 123;
- há **`PANELS` = 6** painéis, `pipes:request_1` … `_6`, dados por turno a quem abre um tubo de
  pedidos. Quando estão todos em uso, o que foi olhado há mais tempo é fechado para o seu jogador.

Para cada jogador uma *vista* lembra o tubo aberto, a busca, a ordem, o que mostrar, a página, a
quantidade e qual item está em qual casa. `draw_request` declara o painel de novo depois de cada
clique. Os cliques voltam como `core:ui_click`: `i<n>`/`w<n>` (casa n: ícone ou chave inglesa),
`prev`, `next`, `order`, `show`, `find` (com o texto), `clear`, `a1`, `a8`, `astack`, `amount`
(com o texto), `core:esc`. Cada um só vale se o tubo ainda está lá e o jogador a até 8 blocos; se
não, o painel fecha. O texto escrito perde os caracteres de controle e é cortado em 40 bytes; a
quantidade fica entre 1 e `MAX_ASK` = 1024.

`stock` conta cada coisa só nos anunciadores de onde ela pode mesmo vir: numa rede com firewalls,
ele olha para cada coisa os caminhos de Dijkstra calculados para ela (os do próprio pedido,
guardados), então o painel mostra só o que um pedido obtém.

### As máquinas e os planos

**As receitas.** As da bancada de trabalho vêm do motor: `core.recipe_count()`,
`core.recipe_result(i)`, `core.recipe_yield(i)` e `core.recipe_items(i)` (ingredientes pelo nome,
uma palavra por casa, sem forma). As da fornalha vêm de `mods/furnace`: ao evento `furnace:ask` ele
responde na hora com um `furnace:smelt {from, to}` para cada fundição e um
`furnace:fuel {item, smelts}` para cada combustível. `ways_to_make()` as lê **uma vez**, no
primeiro pedido, e as guarda pelo id do produto como *modos* `{kind = "craft" | "smelt", makes,
yield, needs}`.

**O plano.** `plan(estoque, item, q, máquinas)` é uma busca em profundidade:

- `need(id, q, passo)` tira do estoque o que pode (anotando em `passo.uses`, que será levado à
  máquina) e pede o resto a `make`;
- `make(id, q, pai)` tenta os modos de fazer aquilo que as máquinas presentes sabem fazer,
  `⌈q ÷ rendimento⌉` vezes, e para uma fundição o primeiro combustível de que há o bastante;
- toda retirada do estoque vai para um registro, e uma tentativa que falha é **desfeita** até a
  sua marca; uma coisa que já está sendo feita mais acima na cadeia não é tentada de novo (sem
  ciclos);
- limites: `MAX_DEPTH` = 6 receitas em cadeia, `PLAN_BUDGET` = 4000 tentativas por plano.

Os passos saem na ordem em que podem ser feitos: os filhos antes do pai. Cada passo sabe para onde
vai o seu produto (`parent`) e por quantos filhos espera (`children`).

`makeable` pede um plano *a seco* de um de cada produto, e o painel o usa para a chave inglesa. Com
um estoque de 638 tipos e as duas máquinas, 286 coisas saem fabricáveis e desenhar o painel custa
cerca de 3,5 ms.

Se o plano para a quantidade inteira falha, `craft` acha, cortando pela metade, a maior quantidade
que pode ser feita.

**As receitas de uma máquina.** `abilities(rede)` olha, para cada máquina da rede, os tubos de
interface nos seus seis lados e as fileiras deles com um chip de receita (`rows_of`, guardado até o
contêiner mudar). Ele dá `[tipo][produto]` = as máquinas que sabem fazê-lo. `plan` tenta só os
modos cujo produto está ali, e `makeable` e `craft` partem dele: o painel e os trabalhos veem na
hora um chip posto ou tirado.

**O trabalho.** `craft` dá cada passo, entre as máquinas com o chip dele, à de menor pontuação
(distância do tubo de pedidos + 16 por passo já na fila). Depois manda a cada máquina, a partir dos
anunciadores, o que o seu passo tira do estoque. Os cubinhos levam `into` = a célula da máquina, e
`deliver` os põe no contêiner dela.

**A vez de uma máquina.** Um handler de `core:tick` passa pelas máquinas com trabalho:

- se há um em andamento e o tempo acabou, ela entrega o produto: no próprio contêiner se o pai
  está na mesma máquina, se não enviado à máquina do pai, ou ao tubo de pedidos no último passo;
- se não, ela procura o primeiro passo da fila que tem tudo o que precisa (`needs`) no contêiner,
  o tira e acende a máquina (troca o bloco entre `_on` e apagado, mantendo a orientação);
- se um passo sem filhos pendentes esperou mais que `WAIT_FOR` (90 s, contados desde a última
  coisa que chegou à máquina), o trabalho é abandonado;
- uma máquina parada por `IDLE_FLUSH` (5 s) manda de volta as suas sobras, uma pilha por vez.

**As máquinas no grafo.** Uma máquina não é um tubo. `build` a acha entre as células ao lado da
rede e a registra em `net.machines` com o tubo que a alimenta, e esse tubo vira um nó do grafo. As
máquinas não contam como contêineres para interfaces e tubos de pedidos. Acender e apagar não joga
o grafo fora, já que é a mesma máquina.

**As sobras das receitas.** `ways_to_make` pergunta ao motor, para cada ingrediente,
`core.item_leaves(nome)` (o que `core.item_returns` disse que sobra): quando um passo começa, a
máquina põe no seu contêiner o que os ingredientes gastos deixam, e o manda de volta ao estoque
quando fica parada.

**Depois de um reinício.** Os trabalhos não são salvos. Um LBM (`pipes:machines`) acha as máquinas
de novo quando os chunks delas carregam, apaga uma que ficou acesa e a registra: parada e sem
trabalho, ela manda o que tem de volta ao estoque. As pilhas que iam para uma máquina são salvas
com o seu `into` e chegam lá, depois voltam do mesmo jeito.

### As pilhas através de um reinício

A cada `SAVE_EVERY` (3 s) `save_transit` escreve no armazenamento do mod (`core.storage_set`,
salvo no arquivo do mundo) toda pilha em trânsito: `item|quantos|célula de|célula para|destino|into|pedida`,
separadas por `;`, em quantos valores forem necessários (`transit_1`, …, no máximo 4 KB cada;
`transit_n` diz quantos), e esvazia os que não são mais necessários. No `core:world_ready`
`restore_transit` as põe de volta nos tubos: elas partem da célula onde estavam, sem caminho mas
com o destino, e acham o caminho no primeiro tubo roteado. As contagens dos chips de estoque
(`pending`) são refeitas a partir das pilhas pedidas. Uma pilha cuja célula à frente não está na
memória (`core.loaded`) espera.

**Os modelos.** `tools/make_pipes_machines.py` (Blender, `--background`) monta os quatro modelos --
`autocrafter`, `autocrafter_on`, `autofurnace`, `autofurnace_on` -- exporta-os para `mods/pipes`
e deixa o trabalho em `assets/blender/pipes_machines.blend`. Os ícones e os modelos na mão vêm de
`tools/make_held_models.py` (`ONLY=pipes:autocrafter,pipes:autofurnace`).

### Para quem escreve mods

- **`pipes:refresh {x, y, z}`**: calcula de novo a forma de uma célula. Emita depois de colocar
  tubos com `core.set_block`, que não gera `core:placed`. O grafo da rede se atualiza sozinho
  (`core.watch_cell`): o evento é para a forma.
- **`pipes:clear {x0, z0, x1, z1}`**: faz as pilhas em trânsito nesse retângulo (qualquer y)
  sumirem sem cair. Para quem monta uma rede de novo (a demonstração usa).
- Um tubo de interface ou firewall colocado com `core.set_block` não tem casas: crie-as com
  `core.container_make(x, y, z, 36)` (interface) ou `9` (firewall). A demonstração mostra como
  enchê-las.
- Qualquer bloco com um contêiner (`core.container_make`) funciona ao lado de uma interface; os
  papéis das casas (`core.container_roles`) decidem de onde o extrator e o anunciador tiram.

Os números para mudar estão no topo de `init.luau`: `MAX_NET`, `STALE_AFTER`, `MAX_ROUTES`,
`PANELS`, `MAX_ASK`, `LOST_FOR`, `TAKE`, as velocidades em `CLASS`, `REQUESTED`, `LOST`, e para as
máquinas `MAX_DEPTH`, `CRAFT_SECONDS`, `SMELT_SECONDS`, `WAIT_FOR`, `IDLE_FLUSH`, `PLAN_BUDGET`,
`SAVE_EVERY`, e para os tubos novos `LATENCY_COST`, `LATENCY_SLOW`, `RECIPE_SLOTS`,
`MAX_EXTENSIONS`, `KEEP_EVERY`.

### Os tubos novos, por dentro

- **Válvulas:** sempre nós do grafo (`net.valves`). `ways_of` adiciona toda válvula fechada aos
  nós deixados de fora e à chave da cache, então abrir ou fechar uma escolhe outro conjunto de
  caminhos sem jogar o grafo fora. `valve_open` olha os seis vizinhos. `arrive` para uma pilha na
  frente de uma válvula fechada no seu caminho.
- **Latência:** o comprimento de uma aresta é a soma das suas células, `LATENCY_COST` para um tubo
  de latência; no tick uma pilha anda a `LATENCY_SLOW` da sua velocidade enquanto está num deles.
- **Prioridade:** cada fileira de chip tem `prio` = quantos daquele chip a interface tem (guardado
  com as fileiras). `class_of` dá classe e prioridade; `route` olha todas as interfaces e guarda
  para cada classe a prioridade mais alta, a mais próxima entre iguais; `broadcasters` ordena por
  prioridade, depois por distância.
- **Tubos de fabricação:** as receitas ficam no meta da célula `r1` … `r9` (nomes de itens).
  `recipes_of_pipe` as junta com as das extensões alcançadas, e `abilities` as dá à máquina que o
  tubo toca. O painel (`pipes:recipes_1` … `_3`, por turno) oferece os produtos dos modos do tipo
  da máquina atendida (`served_kind`).
- **Roteador:** `show_network` escreve a página `pipesnet`.
- **Trabalho longe:** a cada `KEEP_EVERY` (20 s), `core.keep_loaded` nas colunas de toda pilha a
  caminho e de toda máquina com trabalho, que ficam na memória pelos 30 s seguintes.
