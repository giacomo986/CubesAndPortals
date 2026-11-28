# Escrever um mod

*[Read in English](MODDING.md) · [Leggi in italiano](MODDING.it.md)*

Estado: M2 em andamento. Este documento descreve o que **funciona hoje**, não o que está previsto;
para o plano completo veja `DESIGN.md` §5 e §6.

Um mod é uma pasta dentro de `mods/` com dois arquivos:

```
mods/beacons/
    mod.conf      <- o manifesto
    init.luau     <- o código
```

Nenhuma toolchain. `init.luau` é um arquivo de texto que você abre num editor e ele roda.
`--!strict` no topo liga os tipos, mas não é obrigatório (DESIGN.md §5.5).

---

## mod.conf

Formato `chave = valor`, linhas que começam com `#` são comentários.

| chave | padrão | significado |
|---|---|---|
| `name` | — (obrigatória) | o nome do mod, e o namespace dos seus blocos |
| `version` | `0.0.0` | `1`, `1.2` ou `1.2.3`; o que falta é zero |
| `api` | a mais antiga servida | o número inteiro da API de mods para a qual o mod foi escrito |
| `description` | vazio | uma linha |
| `entry` | `init.luau` | o arquivo carregado |
| `enabled` | `true` | `false` = presente mas não carregado |
| `capabilities` | vazio | uma lista separada por vírgulas, veja abaixo |
| `depends` | vazio | mods que precisam ser carregados antes, com ou sem versão |
| `memory_kb` | 32768 | o teto do heap da VM |
| `interrupt_budget` | 2000000 | safepoints por chamada |
| `rooms` | — | `all`: um mod do jogador dado a uma sala de um servidor é carregado também em todas as outras salas (veja abaixo) |

**Uma chave desconhecida é um erro de carregamento, não um aviso.** Com `capabilties = os` escrito
por engano, o mod não inicia: um manifesto que se lê como "nenhuma capacidade" por causa de um erro
de digitação é exatamente como um sistema de permissões acaba mentindo.

Uma pasta sem `mod.conf` não é um mod que ganha tudo: não é um mod.

**`rooms = all`.** Os mods do jogador (`--user-mods`) vão para as salas a que são dados: um mundo
iniciado pela tela de título os dá ao sandbox e aos salões de bolso, então um mod feito para o
sandbox não roda na arena nem na pista. Um que diz `rooms = all` -- uma espécie de um add-on, que
precisa ser desenhada em todo mundo em que o jogador entra -- o servidor carrega também em todas as
outras salas, só aquele mod (`--add-mod DIR`, que ele mesmo escreve): o resto da pasta fica onde foi
dado, e um mod que o mundo desligou (`--no-mod`) não é levado. O log diz
`server: mod 'lizard' in every room (rooms = all)`.

---

## Versões

`version` são **três números**, não uma string livre: `1`, `1.2` e `1.2.3` são todas versões, e o
que não está escrito vale zero. Todo o resto -- uma letra, um componente vazio, um quarto número --
é um erro de carregamento.

O motivo é que uma versão existe para ser **comparada**. Como string, `1.10` vem antes de `1.9`, que
é o contrário do que pede quem escreve `>= 1.9`.

### `depends`, com ou sem versão

```
depends = spike >= 0.2, guard
```

Um nome sozinho quer dizer "qualquer versão", e é isso que todo `depends` deste repositório queria
dizer antes que as versões pudessem ser comparadas. Os operadores são cinco: `>=`, `>`, `=`, `<=`,
`<`. Espaços como quiser: `spike>=0.2`, `spike >=0.2` e `spike >= 0.2` são a mesma coisa.

Duas recusas diferentes, porque são dois problemas diferentes:

```
mod 'beacons' not loaded: depends on 'spike', which is not loaded
mod 'beacons' not loaded: depends on spike >= 2.0.0, and the one loaded is 0.2.0
```

A primeira manda você instalar um mod, a segunda atualizar o que já está lá. Dizer a primeira quando
a verdade é a segunda é uma tarde perdida.

### `api`, ou seja, para qual motor o mod foi escrito

`api` é **um número inteiro**, não uma versão. Diz para qual versão da API de mods o mod foi escrito.
Esta build serve duas, `api = 1` e `api = 2`, e recusa qualquer outra:

```
mod 'futuro' not loaded: written for mod API 3, and this build serves 1 to 2
```

| `api` | o que muda |
|---|---|
| 1 | uma chamada que responde sim ou não responde **1 ou 0** -- e em Luau 0 é verdadeiro, então é preciso comparar: `core.loaded(x, y, z) == 1` |
| 2 | as mesmas chamadas respondem **true ou false**: `if core.loaded(x, y, z) then`. `docs/api.d.luau` as tipa assim (`flag` em `src/script/api.luau`, uma nota embaixo de cada uma em [API.md](API.md)) |

Um mod escrito para a 1 continua carregando e recebendo números. Passá-lo para a 2 é a linha `api` e
cada comparação com 1 ou 0 dessas chamadas -- o verificador de tipos lista todas, como "Cannot
compare unrelated types 'boolean' and 'number'".

Um mod que não a declara é tomado pela **mais antiga** que esta build serve. É a única suposição que
não pode promover às escondidas um mod velho para uma API nova: no dia em que a mais antiga servida
subir, esse mod é recusado e recebe o pedido de declarar o que quer, em vez de falhar na linha 40.

A linha do log diz isso, e diz também quando ninguém disse:

```
mod 'cave' 0.1.0 (api 2) loaded, capabilities: none
mod 'vecchio' 0.1.0 (api 1, assumed) loaded, capabilities: none
```

A mesma regra vale no recarregamento: um `mod.conf` mudado para uma API que esta build não serve
**não** é carregado, e a versão de antes continua rodando.

---

## Coisas que acontecem sozinhas com as células

Dois tipos, e não são a mesma coisa:

```lua
-- ABM: a cada `interval` segundos, cada célula carregada que contém um destes
-- blocos tem uma chance em `chance` de a ação disparar.
core.register_abm("timber:moss", {"timber:log"}, 2.0, 4, function(cell)
    core.set_block(cell.x, cell.y, cell.z, MOSSY)
end)

-- LBM: uma vez por célula correspondente **quando o chunk é carregado**, e
-- nunca mais. É o que serve para consertar um mundo escrito por uma versão
-- antiga do mod.
core.register_lbm("timber:clean", {"timber:mossy_log"}, function(cell)
    core.set_block(cell.x, cell.y, cell.z, LOG)
end)
```

A ação recebe **uma tabela** -- `cell.x`, `cell.y`, `cell.z`, `cell.id` --, que é a forma de todo
handler deste motor, de um evento a uma etapa de mapgen. A lista de blocos pode ser uma tabela ou uma
string só.

**Eles são registrados enquanto o mod carrega**, como `core.register_mapgen`: pedir um a partir de
um handler de tick é recusado, porque seria um modificador que nunca roda. Um nome já usado é
recusado, e ABMs e LBMs dividem o mesmo espaço de nomes. Um `interval` que não é positivo ou uma
`chance` abaixo de um são recusados. Um bloco que nenhum mod registrou **desativa** esse modificador
e diz isso no log, nomeando-o: um ABM sobre um bloco que não existe varreria o mundo para sempre sem
achar nada.

**Quanto custa, e por que não pode atrasar o tick.** Um mundo de 512 são 512 chunks e 16,7 milhões
de células. Um ABM não as lê: cada chunk carrega um bit por id de bloco *módulo 64*, então um chunk
que não pode conter os blocos do ABM é pulado com um AND. O que sobra é lido célula por célula, e
cada passo tem um **orçamento de 2 ms**: se a varredura não cabe, ela para onde está e continua no
tick seguinte, e o intervalo só recomeça a partir de uma varredura **terminada**. Um mod que pede
mais do que isso roda mais devagar do que pediu em vez de atrasar o servidor, e o log diz isso:

```
modifiers: 3 action(s) over 262144 cell(s) in 0.008 s, 0 sweep(s) still going
  timber:moss           3 fired of     262144 tested, 0.008 s
```

**Determinismo.** A chance é um **hash** da semente, do número da varredura e das coordenadas da
célula, não um gerador cujo estado depende de como o orçamento cortou a varredura. Duas execuções da
mesma semente fazem a mesma coisa, e os chunks são visitados em ordem pelo mesmo motivo.

**Uma ação que dá erro custa o seu mod**, como um handler de evento, e esse modificador não é mais
chamado. Os outros continuam no mesmo passo.

**O que um LBM não promete:** se o orçamento acaba no meio dos modificadores de um chunk, esse chunk
volta para a fila e os que já tinham rodado rodam ali de novo. Um LBM precisa ser escrito de modo que
rodar duas vezes numa célula já consertada seja inofensivo -- que é o que "carregamento" quer dizer
para um modificador que pode ser interrompido.

---
## Blocos que giram

Um bloco declara **o que o byte de orientação da célula quer dizer**, e ele não quer dizer nada se o
bloco não declara:

| `turn` | valores | o que é |
|---|---|---|
| — (nada) | 0 | não gira |
| `upright` | 0..3 | quartos de volta em torno da vertical: um baú, uma fornalha |
| `facedir` | 0..23 | qualquer uma das vinte e quatro orientações de um cubo: um tronco deitado, uma escada |
| `wallmounted` | 0..5 | a **face** para onde a frente aponta: uma escada de mão, uma placa |

```lua
local LOG = core.register_node("timber:log", 126, 92, 54, true, 0, "facedir", 0, 1.6, "wood")
core.set_block(x, y, z, LOG)
core.set_facing(x, y, z, 12)     -- deitado ao longo de x
```

`facedir` é `eixo * 4 + giro`, onde o eixo diz **para onde vai o +Y do bloco** -- 0 +Y, 1 +Z, 2 -Z,
3 +X, 4 -X, 5 -Y -- e o giro são quartos de volta em torno desse novo alto. **0..3 são exatamente os
quatro giros que já existiam**, porque o eixo 0 não inclina nada: todo byte em todo arquivo de mundo
mantém o significado que tinha. A ordem dos eixos é a do `param2` facedir do motor de partida, o
motor de onde este projeto vem: uma convenção já testada, adotada porque é boa. A camada de
compatibilidade para a qual ela foi escolhida não está mais sendo feita (DESIGN.md §7.5), e a ordem
continua esta assim mesmo -- mudá-la agora significaria reescrever o byte de cada célula girada em
cada arquivo de mundo.

`wallmounted` é um **índice de face** nesta ordem: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z. Uma célula
que ninguém girou vale 0, ou seja +X; colocar um bloco sempre escreve a face clicada, então isso só
aparece numa célula escrita com `core.set_block` e nunca girada.

**Colocar dá quatro giros, não vinte e quatro.** Um bloco `upright` ou `facedir` colocado por um
jogador vira para ele, a partir da sua **posição** (o servidor tem isso, e não o olhar); um
`wallmounted` aponta para fora da face que foi clicada. As outras vinte orientações existem para um
mod -- `core.set_facing` -- e para a ferramenta que gira uma célula já colocada (`mods/screwdriver`).

**Um byte que o bloco não aceita é recusado**, não mascarado: um baú girado para 7 é um erro de quem
escreve o mod, e `core.set_facing` diz isso no log com o nome do mod. Ao desenhar, uma célula com um
byte que o seu bloco não aceita é desenhada **sem giro** em vez de ler fora de uma tabela.

**O id nunca muda.** Um bloco que gira é **um** bloco: o id que `register_node` devolveu é o que está
no mundo, girado como estiver, então um handler compara `ev.id` diretamente.

**As partes giram com o giro das faces.** Um bloco feito de caixas (`core.node_box`) deitado tem as
caixas giradas pela mesma matriz que decide qual imagem vai em qual face: forma e imagens não podem
discordar sobre como ele está posto.

---

## Um mod em vários arquivos: `require`

A entrada de um mod (`init.luau`) pode passar partes de si para outros arquivos do mesmo mod:

```lua
local receitas = require("@self/receitas")  -- de init.luau: receitas.luau, ao lado
local ui       = require("@self/ui")        -- ui.luau, ou ui/init.luau se ui for uma pasta
local estrada  = require("../estrada")      -- de um arquivo dentro de ui/: o de cima
```

Cada arquivo é **um chunk à parte**, então tem as suas próprias 200 variáveis locais: o limite que
Luau põe no corpo principal de um chunk, e que um mod longo alcança (`mods/villagers` alcançou). Os
arquivos dividem as variáveis globais do mod e a sua VM -- a mesma cota, o mesmo orçamento, o mesmo
`core`.

- **Os caminhos são os do próprio Luau** (require-by-string): `./` e `../` a partir do arquivo que
  pede, `.luau` acrescentado, uma pasta quer dizer o seu `init.luau`, e **`@self/` para "dentro de
  mim"**: de um `init.luau` a sua própria pasta, de `a.luau` a pasta `a/`. Um editor que conhece
  Luau também os segue. **A partir de um `init.luau`, escreva `@self/`**: Luau agora lê `./` ali
  como a pasta de cima, então `require("./x")` carrega no jogo (o motor sempre o leu como ao lado do
  arquivo, e continua lendo) e num editor é "Unknown require". Um caminho sem `./`, `../` ou
  `@self/` é recusado: não há outro lugar onde procurar.
- **Um arquivo executado pelo que faz** -- registra, preenche uma tabela compartilhada -- **termina
  com `return nil`**. O motor toma o nada por `true` de qualquer jeito; o verificador de tipos se
  recusa a fazer `require` de um arquivo que não devolve exatamente um valor.
- **Só dentro da pasta do mod.** Um caminho que sai dela é recusado, seja como for escrito.
- **Um arquivo roda uma vez só.** O segundo `require` dele recebe o que o primeiro recebeu: o que o
  arquivo devolveu com `return` (`true` se não devolveu nada).
- **Dois arquivos que pedem um ao outro** enquanto os dois ainda estão rodando são um erro que nomeia
  o arquivo, não uma tabela construída pela metade.
- **Editar um arquivo pedido recarrega o mod**, como editar a sua entrada.
- Viaja junto: um pacote, e o código de um mundo enviado para a máquina de um jogador, são a pasta
  inteira.
- **Um arquivo pode ser um link** para um de outro mod, e é assim que dois jogos dividem código: o
  `battle.luau` do assalto é um link para o do front (`games/front/mods/front_game/battle.luau`), e
  os dois fazem `require("@self/battle")`. Ele é lido através do link, e empacotado -- e enviado
  para a máquina de um jogador -- com o nome que tem no mod. Cada mod ainda recebe a sua própria
  cópia: o que se divide é o texto, não uma tabela.

## Um mod como um arquivo só

Uma pasta não é algo que se possa entregar a alguém: nada diz quais arquivos fazem parte dela, nada
diz que todos chegaram, e nada pode ser verificado antes de rodar o Lua de outra pessoa. Um
**pacote** é esse arquivo.

```sh
cnp_modpack pack   mods/ore            # ore-0.1.0.cnpmod, a partir do mod.conf de dentro
cnp_modpack list   ore-0.1.0.cnpmod    # o que contém, e se está inteiro
cnp_modpack unpack ore-0.1.0.cnpmod ./ore
```

Um `.cnpmod` colocado na pasta dos mods carrega como uma pasta:

```
mods/
    cave/                 <- uma pasta
    ore-0.1.0.cnpmod      <- um pacote
```

O motor o verifica **inteiro**, o descompacta numa pasta de preparo e daí em diante ele é um mod como
os outros -- o mesmo manifesto, a mesma ordem, as mesmas recusas. Meio mod é pior que nenhum mod:
registraria metade dos seus blocos e deixaria um mundo cheio deles, e quem olha não tem como saber
qual metade.

O formato, por inteiro, está em [`src/modpack/mod_package.h`](../src/modpack/mod_package.h). O que
importa para quem escreve mods:

- **O nome do mod não está escrito no pacote**: está no `mod.conf` de dentro, o único lugar de onde
  um manifesto é lido. A ferramenta decide o nome do *arquivo* a partir dali. Um nome do lado de
  fora é um nome que pode mentir.
- **Um pacote é canônico**: os arquivos são ordenados pelo caminho e os seus bytes são contíguos,
  então empacotar o mesmo mod duas vezes dá o mesmo arquivo, byte por byte. Medido: `mods/ore` dá o
  mesmo `.cnpmod` toda vez.
- **Um caminho que sai do mod é recusado** -- `..`, um caminho absoluto, uma barra invertida, dois
  pontos -- e três vezes: quando o pacote é escrito, quando é lido e quando é descompactado.
- **O que não vai dentro**: arquivos que começam com um ponto, em todos os níveis. Um `.git` não faz
  parte de um mod.
- **Um pacote não é recarregado a quente.** A recarga a quente observa as datas de modificação dos
  arquivos descompactados, que não mudam: você edita uma pasta, você distribui um pacote.
- **Dois mods não podem ter o mesmo nome.** Um pacote ao lado da pasta do mesmo mod são duas cópias
  registrando os mesmos blocos: a segunda é recusada dizendo de onde vem a primeira.

---

## Capacidades

Luau já começa muito menor que Lua: `io`, `package`, `loadstring`, `dofile`, `os.execute`,
`os.getenv` **não existem**. Não há nada para remover porque eles nunca estiveram lá. O que sobra e
um mod não recebe por padrão:

| capacidade | o que concede | estado |
|---|---|---|
| `os` | `os.clock`, `os.date`, `os.difftime`, `os.time` | concedida |
| `debug` | a tabela `debug` | concedida |
| `native`, `exec`, `fs:read <dir>`, `net:http <host>` | — | **recusadas**: esta build não as concede a ninguém |

Uma capacidade desconhecida é um erro de carregamento. Uma declarada mas que não pode ser concedida é
um erro de carregamento **com o nome dela na mensagem** -- não um carregamento bem-sucedido com uma
surpresa na hora de rodar.

As globais não concedidas são **removidas da tabela global antes de a sandbox ser congelada** e antes
que uma linha do mod rode. Não estão escondidas nem vazias: estão ausentes. Um mod pode verificar isso
sozinho:

```lua
assert(os == nil, "este mod não declarou os")
```

`os` fica atrás de uma capacidade porque o tick precisa poder ser determinístico para semente +
entrada (DESIGN.md §5.7), e um mod que lê o relógio não é.

---

## Uma VM por mod

Cada mod tem o seu próprio `lua_State`, a sua cota de memória e o seu orçamento de interrupção. As
consequências são verificadas por `tests/script_isolation_test.cpp` (roda no `ctest`, sem GPU):

- Um mod num **loop infinito** é interrompido (`script exceeded its interrupt budget`) e
  desativado. Os outros continuam rodando **no mesmo tick**.
- Um mod que **fica sem memória** recebe um erro Lua, não mata o processo. É a lição do mundo de
  512 do M1, onde o OOM killer chegou primeiro.
- **As globais não são compartilhadas**: dois mods podem ter os dois `elapsed` e `on_step` sem se
  enxergarem.
- Um mod que falha é desativado uma vez, não é tentado de novo a cada frame.

O timeout de 60 s no teste não é burocracia: se a interrupção para de disparar, o teste não falha --
ele roda para sempre dentro do `while true do end` do mod ruim. Verificado zerando esse orçamento.

---

## Ordem de carregamento

Alfabética, depois reordenada por `depends`. Isso importa de verdade: `beacons` vem antes de `spike`
na ordem alfabética, e na primeira execução com dois mods `core.node_id("spike:copper")` devolveu
`0`. Com `depends = spike` ele devolve o id certo.

Uma dependência que falta é uma recusa que a nomeia. Um ciclo não trava o carregamento: os mods
envolvidos são recusados um a um, cada um dizendo o que estava esperando.

### Os mods do próprio jogador

Além dos `mods/` do jogo, um mundo iniciado pela tela de título carrega os mods da pasta de mods do
jogador -- `~/.cubes-and-portals/mods`, escondida na home, no Linux e no macOS,
`%APPDATA%/cubes-and-portals/mods` no Windows --, pastas ou arquivos `.cnpmod`, só no seu sandbox
(os mundos temáticos são do jogo). Um servidor recebe o mesmo com `--user-mods DIR`. As duas pastas
são **um só conjunto**: um mod ali pode ter `depends` num do jogo, e um mod com o nome de um do jogo
é recusado, dizendo de onde vem o primeiro.

Um mundo pode ter mods próprios e desligar os globais. Na pasta do mundo
(`~/.local/share/cubes-and-portals/worlds/<mundo>/`):

```
mods/             <- mods só deste mundo, como acima
mods.conf         <- off = um_mod, outro_mod
```

A mesma linha `off = ...` em `~/.cubes-and-portals/mods.conf` (ao lado da pasta de mods) desliga um
mod em todos os mundos. A página **Mods** do launcher escreve os dois: lista os mods do jogo (só
leitura), os seus em todo lugar e os de um mundo, diz do que cada um precisa e o que pede, desliga
ou liga, remove, e adiciona um que for solto na janela (uma pasta ou um `.cnpmod`).

Um mod desligado é nomeado no log (`mod 'x' left out by this world`) e não é carregado; um servidor
o recebe como `--no-mod NOME`, e `--user-mods` mais de uma vez. Os blocos dele que já estão no mundo
viram marcadores magenta até ele ser ligado de novo, e aí voltam a ser eles mesmos.

---

## Recarga a quente

Mude um arquivo de um mod e salve: em meio segundo o mod é recarregado, **sem reiniciar o jogo e sem
gerar o mundo de novo**. `F5` força a recarga de todos os mods.

O que a recarga promete:

- **O código novo roda, o velho sai.** Uma VM nova, globais novas.
- **Uma versão que não compila não custa o mod.** A VM nova é construída e executada *antes* de a
  velha ser destruída: se falhar, a versão que funcionava continua rodando, e o erro vai para o log.
- **Os ids dos blocos continuam os mesmos.** O mundo está cheio desses blocos; renumerá-los
  significaria reescrever cada chunk.
- **Um bloco que o mod não define mais vira órfão**: id mantido, textura magenta viva, ainda sólido
  (ninguém deve cair por um chão que estava ali). O mesmo acordo que os jogos de blocos clássicos
  fazem.
- **As capacidades não se ampliam sozinhas.** O manifesto é lido e avaliado de novo: acrescentar
  `capabilities = native` e salvar faz a recarga falhar, não a concede.
- **Os outros mods não são tocados**: a mesma VM, o mesmo estado, nenhum tick perdido.

### Estado que sobrevive à recarga

```lua
function on_save(): string
    return tostring(counter)      -- o mod decide qual é o seu estado
end

function on_load(state: string?)
    counter = tonumber(state) or 0
end
```

Uma string, não uma tabela: um serializador genérico teria que ter opinião sobre funções, ciclos e
userdata, e erraria nas três. O limite é 64 KB, e passar disso é um erro -- não um corte silencioso.

---

## Barramento de eventos tipado

O esquema de um evento é **declarado**. Emitir um campo errado é um erro que nomeia o campo, não um
`nil` que alguém lê três handlers depois.

```lua
core.event.define("guard:alarm", {
    fields = { x = "number", y = "number", z = "number" },
    cancellable = false,
})

core.event.on("core:pre_place", 10, function(ev)
    if ev.x >= 36 and ev.x <= 46 then
        ev.cancel = true                       -- cancela
    elseif ev.id == core.node_id("core:grass") then
        ev.id = core.node_id("core:stone")     -- muda
    end
end)

core.event.emit("guard:alarm", { x = 40, y = 21, z = 40 })
```

Tipos aceitos nos campos: `number`, `string`, `boolean`. Três, não qualquer um: um barramento que
carrega qualquer coisa é um barramento cujo esquema não quer dizer nada.

- **Prioridade**: um número, padrão 0, executado do menor para o maior; no empate ganha quem se
  registrou primeiro. Declará-la é o ponto -- ordem de carregamento não é uma ordem.
- **Cancelar** para os handlers *seguintes*. Quem veio antes já falou.
- **Mudar** o payload o muda para os handlers seguintes **e para o motor**: `core:pre_place` com
  `ev.id` reescrito coloca o bloco reescrito.
- **Entre VMs**: um mod emite, outro escuta, nenhum chama o outro.
- Um handler que dá erro **desativa o próprio mod e só**: o evento segue para os outros handlers.
- Escutar um evento que ninguém declarou é um erro de carregamento que o nomeia. Declarar de novo o
  evento de outro mod é recusado.
- Um evento emitido precisa de **todos** os seus campos (`event 'gates:ask' takes 5 field(s), got 4`).

### Eventos do motor

O núcleo usa o mesmo barramento, com os mesmos esquemas, sem caminhos privilegiados (DESIGN.md
§6.3).

| evento | campos | cancelável |
|---|---|---|
| `core:pre_place` | `x, y, z, id, player` | sim |
| `core:placed` | `x, y, z, id, player` | não |
| `core:pre_break` | `x, y, z, id, player` | sim |
| `core:broken` | `x, y, z, id, player` | não |
| `core:ui_click` | `panel, id, player, text` | não |
| `core:used` | `x, y, z, id, player, held` | sim |
| `core:tick` | `dt` | não |
| `core:hit` | `player, target, damage, killed` | não |
| `core:watched` | `x, y, z` | não |
| `core:world_ready` | — | não |
| `core:net_message` | `player, mod, text` | não |
| `core:command` | `player, name, args` | não — só para o mod que o declarou |
| `core:page_open` | `player, page` | não — só para o mod que registrou a página |
| `core:page_action` | `player, page, id, text` | não — só para o mod que mostrou a página |
| `core:input` | `action, player` | sim — só num cliente |
| `core:far_moved` | `index, x, y, z, axis, sign, half_w, half_h` | não — só num servidor de salas |
| `core:arrived` | `player, from` | não — só num servidor; um portal o trouxe com um bilhete do servidor amigo `from` (com o nome dado por /friend) |
| `core:fed_message` | `server, mod, text` | não — só num servidor, só para o mod com esse nome, na primeira sala; `core.fed_send` no amigo `server` |
| `core:died` | `player, x, y, z` | não — só num servidor; o corpo já voltou para a cama ou para o spawn, x y z é onde morreu |
| `core:hurt` | `player, damage` | não — num servidor e num mundo de um só; o golpe de uma criatura ou uma flecha atingiu esse jogador, `damage` antes da armadura |
| `core:burst` | `x, y, z, nx, ny, nz` | não — só num servidor; a bola de fogo de uma criatura estourou no bloco em x y z, na face com normal nx ny nz |

`core:used` é um clique direito num bloco ao alcance, **com qualquer coisa na mão**. Ele nomeia o
bloco atingido, não a face ao lado, e é emitido **antes** de colocar: um handler que põe
`ev.cancel = true` **tomou para si** o clique, e a colocação por trás dele não acontece. É assim que
um baú abre com um bloco na mão em vez de obrigar você a guardá-lo -- e sem esse `cancel` o mesmo
clique abriria o baú e construiria em cima dele. Com **Shift** apertado e um bloco que dá para
colocar na mão, `core:used` não é emitido: o clique coloca e só, contra um baú ou uma mesa também.

`held` é o que quem clicou **tinha na mão**, pelo nome, vazio para a mão vazia -- e `id` é o bloco
**clicado**. Um pedestal que lia `id` para saber o que estava sendo oferecido a ele estava lendo a
si mesmo. O clique é o único momento em que o motor sabe o que está na mão: `Inventory` guarda um
slot selecionado, mas no servidor ele só é escrito quando uma colocação dá certo, então entre duas
colocações ele é o slot da última e não a mão. Por isso `held` está no evento e não é uma chamada
`core.held_item(player)` que responderia com algo que era verdade um tempo atrás.

`text` é o que o jogador escreveu num campo (`core.ui_field`) quando apertou Enter, `""` para um
botão: texto de um cliente, até 128 bytes, a ser tratado como não confiável.

`player` é quem fez: um número para devolver a `core.ui_show` e pôr um painel **naquela** tela e em
nenhuma outra. Um mod não o inventa e não mantém uma tabela de jogadores. No cliente de um só
processo o campo existe do mesmo jeito e tem sempre o mesmo valor, então um mod escrito para um
servidor roda ali sem saber onde está.

`core:tick` é o gancho de tick para quem prefere se inscrever em vez de definir uma global
`on_step`. Desde 2026-10-06 ele também chega ao código de um mundo na máquina de um jogador
conectado, uma vez por frame com o seu `dt` (um relógio para mostrar, uma primeira mensagem ao
servidor assim que houver uma); `on_step` e o resto continuam sendo do servidor. Um mod `both` que
registra um tick só para o servidor o coloca dentro de `if serving`. Um emit sem handler custa uma
busca num hash e um vetor vazio.

`core:world_ready` é **onde vai tudo o que olha para o mundo**, e não é uma comodidade. Um mod é
carregado **antes** de o arquivo do mundo ser aberto -- o arquivo é conferido com a tabela de blocos
*final*, então não pode ser aberto até que cada mod tenha registrado os seus --, então um mod que lê
o mundo enquanto carrega está lendo o que o mapgen fez, não o que foi salvo. Um relógio que lia a
própria contagem ao carregar lia zero toda vez, e, sem ver nada ali, construía de novo por cima do
que estava para ser carregado sobre ele. `core:world_ready` é o motor dizendo: agora o mundo é o que
vai ser. É emitido uma vez só, e também num servidor sem arquivo de mundo -- um mod que espera por
ele não pode esperar para sempre.

`core:net_message` é o que chegou **no canal de um mod** -- veja `core.net` abaixo. O campo `mod` é
o canal lido de volta como nome. O evento chega **só aos handlers do mod daquele canal** (desde
2026-09-25): uma mensagem enviada por `hunger` não é ouvida por `enchant`. Antes chegava a todos, e
no cliente as barras de `mods/hunger` e `mods/enchant` trocavam os valores.

`core:hit` é um golpe de um jogador **que acertou** um corpo (uma criatura, uma entidade de um mod
com vida): `target` é o id do corpo, o que `core.spawn_mob` dá e que `core.body_player` e as chamadas `core.mob_*` recebem, `damage` os pontos tirados, `killed` se foi o último. Emitido
depois do fato, no servidor e no cliente que é o seu próprio servidor. `mods/hunger` o conta como
esforço, `mods/enchant` como experiência por uma morte.

`core:far_moved`: uma abertura de outra sala do mesmo servidor, que leva à abertura `index` deste
mundo, foi movida com `core.portal_place`; os outros campos são a sua forma nova. Chega no começo do
tick seguinte. Serve para mover também esta ponta (`games/inferno` constrói assim a moldura
correspondente). Um servidor em outro endereço não o ouve. As colunas em volta de cada abertura são
sempre geradas, mesmo sem jogadores perto.

`core:watched` é uma escrita numa célula que o mod pediu para observar com `core.watch_cell` -- veja
abaixo. Chega no começo do tick seguinte, uma vez por célula por tick.

`core:died`: um jogador morreu em `x, y, z` e já foi devolvido à sua cama ou ao spawn, com a bolsa
ainda com ele (`ServerSessions::kill_body` diz por quê); um mod que quer que uma morte custe algo a
pega aqui (`mods/grave`, `core.bag_to_container`).

Limite conhecido: se uma versão de um mod chama `define` e depois falha ao carregar, o esquema
mudado fica. Os handlers não -- eles saem junto com a VM que os registrou (sem esse passo o processo
dá segfault no primeiro emit depois de uma recarga; verificado).

---

## A API disponível hoje

**Referência completa: [API.md](API.md)** -- gerada, com assinaturas, tipos e valores padrão. Para o
verificador de tipos do editor: [api.d.luau](api.d.luau). O repositório já vem configurado para isso:
`.luaurc` deixa todo arquivo `--!strict`, e `.vscode/settings.json` aponta a extensão Luau Language
Server (luau-lsp) para `docs/api.d.luau` -- abra a pasta e o editor mostra o que
`tools/typecheck.sh` relata. Outro editor com luau-lsp usa o mesmo arquivo como arquivo de
definições.

```lua
core.set_block(x, y, z, id)
core.get_block(x, y, z) -> id
core.register_node(name, r?, g?, b?, solid?, light?, turn?, level?, hardness?) -> id
core.node_id(name) -> id            -- 0 se não existe
core.node_name(id) -> name
core.set_facing(x, y, z, turn)      -- 0 .. core.node_turns(id) - 1
core.node_turns(id) -> n            -- 1, 4, 6 ou 24: quantos giros esse bloco tem
core.loaded(x, y, z) -> bool       -- 0: coluna fora da memória, get_block lê ar
core.light(x, y, z) -> 0..15        -- a luz agora: a dos blocos, ou a do céu para a hora
core.get_facing(x, y, z) -> turn
core.set_portal(index, x, y, z)     -- no servidor: as aberturas do mundo, só enquanto os mods carregam
core.portal_open(x, y, z, axis, sign, turn, half_w, half_h, colour?) -> id|-1  -- no servidor, quando escuta: -1 antes, tente de novo; quantas quiser: as 40 aberturas vão para as mais perto dos jogadores
                                    -- colour: como ela fica fechada enquanto não leva a lugar nenhum: 0 verde (fixa, padrão), 1 azul, 2 laranja (portal gun)
core.portal_close(x, y, z) -> 0..15|-1                             -- pelo centro, não pelo índice
core.portal_move(id, x, y, z, axis?, sign?, turn?) -> bool          -- a cada tick: ela se move (desenhada suave, atravessada relativa a ela); um plano/giro novo ou um salto > 4 a põe lá
core.portal_move(id, x, y, z, -1, 0, -1, nx, ny, nz, ux?, uy?, uz?)   -- girada livremente: frente n, alto u, para qualquer lado; pode girar a cada tick
core.portal_scale(id, on?) -> bool   -- as duas pontas de um par de dois tamanhos: o que atravessa sai escalado por alta/baixa (um jogador que ela levaria além de 1/32..4 não atravessa)
                                     --   (jogadores, criaturas e caixas que rolam, sempre entre 1/32 e 4 da sua espécie)
core.body_scale(player, s?) -> bool  -- um corpo 1/32..4 vezes maior: caixa, degraus, olho, velocidades, gravidade, alcance, desenhado assim
core.body_size(player) -> s         -- o tamanho dele agora (body_scale, um portal que escala); 0 para ninguém
core.portal_link(a, b) -> bool       -- a e b levam uma à outra; b = -1 desliga a
core.portal_window(a, b) -> bool     -- uma janela, de mão única, só para os olhos: a mostra o que b encara, b não é desenhada em lugar nenhum, nada atravessa nenhuma das duas (com portal_scale nas duas: uma miniatura)
core.log(message)
core.play_sound("mod:thing", x, y, z) -> bool  -- para todos no mundo; arquivo mods/<mod>/<thing>.ogg no cliente
core.sound_loop("mod:thing", x, y, z, range?, gain?) -> id|0  -- tocando sem parar até ser parado; quem chega depois também ouve; ouvido através de portais e janelas como core.play_sound; parado para todos quando o mod que o iniciou dá erro, é recarregado ou fecha
core.sound_loop_move(id, x, y, z) -> bool      core.sound_loop_stop(id) -> bool

core.take_item(player, name, count?) -> taken    -- até `count`, diz quantos
core.give_item(player, name, count?) -> fitted   -- o que não cabe fica com o mod
core.bag_to_container(player, x, y, z) -> moved  -- as 36 casas da bolsa, como estão, num contêiner

core.players() -> { id }                         -- quem tem um corpo neste mundo
core.player_pos(id) -> (x, y, z) | nil           -- os PÉS, ou nada se não está
core.player_name(id) -> name                     -- "" se não está; continua igual ao voltar, o id não
core.player_look(id) -> (dx, dy, dz) | nil       -- para onde olha, comprimento 1
core.player_tilt(id) -> (pitch, roll, dx, dy, dz) | nil -- como está o kart que conduz (o fantasma de games/track)
core.player_place(id, x, y, z, dx?, dy?, dz?) -> bool  -- move e gira um corpo, como um portal
core.ride(id, host, x?, y?, z?) -> bool         -- sentado no corpo de host; host 0 = desce
core.monsters_near(x, y, z, r) -> n              -- criaturas de combate dentro de r
core.mob_health(id, hp) -> bool                 -- a vida de uma criatura e o seu máximo
core.mob_life(id) -> (hp, whole) | nil          -- quanto resta a uma criatura: a barra de um chefe
core.world_height() -> n                        -- a primeira célula acima do topo do mundo (64, ou --world-top)
core.raycast(x, y, z, dx, dy, dz, max?) -> (bx, by, bz, distance) | nil

core.meta_set(x, y, z, key, value) -> bool      -- valor vazio = apaga
core.meta_get(x, y, z, key) -> string            -- "" se não há nada
core.storage_set(key, value) -> bool            -- do mod, no arquivo do mundo; vazio = apaga
core.storage_get(key) -> string                  -- "" se não há nada
core.shared_storage_set(key, value) -> bool     -- o mesmo, um para todas as salas (ainda do próprio mod)
core.shared_storage_get(key) -> string
core.notice(player, text) -> bool               -- uma frase na tela; 0 = todos
core.chat(player, text) -> bool                 -- uma linha no chat (T); 0 = todos
core.command(name, help, perm?) -> bool        -- declara /name; chega como core:command
core.has_perm(player, perm) -> bool            -- o papel do jogador alcança perm?
core.role(player) -> string                      -- guest, player, helper, moderator, admin, owner
core.is_muted(player) -> bool                   -- o chat dele está bloqueado (/mute)?
core.page_register(name, title, perm?) -> bool  -- uma página que /page NOME abre
core.page_show(player, name, markup) -> bool    -- mostra uma página a um jogador

core.find_nodes(x1,y1,z1, x2,y2,z2, name) -> { x,y,z, x,y,z, ... }

core.register_mapgen(name, fn)                   -- no carregamento, não depois
core.terrain_height(x, z) -> y                   -- o chão segundo o gerador
core.water_level(x, z) -> y                      -- até onde vai a água: 18 no mar, um lago no alto o seu;
                                                 -- o chão onde a coluna é seca, 0 sem gerador
core.biome(x, z) -> name                         -- o clima da coluna, segundo o gerador:
                                                 -- core:plains/desert/tundra/peaks, ou onde o chão está
                                                 -- em relação ao mar: core:ocean/sea/river/lake/beach,
                                                 -- ou core:gorge
core.seed() -> n                                 -- a semente do mundo (os seus 31 bits baixos), em qualquer lugar
core.plaza() -> lo, hi | nil                     -- os limites da praça, nil num mundo sem praça
core.world_spawn() -> x, y, z | nil              -- onde um jogador chega
core.node_liquid(id, opacity?) -> bool          -- dá para nadar nele, e a luz passa por ele

core.container_take(x, y, z, item, count?) -> taken  -- de um contêiner, a partir do primeiro slot

core.ecs.component(name, { "field", ... })       -- campos numéricos, declarados
core.ecs.phase(name, order)                      -- um ponto no tick, e um número
core.ecs.system(name, phase, component, fn)      -- fn(batch), uma vez para muitas
core.ecs.spawn(component, x, y, z) -> id | nil
core.ecs.set(id, field, value) -> bool           -- "x"/"y"/"z" ou um campo do mod
core.ecs.get(id, field) -> number | nil
core.ecs.kill(id) -> bool
core.ecs.clip(id, name) -> bool                  -- uma animação do seu modelo; "" a devolve ao movimento
core.ecs.box(id, half, high) -> bool             -- o tamanho dela para um golpe
core.ecs.count(component) -> how many

core.net.open() -> channel | nil                 -- no carregamento, não depois
core.net.send(player, text) -> bool              -- 0 = todos neste mundo

core.render.pass(name, order, fn)                -- só num cliente
core.render.box(x, y, z, w, h, d, r, g, b, a)    -- só dentro de um pass
core.render.model(id, x, y, z, yaw?, scale?, pitch?, roll?, dissolve?, alpha?, tint?, clip?, time?)   -- um modelo do mundo, só dentro de um pass; posado por um clip dele naquele tempo
core.render.fx(kind, x, y, z, size?, r?, g?, b?, a?, t?, x2?, y2?, z2?)   -- um efeito de luz, só dentro de um pass
core.render.camera(id, x, y, z, yaw?, pitch?, fov?, still?)   -- a imagem de uma câmera, desenhada de vez em quando (still: uma vez)
core.render.camera_drop(id)                      -- e esquecida
core.render.screen(id, x, y, z, yaw?, w?, h?, plain?)   -- a imagem de uma câmera num retângulo, só dentro de um pass
core.render.text(text, x, y, z, yaw?, size?, r?, g?, b?, a?, max_w?, max_h?)   -- palavras no mundo, só dentro de um pass
core.render.camera_read(id)                      -- a sua última imagem, 64 x 64 em base64 ("" se ainda não há)
core.render.photo(id, data)                      -- uma câmera feita dessa imagem, nunca desenhada
core.overcast(how_much)                          -- céu e nuvens cinzentos, 0..1: o tempo (mods/weather)
core.sky_flash(how_much)                         -- a luz de um raio na terra e no céu, neste frame
core.sky_eclipse(how_much)                       -- o sol negro num anel, o céu e a terra escuros, 0..1
core.cloud_sea(base, top)                        -- as nuvens do céu de novo entre essas alturas: um mar sob ilhas (games/skies)
core.node_foliage(id, on?)                       -- um bloco que pega a cor da estação
core.foliage(r, g, b, how_much)                  -- a cor da estação na folhagem, 0..1

core.hud.pass(name, order, fn)                   -- na tela, uma vez por frame
core.hud.text(x, y, text, size?, align?, r, g, b, a)
core.hud.rect(x, y, w, h, r, g, b, a)            -- só dentro de um hud pass
core.hud.image(name, x, y, w, h, a)              -- um PNG da mídia do mundo
core.hud.item(name, x, y, side)                  -- o quadrado de um item, como na bolsa
core.hud.mouse() -> x, y                         -- o cursor, em frações da tela

core.event.define(name, { fields = {...}, cancellable = bool })
core.event.on(name, priority?, function) -> ref
core.event.emit(name, { fields }) -> cancelled
```

`register_node` devolve o id atribuído: **um mod nunca vê um número que não pediu**, e nenhuma parte
do motor sabe mais qual é o id da pedra. Registrar o mesmo nome duas vezes devolve o mesmo id -- é o
gancho em que a recarga a quente se apoia.

`on_step(dt)`, se definida como global, é chamada uma vez por frame; `on_save`/`on_load` servem à
recarga. Um mod sem `on_step` não paga nada por frame além da busca da global.

`meta_set`/`meta_get` são **o que uma célula lembra**: sobrevive ao fechamento do mundo, e vai
embora quando a célula vira ar -- quebrar um bloco leva o estado junto, então o bloco colocado depois
não herda o do anterior. Mudar o *id* não o apaga: um botão que passa de cima para baixo é a mesma
célula com o mesmo estado.

**Strings, e só strings**, de propósito. Um pedestal que salva o id 41 está salvando o que quer que
41 signifique da próxima vez que o mundo abrir, porque os ids são atribuídos na ordem de registro;
um **nome** sobrevive à chegada de um mod novo. Quem quer um número escreve `tostring` e lê
`tonumber`, que é uma conversão que dá para ver. Limites: nome do campo 1-64 bytes, valor ≤ 256
bytes, 16 campos por célula, 65536 células por mundo -- recusados, não cortados, com o motivo no log.

`storage_set`/`storage_get` são **o que um mod lembra**, sem ligação com uma célula: strings sob um
nome, só do mod que as escreve, salvas no arquivo do mundo com os chunks (só as chaves que mudaram).
O arquivo é lido depois que os mods carregaram, então leia de novo a partir de `core:world_ready`.
Sem arquivo de mundo, e no cliente jogando sozinho, elas duram enquanto o processo roda; o código que
um mundo envia ao cliente não tem nenhuma (0 e ""). Limites: chave 1-64 bytes, valor ≤ 4096 bytes,
4 MiB de chaves e valores por mod (até 2026-09-28 eram 1024 chaves). `games/track` guarda ali a
melhor volta de cada jogador (`best:<nome>`) e o placar (`board`).

`shared_storage_set`/`shared_storage_get` são o mesmo, mas **um para todas as salas** do servidor:
no arquivo de mundo da primeira (`--room`, o sandbox), que toda sala lê e escreve. Vai ali o que é
do jogador onde quer que ele esteja -- a fome (`hunger`), a experiência (`enchant`), as chaves vistas
(`keys`), o contador que numera as mochilas (`backpack`) -- e não o que é de um lugar (uma cama, uma
mesa), que fica em `storage_set`. Um valor guardado na memória precisa ser lido de novo quando o
jogador volta: nesse meio-tempo ele pode ter mudado em outra sala. Num servidor de um mundo só é o
mesmo armazenamento de `storage_set`. O conteúdo de uma mochila (um contêiner carregado, abaixo de
`kCarriedBelow`, no número da mochila) é levado pelo próprio servidor para a sala onde quem a carrega
entra.

`notice` põe uma frase na tela de um jogador por alguns segundos, onde vai o motivo de uma mudança
recusada. Serve para quem **muda** o que o jogador fez em vez de recusar: um mod que troca o bloco
colocado (`ev.id = ...`) precisa dizer isso, ou o jogador vê um bloco que não tinha na mão.

Um handler que escreve no payload uma chave que o evento não tem -- `ev.cancelled` em vez de
`ev.cancel` -- é apontado no log, uma vez por mod, evento e chave.

`players`/`player_pos` são **quem está aqui e onde**. Duas chamadas e não uma: "quem está aqui" e
"onde está esse" são perguntas diferentes, e uma só que devolvesse uma lista de posições
responderia ao botão e deixaria de mãos vazias o pedestal, que precisa dar algo a um jogador **com
um nome**. Observadores -- um portal olhando para dentro -- **não estão aqui**: não têm avatar nem
inventário (DESIGN §4.4), então não há ninguém para pôr num botão.

`player_pos` devolve **os pés**, e é a posição que a autoridade de movimento *aceitou*, não a última
que um cliente alegou. Para quem não está neste mundo não devolve nada:

```lua
local x, y, z = core.player_pos(id)
if x == nil then continue end   -- saiu entre as duas chamadas
```

Não `0, 0, 0`: a origem é um lugar de verdade, e um mod que medisse uma distância ali acharia todo
mundo em pé no botão no canto do mundo.

Essas duas são também as primeiras a usar **duas formas de retorno novas** do gerador -- uma lista e
vários valores. Antes do M8, `core.*` era "escalares entram, um escalar sai", e a alternativa era
escrevê-las como cinco chamadas (`player_count`, `player_at`, `player_x/y/z`) onde duas bastam.

`take_item`/`give_item` são **a bolsa de um jogador**, pelo nome e não pelo id, pelo mesmo motivo de
`meta_*`. As duas devolvem **quantos se moveram de verdade**, que é a única resposta honesta: uma
bolsa pode não ter o que se pede, e pode estar cheia. O que não cabe com `give_item` **não é jogado
no chão nem perdido**: nada acontece com ele, e o mod decide -- `core.drop_item` o põe no chão onde
cai a pilha de um bloco quebrado, e não fazer nada deixa para o jogador abrir espaço. Um motor que o
jogasse fora decidiria por todos os mods de uma vez. O jogador é avisado: a bolsa é enviada de novo
a ele no tick seguinte, para que ele não seja desenhado segurando o que um mod tirou.

`find_nodes` são **as células que um mod não colocou**: o equivalente ao LBM do motor de partida. Um
mod que pôs os próprios blocos poderia simplesmente lembrar onde; isto é para os que um *jogador*
pôs, ou o mapgen. Triplas planas e não uma tabela por célula, que seria uma alocação por célula na
única chamada feita sobre uma região inteira: `#found // 3` é quantas são.
Um nome que termina em `*` é todo bloco cujo nome começa assim, numa só passada pela caixa:
`"cinnabar:dust*"` acha o pó em todas as formas em que ele é desenhado.

Dois limites, declarados e não escondidos. Voltam no máximo **512 posições**, então uma busca que
devolve exatamente 512 provavelmente achou mais e deve ser refeita numa caixa menor. E uma caixa de
mais de **um milhão de células** é **recusada**: a busca roda dentro do tick, e o orçamento de
interrupção que para um mod descontrolado conta safepoints na VM, não o tempo que o motor gasta em
nome do mod. Um **nome que ninguém registrou** também é recusado em vez de responder "não há
nenhum": um erro de digitação que responde zero é um mod que não faz nada e um relatório dizendo que
a busca está quebrada. As duas recusas devolvem uma **lista vazia** -- uma lista não tem lugar para
uma frase -- e o motivo vai para o log.

### Entidades que o mod possui: `core.ecs`

```lua
core.ecs.component("turret:pellet", { "vx", "vy", "vz", "age" })
core.ecs.phase("turret:motion", 100)
core.ecs.phase("turret:impact", 200)

core.ecs.system("turret:fly", "turret:motion", "turret:pellet", function(b)
    for i = 1, b.n do
        b.x[i] += b.vx[i] * b.dt
        b.age[i] += b.dt
    end
end)

local e = core.ecs.spawn("turret:pellet", 32.5, 22.0, 32.5)
core.ecs.set(e, "vx", 0.4)
```

O lote `b` que um sistema recebe tem `b.n` (quantas entidades), `b.dt`, `b.tick`, `b.id` e **uma
coluna por campo**: `b.x`, `b.y`, `b.z` e os campos do componente. São arrays, indexados a partir de
1, e **o que se escreve neles é lido de volta**: um sistema que moveu algo o moveu.

Três coisas que isto faz e uma tabela Lua percorrida em `on_step` não faz:

- **são as entidades do motor.** Um projétil é uma entidade no mundo onde vivem os mobs e as pilhas
  no chão. Um segundo armazenamento seriam dois espaços de ids para uma coisa só.
- **uma chamada para muitas entidades.** 512 entidades, uma chamada. Uma chamada por entidade gasta
  mais tempo atravessando a fronteira do que lá dentro.
- **fases ordenadas por um número.** Dois mods que nunca se conheceram rodam numa ordem declarada em
  vez da ordem em que carregaram. Deixe os números espaçados de propósito: quem quer rodar entre dois
  não deve ter que pedir a ninguém para renumerar.

Limites, declarados: um sistema nomeia **um** componente e recebe quem o tem -- não há junção entre
dois, e quem quer um segundo o lê pelo id, que é uma chamada por entidade e devolve exatamente o que o
lote existia para evitar. Os campos são **números**; strings moram numa célula (`meta_set`) ou dentro
do mod. Um lote tem no máximo **4096** entidades, então um mod com mais recebe mais chamadas. E as
entidades vivem no mundo que possui a tabela de sessões: um servidor offline (`--ticks` sem
`--listen`) não tem nenhuma.

Uma fase declarada duas vezes com **dois números diferentes** é um erro que nomeia os dois mods:
manter a primeira em silêncio poria um dos dois numa ordem que ele não pediu sem nunca avisá-lo.

### Um canal por mod: `core.net`

```lua
local channel = core.net.open()         -- enquanto o mod carrega, não depois
core.event.on("core:net_message", function(ev)
    core.net.send(ev.player, `recebido: {ev.text}`)
end)
```

A mensagem **não carrega o nome do mod**: o nome é o **canal**, que é o que o handshake distribuiu.
Um nome no payload seria uma segunda resposta para "de quem é isto".

Pedido no **carregamento**, porque os números saem na mensagem de boas-vindas: um mod que pedisse
depois que o primeiro cliente se conectou teria um número que nunca foi dito a esse cliente, e
`core.net.open` nesse caso não responde nada e escreve isso no log. Uma mensagem é uma string de no
máximo **512 bytes**.

Do lado do cliente o número é **o do servidor**: `core.net.open` não responde nada até haver uma
conexão, e um mod do cliente não precisa dele -- envia com `core.net.send` e ouve com
`core:net_message`. `player` é ignorado desse lado, porque um cliente fala com o servidor e só.

### Desenhar: `core.render`

```lua
core.render.pass("hologram:light", 100, function()
    core.render.box(34.5, 22.0, 32.5, 1.5, 1.5, 1.5, 255, 0, 255, 255)
end)
```

Um **pass** não é o evento `core:tick`: um handler de tick roda *entre* os frames, sem câmera
montada e sem nada onde desenhar, e roda uma vez por frame -- enquanto um portal desenha este mundo
de novo a partir de outra câmera. Um holograma desenhado a partir de um tick faltaria em cada
abertura. Um pass roda dentro da renderização, **uma vez por câmera**, e desenhar é a única coisa
que ele pode fazer: `core.render.box` fora de um pass é recusado e diz isso.

`core.render.model(id, x, y, z, yaw, scale)` desenha no mesmo ponto do frame um modelo do mundo (um
id de `core.model_id`), parado na sua pose de repouso, com os pés em `x, y, z` e girado por `yaw` em
torno da vertical (e antes, se dados, inclinado por `pitch` em torno do seu x e por `roll` em torno
do seu z: uma carta segurada na mão, `mods/cards`): algo cujo lugar só esta máquina sabe, como o
cubo nas mãos de um jogador em `games/chambers` (que o servidor não envia a ele, `core.mob_unseen`).
`dissolve`, de 0 a 1, é quanto dele já sumiu: o shader do modelo o consome em manchas de um ruído
fixo no modelo, com as bordas queimando em azul elétrico e o que sobra ficando azul, o interior
aparecendo pelos buracos -- o mod conta o tempo, como para `t` abaixo (um cubo que um campo desfaz
em `games/chambers`). `alpha` abaixo de 1 o desenha transparente (nunca abaixo de 0.05), e `tint`
são as suas três cores de tinta como as de uma peça vestida (`core.player_wear`): o kart fantasma
de `games/track`. Devolve false para um modelo que este cliente não tem.

`core.render.fx(kind, x, y, z, size, r, g, b, a, t)` desenha um **efeito de luz** com os shaders do
motor (`src/client/render/fx_render.cpp`): ele se soma ao que está atrás, as paredes o cobrem e ele
não cobre outros efeitos. `kind` é `"glow"` (um brilho), `"flame"` (uma chama que sobe), `"ring"`
(uma onda de choque deitada no chão), `"frost"` (lascas de gelo no chão), `"bolt"` (um raio de
`x, y, z` até `x2, y2, z2`), `"slash"` (o arco de um golpe de espada), `"sparks"` (faíscas voando),
`"vortex"` (um redemoinho) ou `"jet"` (uma chama saindo de `x, y, z` em direção a `x2, y2, z2`, o
nitro dos karts), `"streak"` (uma estrela cadente, a cauda em `x, y, z` e a cabeça em `x2, y2, z2`)
ou `"aurora"` (uma cortina de luz de altura `size` em pé sobre a linha de `x, y, z` até `x2, y2, z2`,
com raios correndo por ela, violeta no alto) -- ou `"aura"` (chamas subindo em volta de um corpo em
pé, `size` metade da sua altura, ocas onde está o corpo, misturadas por cima do que está atrás para
que o vermelho continue vermelho contra um céu claro: o fogo de `games/platformer`) ou `"smoke"`, o
único que não é luz: uma baforada que incha e se dissipa conforme `t` passa, misturada por cima do
que está atrás na sua própria cor (fumaça cinza escurece e esconde; o fumigador de `mods/bees`).
`size` é o raio em nós (para o raio, a sua largura), `t` quanto da sua vida já passou, de 0 a 1: o
shader cresce, varre e se apaga com ele, e o mod só conta o tempo. Devolve false para um tipo que
não existe.

**Câmeras e telas.** `core.render.camera(id, x, y, z, yaw, pitch, fov)` -- a qualquer momento, não
só num pass -- põe uma câmera em `x, y, z` olhando ao longo de `yaw` (0 para -z, como o de um corpo)
e `pitch` (para cima é positivo), com `fov` graus de abertura (70 se não for dito); chamada de novo,
ela a move, e `core.render.camera_drop(id)` a esquece; um oitavo argumento `true` a deixa **parada**:
desenhada uma vez e guardada, uma fotografia (se movida, é desenhada mais uma vez). Dentro de um
pass, `core.render.screen(id, x, y, z, yaw, w, h, plain)` mostra a imagem dessa câmera num retângulo
em pé de `w` por `h` em torno de `x, y, z`, virado ao longo de `yaw`, iluminado por si mesmo como uma
tela -- ou, com `plain`, como uma foto impressa num quadro, sem linhas de varredura; uma câmera ainda
sem imagem mostra chuvisco. O motor desenha a imagem de uma câmera (128 pixels de lado, 96 nós de
profundidade, com os corpos do seu mundo e os passes dos mods) só enquanto uma tela dela foi mostrada
no último segundo, no máximo duas imagens por frame e cada uma no máximo a cada quatro frames: a
imagem de uma tela fica um pouco atrasada, e sessenta e quatro câmeras por mundo, contando as
paradas, é o máximo. Os ids de um mod são só dele. Só no cliente, como todo `core.render`; o que uma
câmera vê o servidor não sabe (`mods/cameras` calcula sozinho o que vê, com raios).

**Palavras no mundo.** Dentro de um pass, `core.render.text(text, x, y, z, yaw, size, r, g, b, a,
max_w, max_h)` escreve `text` com a fonte do jogo num plano em pé, com as linhas (separadas por
`\n`) cada uma centralizada e o bloco delas centralizado em `x, y, z`, virado ao longo de `yaw` como
uma tela (0 para +z), `size` a altura de uma linha em nós (0,1 se não for dito), branco se nenhuma
cor for dita. Com `max_w` e `max_h` (> 0), ele é diminuído -- nunca aumentado -- para caber nessa
caixa. Cada linha é antes passada para o idioma do jogador (`assets/lang`), como o texto do hud.
Paredes na frente o escondem, ele não esconde nada, e só é visto de frente. `mods/building` escreve
as suas placas com ele.

Uma câmera parada é desenhada no primeiro frame depois de criada, mostrada numa tela ou não: o
momento em que ela é criada é o momento da foto. **Uma foto que viaja:** `core.render.camera_read(id)`
dá a última imagem desenhada pela câmera `id` como 64 por 64 pixels (vermelho 5 bits, verde 6, azul
5, dois bytes cada, as linhas de cima para baixo) em base64, 10924 caracteres -- `""` enquanto não
tem nenhuma --, que um mod pode enviar em pedaços e guardar; `core.render.photo(id, data)` faz da
câmera `id` essa imagem, em outra máquina ou em outro dia: as suas telas mostram esses pixels e ela
nunca é desenhada. `mods/album` envia ao servidor cada foto que a máquina de quem a tirou desenhou, e
a máquina de todos os outros a mostra como ela era.

O número ordena os passes **entre si**, não em relação ao desenho do motor: há um único ponto no
frame onde os mods desenham, depois do mundo opaco e dos corpos e antes das aberturas.

Só no cliente. Num servidor, `core.render.pass` diz "este processo não desenha" em vez de falhar de
um jeito genérico.

O que **ainda não existe**: um `core.ecs` que saiba juntar dois componentes. As entidades de um mod
visíveis para os outros jogadores existem: `core.ecs.visible`, abaixo.

### Um bloco que pede uma ferramenta

`register_node(..., dig_level = 2)` declara um bloco que **não sai** se não houver na mão pelo menos
uma ferramenta de nível 2. Não "sai mais devagar": não sai, por mais que você segure.
`register_item(..., dig_level = 3)` declara a ferramenta do outro lado.

```lua
local HARD  = core.register_node("mymod:hard", 90, 90, 110, true, 0, false, 2)
local POINT = core.register_item("mymod:point", 1, "", 3)
```

Zero é a **mão nua**, e é o que vale para todo bloco e item que não diz nada -- ou seja, tudo o que
existia antes de os níveis existirem.

É um **nível, não um tipo**: um machado de nível 2 cava pedra como uma picareta de nível 2. Grupos
são outra coisa, maior; o nível é a metade que faz uma progressão.

E é separado de `breakable`, que é um não diferente: o chão do mundo não é um bloco muito duro, é o
chão, e não sai com nenhuma ferramenta que possa vir a existir. É por isso que a recusa pela
ferramenta diz o que é preciso -- "that needs a level 2 tool, and this one is level 0" -- e a do chão
não: mandar alguém procurar uma ferramenta melhor para a rocha matriz é mandá-lo procurar algo que
não existe.

### Um bloco que leva tempo

`register_node(..., dig_level, hardness)` diz quantos **segundos** leva com a mão nua;
`register_item(..., dig_level, dig_speed)` diz quantas vezes mais rápido uma ferramenta cava. Zero
segundos é instantâneo, e é o que vale para todo bloco que não diz nada.

```lua
core.register_node("mymod:granite", 120, 120, 130, true, 0, false, 2, 4.0)
core.register_item("mymod:point", 1, "", 3, 4.0)   -- quatro vezes mais rápido
```

Quem cava segura o botão. **O relógio é do servidor**: o primeiro golpe numa célula começa a
escavação, o que vem depois dos segundos a quebra, e no meio não acontece nada -- duas mensagens por
escavação, não sessenta, e um cliente que se apressa manda o segundo golpe cedo demais e não ganha
nada. Mexer a mira ou soltar recomeça do zero, e o mesmo acontece se o bloco mudar embaixo.

Uma barra aparece embaixo da mira enquanto a escavação anda. Em zero nada é desenhado, então um bloco
instantâneo nunca a faz aparecer.

**Os blocos do motor declaram os seus segundos**: pedra 1,5, madeira da moldura 0,9, vidro 0,35, e
terra, areia e neve abaixo de meio segundo. Nenhum deles pede uma ferramenta -- `dig_level` continua
0 em todos, porque ferramentas são conteúdo e moram num mod, e um bloco do motor que exigisse uma
seria um bloco que ninguém consegue cavar com os mods desligados. A rocha matriz não tem dureza
nenhuma: uma duração não é uma recusa, e um chão tornado intransponível por ser muito lento é um chão
que alguém fura numa tarde.

### Uma ferramenta serve para um tipo de trabalho

`register_node(..., material)` diz do que o bloco **é feito**; `register_item(..., digs)` diz para
que a ferramenta **serve**. Nível e velocidade valem contra esse material e são os de uma mão nua
contra todo o resto -- e é só isso que faz um machado ser diferente de uma picareta.

```lua
core.register_node("mymod:granite", 120, 120, 130, true, 0, false, 1, 4.0, "stone")
core.register_item("mymod:pickaxe", 1, "", 1, 5.0, "stone")
core.register_item("mymod:shovel",  1, "", 1, 4.0, "soil")
```

A pá acima é **melhor que a picareta em todos os números que tem** -- o mesmo nível, e na terra ela
vai quatro vezes mais rápido que uma mão -- e contra o granito não vale nada: contra `stone` ela é
nível 0 e velocidade 1. Um nível diz o quanto uma ferramenta é boa, um material diz **em quê**, e sem
o segundo toda ferramenta é a mesma ferramenta com um número diferente em cima.

`register_item(..., damage)`, depois de `digs`, diz quantos pontos tira um golpe dado com esse item
na mão: 0 é o de um soco (1). O golpe leva o slot da hotbar e o servidor lê o número da **sua**
tabela, então um cliente não pode alegar que bate mais forte.

```lua
core.register_item("tools:sword", 1, "", 0, 1.0, "", 4)   -- 4 dos 20 de um corpo
```

Vazio de um lado ou do outro quer dizer "ninguém disse", e aí a regra não se aplica: uma ferramenta
que não nomeia material é geral (o seu nível vale em todo lugar) e um bloco que não diz do que é
feito é cavado por qualquer ferramenta pelo que ela vale. São as duas formas de ficar calado, e é o
que é todo bloco e item registrado antes de os materiais existirem.

A recusa diz isso: "that needs a level 1 tool for stone, and this one is level 0 against stone". O
número sozinho soaria como mentira para quem segura um machado de nível 3: o machado **é** de nível
3, o que ele não é é uma ferramenta para isto.

Os blocos do motor usam quatro nomes -- `stone`, `soil`, `wood`, `glass` -- e não um por bloco: um
material para cada bloco é uma tabela onde cada ferramenta é a sua própria ferramenta, uma lista e
não uma regra.

### Um bloco que não é um cubo

`core.node_box(id, x0, y0, z0, x1, y1, z1, piece)` acrescenta uma caixa à forma de um bloco. Um que
declara pelo menos uma **deixa de ser um cubo**: o mesher deixa a sua célula de lado e as caixas são
desenhadas por um passo separado.

```lua
local CHEST = core.register_node("chest:box", 150, 110, 60, true, 4, true)
core.node_box(CHEST, 0.00, 0.00, 0.00, 1.00, 0.62, 1.00)
core.node_box(CHEST, 0.03, 0.62, 0.03, 0.97, 0.88, 0.97, "lid")
```

As coordenadas estão no **espaço do nó**: 0 é uma parede da célula, 1 a oposta.

As caixas **são** o bloco, não um desenho dele: um corpo é parado por elas e não pela célula, um
raio as encontra e atravessa o resto da célula, e o contorno que o jogador vê é desenhado em volta
delas. Num bloco de meia célula de altura você pisa a meio nó do chão e anda na outra metade da sua
célula. `mods/steps` é a parte do jogo que põe isso na mão: uma laje e uma escada.

Você sobe nele **andando**, até 0,55 de um nó: é isso que faz de uma escada uma escada. Mais alto
você pula, e um nó inteiro é sempre um pulo -- um cubo continua sendo uma parede.

A forma **viaja**: desde o protocolo 27 as boas-vindas a levam ao lado do nome do bloco, então até
um cliente sem o seu mod vê as caixas certas e anda sobre elas como um que o tem. A do servidor
sempre ganha, mesmo num nome que o cliente já conhece.

### Uma peça de pista: `core.node_piece`

Peças de estrada como nos jogos de corrida acrobática: uma estrada lisa numa grade de 8 nós, que se
encaixa na grade quando colocada e se junta à peça ao lado onde as bordas coincidem. O bloco é o
canto da peça; as outras células são um bloco de "preenchimento" (caixas embaixo da estrada, que se
pisa, nunca desenhadas).

```lua
local FILL = core.register_node("track:fill", 60, 62, 70)
local BEND = core.register_node("track:banked_bend", 64, 66, 76, true, 0, "upright")
-- plan 2 = curva à direita, raio de 2 quadrados, borda esquerda 4 nós mais alta
core.node_piece(BEND, FILL, 2, 2, 0, 0, 0, 4, 4, true)
core.place_piece(96, 20, 88, BEND, 2)   -- entrada no quadrado de (96, 88), para +z
```

O kart corre na estrada de verdade (uma malha, a mesma no cliente e no servidor); um pé, um raio e as
verificações do servidor veem as células de preenchimento, sempre embaixo da estrada. Cavar
qualquer célula tira a peça inteira; mirar nela mostra o contorno da peça inteira, e com uma peça na
mão a própria peça, transparente, onde ela vai ficar (vermelha se fosse ficar sobre outra peça ou
cobrir o jogador). Uma peça toma o lugar do terreno e dos blocos que encontra nas suas células e
esvazia as 3 células acima da estrada; só outra peça de pista a impede; Z e X a giram um quarto antes
de colocar. Na bolsa, o ícone de uma peça é a sua planta vista de cima. `plan` 3 e 4 são uma chicane,
à direita e à esquerda. Com `wall = true` (último argumento, só retas) bank0 e bank1 são graus de
rolagem da estrada em torno do seu centro: em 90 ela vira uma parede, e um kart fica nela enquanto
for rápido (a partir de 10 m/s, totalmente a partir de 16). `plan` 5 é uma curva à esquerda;
`width = 2` (último argumento) deixa a estrada com dois quadrados de largura, e uma peça larga é
colocada a partir do quadrado da sua faixa da direita. `plan` 6 e 7 são um loop (fechado ou aberto,
2 ou 3 quadrados, sem zebras): a estrada sobe, passa por cima de cabeça para baixo e desce na faixa
ao lado, à esquerda (6) ou à direita (7); o kart gruda nela como numa parede, e devagar cai do alto.
Desenrolado, um loop é uma chicane: a troca de faixa é feita no volante. `plan` 8 e 9 (só
`width = 2`) estreitam a estrada larga para a sua faixa da direita ou da esquerda em dois quadrados;
colocadas ao contrário, elas a alargam. Uma descida é uma subida colocada ao contrário. Exemplo
completo: `games/track/mods/track_core` (todas as peças e os circuitos que ele constrói, `/pieces`
para tê-las na bolsa, com as ferramentas de linha de largada e checkpoint que `track_game` lê).

Na pista (`games/track`), `/pieces` dá primeiro três ferramentas, lidas por `track_game`:

- **Linha de largada**: clique direito numa estrada ou numa peça põe uma linha de largada de través,
  da largura da estrada (12 de cada lado no máximo), cruzada indo para longe de quem clicou; uma viga
  branca em cima. Uma linha nova é uma pista nova com o seu placar (`customN`), os seus fantasmas e
  um dono, quem a pôs; um mundo guarda quantas forem postas, oito por jogador. Clicada de novo pelo
  dono, ela é removida junto com a pista inteira; a outro jogador diz de quem é.
- **Checkpoint**: o próximo checkpoint da sua última pista, viga amarela; clicado de novo pelo dono,
  é removido.
- **Fora de estrada**: dois cliques são dois cantos opostos de uma área (256 de lado no máximo) onde,
  durante uma volta, o kart pode estar em cima de qualquer coisa; um poste luminoso em cada canto, um
  clique dentro a remove.
- **Portal** (`track_core:portal_tool`): um clique numa estrada põe o portal de entrada de través, da
  largura da estrada e quatro de altura, o clique seguinte o de saída, virado para onde quem clica
  olha: um kart que entra no primeiro sai do segundo embalado. O salto não cruza as linhas, então um
  atalho que pula um checkpoint não fecha a volta. Um clique em qualquer um dos dois tira o par.

E o **editor** (`track_core:editor`, de `track_core`): clique direito numa peça segue a sua pista
para trás até o começo e para a frente até a ponta aberta, e abre um painel com as peças que se
encaixam ali -- mesma largura, mesma inclinação e mesma inclinação lateral na entrada que a ponta
deixa (a metade que falta a uma subida, a saída de uma curva inclinada...), também colocadas ao
contrário ("rev"). Um clique a coloca ali (`core.place_piece`) e o painel é refeito a partir da nova
ponta; **desfazer** remove a última colocada pelo painel (`core.take_piece`); **fechar circuito**
procura (A* nos quadrados da grade, direção e altura) as peças que levam da ponta aberta de volta ao
começo -- retas, curvas, chicanes e `rise` nos dois sentidos, nunca onde já há algo, primeiro no
plano e com subidas só se isso não bastar, e com adaptadores se as duas pontas tiverem larguras
diferentes -- e as coloca. Com o editor na bolsa, toda peça colocada à mão abre o painel sozinha.
**B** com o editor na mão (ou o botão **escolher de longe** do painel, já que com o cursor livre ele
não ouve teclas) escolhe **de longe**, sem limite de distância: o corpo fica onde está
(`core.fixed_view`), a câmera olha de onde fica o começo da peça em teste, já colocada; A e D a
removem e testam a próxima que se encaixa (pulando as que não cabem), **W** a mantém e testa a
primeira a partir da nova ponta, S remove a última mantida, F fecha o circuito, B sai removendo a que
está em teste. No lugar da hotbar há uma grade com todas as peças (`core.hud.item`): acesas as que se
encaixam na ponta, escuras as outras, em amarelo a que está em teste; um clique esquerdo numa peça a
põe em teste (clicada de novo, ao contrário, se couber dos dois jeitos), fora da grade não faz nada.
As teclas são ouvidas pela metade cliente de `track_game` e passadas a `track_core` com o evento
`track_core:pick`; a vista volta com `track_core:pick_view`. A cada peça a pista é encontrada de novo
com `core.piece_entry`, que diz por onde se entra na peça de qualquer célula.

Um corpo que dirige (`core.body_drive`) tem nitro: Shift com o acelerador o empurra até 1,35 vez o
seu `sprint`, gastando em 3 s uma reserva que recarrega em 20; o motor calcula isso igual no cliente
e no servidor.

Fora de uma volta o kart vai aonde quiser; durante uma volta, se ficar fora da estrada, das peças, do
paddock e das áreas por meio segundo, ele é posto de volta na estrada ou na peça plana mais próxima.
O paddock e os acessos às pistas são de terra batida (`track_core:paddock`), permitida numa volta
como a estrada. Linhas e áreas ficam no salvamento do mod (`core.storage_set`).

### Um bloco desenhado como modelo

`core.node_model(id, model)` faz o bloco ser desenhado como um **glTF** em vez de um cubo ou de
caixas: é o mesmo modelo que um corpo veste, declarado com `core.register_model`.

```lua
local M = core.register_model("steps:marker", "marker.gltf")
local B = core.register_node("steps:marker", 200, 170, 90, true, 0, "upright")
core.node_model(B, M)
core.node_box(B, 0.15, 0.0, 0.15, 0.85, 0.6, 0.85)   -- onde o corpo fica
```

**O modelo é a figura, as caixas são o bloco.** Ninguém pergunta a uma malha onde um corpo pode
ficar ou o que um raio achou: uma malha são mil triângulos, uma colisão são quatro caixas. Um bloco
que quer uma forma por baixo do modelo declara as duas coisas, como `collision_box` ao lado de
`drawtype = mesh` no motor de partida.

O modelo é girado em torno do **centro da célula** pelo byte de orientação, como uma caixa, então um
modelo desenhado dentro do cubo unitário com os pés em 0 fica onde o bloco está e olha para o lado em
que foi colocado. Um cliente sem o arquivo desenha as caixas, e se não houver nenhuma não desenha
nada: nunca um cubo, porque um cubo é a única coisa que esse bloco não é.

Limites declarados: um bloco-modelo recebe a luz direcional dos corpos e não a da sua célula, não é
animado e não é cortado por um portal.

O que uma forma **não** muda: um item largado pousa na célula e não na caixa. E o que **não** viaja
com ela: se o bloco é sólido, a sua dureza, o seu nível e o seu material continuam sendo do servidor
-- quem não tem o mod os descobre quando o servidor recusa. TODO.md OPEN-43.

O nome da peça não é decorativo: **uma caixa com nome se mexe**. Ela gira para cima em torno da
própria aresta inferior de trás enquanto alguém tem aquela célula aberta, e volta a descer quando o
último que olhava dentro a fecha. O motor não aprende o que é uma tampa: só pergunta se aquela caixa
tem um nome.

Quem abre e quem fecha são os contêineres que decidem: `core.container_show` põe a célula entre as
abertas, e sair do painel, se afastar demais ou se desconectar a põe de volta entre as fechadas. **É
a CÉLULA que está aberta, não a sessão** -- uma tampa que só quem clicou visse não é uma animação, é
um efeito local.

**Um bit** viaja pela rede, quando muda: o movimento entre fechado e aberto é desenhado por cada
cliente por conta própria, em um quarto de segundo, na sua própria taxa de frames. Um cliente que
entra enquanto um baú já está aberto não sabe disso até ele mudar -- escrito no TODO OPEN-32 em vez
de escondido.

Três consequências, e nenhuma é um detalhe:

- **Ele deixa de esconder os vizinhos.** Uma caixa que ocupa seis décimos da célula não cobre nada,
  então o bloco não é mais opaco, diga o que tiver dito. Uma verdade só: um mod que esquecesse de
  desligar a opacidade acharia um buraco no chão atrás do baú.
- **O giro gira a FORMA.** Um cubo são seis quadrados nas paredes da célula, e girá-lo só pode
  significar trocar as imagens entre esses seis; uma caixa tem uma forma, e gira com ela. A face para
  onde uma caixa olha depois do giro é aquela cuja imagem ela leva e a que a sombreia.
- **As colisões continuam sendo as do cubo.** A forma é como o bloco aparece; a física não ficou
  sabendo. Um baú de seis décimos de altura é pisado como um bloco inteiro.

Ela não entra na malha do chunk porque não caberia: ali as posições são **bytes, uma unidade por nó,
0..32 inclusive**, e oitavos pediriam 0..256, um valor a mais do que cabe num byte. As caixas
visíveis são juntadas em vez disso num único buffer, refeito a cada frame e desenhado numa única
chamada, com **o mesmo shader e o mesmo atlas** dos cubos -- então a luz, o sombreamento por face e o
ciclo do dia são esses, e não uma segunda cópia que vai se afastando.

Num servidor, um cliente sem esse mod vê um **cubo**: a tabela de blocos na rede são nomes, e a
forma ainda não viaja.

### Um líquido: `core.node_liquid`

```luau
local WATER = core.register_node("lakes:water", 52, 104, 178, false)
core.node_liquid(WATER, 150)      -- 0..254: quanto do que está atrás ele esconde
core.node_liquid(RIVULET, 150, 3) -- e quão cheia está a célula, em oitavos (8 = toda)
```

O terceiro argumento, `fill`, é **quão cheia uma célula está** desse líquido, de 1 a 8 oitavos. Ele
viaja na tabela de blocos (protocolo 40), então um cliente sem o mod desenha cada nível na sua altura:
a superfície de uma célula pega, em cada canto, a altura da mais cheia entre as células de líquido
que se tocam ali (uma célula com mais líquido em cima conta como cheia), e assim ela **se inclina** do
lado alto para o baixo e duas vizinhas põem a mesma altura na aresta que dividem. As faces entre duas
células de líquido não são desenhadas: um lago é uma superfície, não uma pilha de paredes
transparentes. Um líquido não passa mais pelo mesher de cubos -- cujos vértices são bytes, uma
unidade por nó --, mas por células com quatro alturas que o renderizador monta em float, como faz com
as caixas.

Um bloco líquido **não para um corpo, não para a luz, é desenhado transparente e não é cavado**. Um
corpo mesmo só em parte dentro dele **nada**: as teclas o empurram a metade do seu passo, ele afunda
devagar (no máximo 2,5 m/s) e sobe segurando o pulo (3,5 m/s). São números de
`core/physics/movement.h`, os mesmos com que o servidor confere o que os clientes alegam: um corpo num
líquido conta como apoiado, então sair de um lago não é voar, mesmo num mundo onde não se voa. Um
bloco colocado numa célula de líquido **toma o lugar** do líquido.

O servidor diz a cada cliente que entra o que cada bloco faz a um corpo e à luz (protocolo 38), então
um cliente **sem o mod** nada no lago como um que o tem -- antes, um bloco não sólido de um mod era
uma parede para quem não tinha o mod.

O que um líquido **não faz**: se mover sozinho. O motor diz que uma célula é líquida, não que ela
escorre -- não há simulação de fluidos em lugar nenhum de `src/`. Quem quer água corrente a escreve
como mod, e `mods/lakes` é o exemplo: uma **nascente** (`lakes:water`, cheia) e seis níveis de água
corrente (`lakes:flow1`..`lakes:flow6`, de dois a sete oitavos), um bloco por nível porque o nível de
uma célula é o bloco dentro dela; um ABM duas vezes por segundo dá a cada célula corrente um passo --
para baixo se embaixo há ar ou água corrente mais fraca, chegando no nível 6 (um riacho desce uma
encosta até onde a encosta for); para o lado um nível mais fraco **só se algo embaixo a sustenta**,
para o ar e **para a água corrente mais fraca**, que ela sobe ao seu próprio nível, e o nível novo
segue ao longo desse riacho; ela pode acabar também em cima de água, de um lago ou de um riacho, mas
ali para, porque água corrente sobre outra água só se junta a ela -- e ela **seca** a que não tem
mais nada em cima nem nada mais molhado ao lado se espalhando. As nascentes não são varridas: um lago
de mil células paradas não custa nada. Só duas coisas abrem caminho para a água, uma célula cavada ao
lado (`core:broken`) e um balde despejado.

### Um bloco com uma frente

`register_node(..., rotatable = true)` declara um bloco que tem uma **frente**. Colocá-lo gira **a
célula**, não o id: a célula leva um byte que diz quantos quartos de volta em torno da vertical ela
está girada, e o mesher lê esse byte para decidir qual imagem vai em qual face. O id no mundo é o que
`register_node` devolveu, sempre.

```lua
local OVEN = core.register_node("mymod:oven", 150, 110, 60, true, 0, true)

core.event.on("core:used", function(ev)
    if ev.id ~= OVEN then return end        -- o id é esse, girado como estiver
    core.log(`forno girado em {core.get_facing(ev.x, ev.y, ev.z)}`)
end)
```

Um mod pode girar uma célula ele mesmo com `core.set_facing(x, y, z, turn)` e lê-la com
`core.get_facing(x, y, z)`. O giro vai de 0 a 3; qualquer outra coisa é ignorada. `core.set_block`
diz **qual** bloco, não como ele está girado: a célula que ele reescreve volta para 0, senão um baú
construído onde havia um forno olharia para onde o forno olhava.

Nem sempre foi assim. Até pouco tempo atrás, registrar um bloco com frente registrava **quatro ids**
-- a base e três quartos de volta -- e o que estava no mundo era um dos três a mais, então um handler
que comparava `ev.id` com o id devolvido não reconhecia três colocações em quatro e o baú só abria
com a mão vazia. Era um defeito real, deste mod. O preço caía em quem chamava: perguntar
`core.node_base` antes de saber que bloco era. Essa chamada não existe mais, e os três ids também
não.

`core.find_nodes` procura um nome e acha as células, viradas para onde estiverem. O que foi embora
com os ids é procurar uma **orientação** -- o velho `mymod:oven^2` achava só aquele giro: quando for
preciso, vai ser um argumento a mais em `find_nodes`, não quatro nomes.

O pacote de texturas pinta as faces pelo nome -- `mymod:oven:front` -- sem saber nada de giros: veja
[TEXTURE_PACKS.md](TEXTURE_PACKS.pt.md).

### Um corpo com um rosto

`core.register_model(name, file)` declara um modelo e devolve um id. O **nome** é o que viaja -- um
corpo na rede leva o id, e o mundo envia a tabela de nomes uma vez só na saudação --, enquanto o
**arquivo** é onde o modelo está *nesta máquina*, ao lado do `init.luau` do mod:

```lua
local PERSON = core.register_model("mymod:person", "person.gltf")
core.player_model(PERSON)   -- o corpo de um jogador
core.mob_model(PERSON)      -- e as criaturas de --mobs
```

### Criaturas

`core.register_mob` declara um **tipo** de criatura; o comportamento é do motor
(`src/core/ecs/mob.h`). Dois jeitos de lutar: `"melee"` se aproxima e golpeia, `"archer"` fica à
distância e atira flechas enquanto vê o jogador.

```lua
local ZOMBIE = core.register_model("mymod:zombie", "zombie.gltf")
--                nome            modelo  vida vel.  luta     dano alcance cada
core.register_mob("mymod:zombie", ZOMBIE, 20,  2.2, "melee",  3,    1.6,    1.0)
core.spawn_mob("mymod:zombie", x, y, z)   -- uma, agora, ali
```

Criaturas de todo tipo registrado aparecem **no escuro** (luz 7 ou menos) entre 18 e 40 nós de um
jogador, até seis perto de cada um, miram no mais próximo dentro de 16 nós e somem quando ninguém está
a menos de 72. O modelo toca animações pelo nome como um jogador (`idle`, `walk`, `run`, `jump_loop`,
`swim`) e, enquanto golpeia ou puxa o arco, `attack` ou `shoot`. Só no servidor: um mundo local não
tem criaturas. `mods/mobs` é o exemplo completo.

Uma criatura que morre (ou sai do alcance) não some de uma vez: o cliente a deixa onde estava e a
**dissolve** em 1,2 s, tocando uma vez a animação `death` do modelo se ela tiver uma, parada na última
pose se não tiver. Só do lado do cliente, sem mensagem a mais: a lista dos corpos próximos chega
inteira em cada mensagem.

**Toda criatura nova precisa ter a sua animação `death`**, no modelo passado a `core.register_mob` e
em cada variante que ela veste (domesticada, tosquiada, filhote): uma animação que toca uma vez e
fica parada na última pose, mais ou menos um segundo. O servidor confere isso quando a criatura é
registrada e escreve `register_mob('...'): its model has no "death" clip` no log se faltar. As do
projeto:

| criaturas | como morrem | de onde |
|---|---|---|
| zumbi, esqueleto (`mods/mobs`), brute (`games/inferno`) | de lado | `tools/export_mob_models.py` (o brute copia o zumbi: depois `tools/make_inferno_models.py`) |
| cachorro, gato (`mods/pets`), porco, vaca, ovelha, galinha (`mods/animals`), com filhotes, tosquiadas e domesticados | de lado | `tools/make_pet_models.py`, `tools/make_animal_models.py` |
| espectro, brasa (`games/inferno`) | o espectro se inclina, afunda e enrola os tentáculos; a brasa deixa cair os bastões e depois a cabeça | `tools/make_inferno_creatures.py` |
| ghoul, imp, pinky, gazer (`games/yafps`) | cada um do seu jeito | `tools/make_fps_models.py` |
| humano, sapo, toupeira (`mods/villagers`), robô ajudante (`mods/helpers`) | de lado | `tools/make_villager_models.py` |
| bacteria, smiler, palhaço (`games/backrooms`) | a bacteria cambaleia e cai dura para a frente; o smiler fecha o sorriso e desce ao chão girando; ao palhaço cedem os joelhos e cai para trás | `tools/make_backrooms_creatures.py` |
| formiga, joaninha, borboleta (`mods/garden`) | de lado; a borboleta fecha as asas antes | `tools/make_garden_insects.py` |
| fantasma amaldiçoado (`mods/ghost`) | o seu corpo não se vê; o mod desenha o fantasma subindo enquanto se desfaz (o dissolve de core.render.model) | `tools/make_ghost_body.py`, o aspecto do Meshy com `tools/meshy_prop.py` |

No Blender o mais simples é `tip_over(rig, [mesh])` de `tools/make_pet_models.py`: a criatura cai de
lado, levantada metade da sua largura para não afundar no chão, com qualquer rig; `drop=` a faz
também cair, para uma que voa. As animações que o motor toca pelo nome, todas opcionais menos
`death`: `idle`, `walk`, `run`, `jump_loop`, `swim`, `swim_idle`, `attack` (corpo a corpo) ou
`shoot` (arqueiro), `sit` (um animal sentado), `death`; um trabalhador, as que o seu mod nomear.

Um tipo com luta `"pet"` é um **animal doméstico**: não ataca ninguém, aparece **de dia** em chão
ensolarado (em `spawn_on`, se dado), no máximo seis perto de cada jogador (contados à parte das
criaturas da noite), passeia e para para olhar quem estiver a menos de 4 nós. `core.mob_tame` diz o
que o domestica e o que ele veste depois:

```lua
local DOG, DOG_TAME = core.register_model("mymod:dog", "dog.gltf"),
                      core.register_model("mymod:dog_tame", "dog_tame.gltf")
core.register_mob("mymod:dog", DOG, 16, 3.2, "pet", 0, 1.6, 1.0, "", 1, 0, false, 0, false, "core:grass")
core.mob_tame("mymod:dog", "mobs:bone", DOG_TAME)   -- o item, e o modelo com a coleira
```

Clique direito com o item dele na mão: gasta um e ele é seu. Daí em diante ele segue você (corre se
você ficar mais de 5 nós à frente, reaparece ao seu lado além de 14), nunca some, continua seu mesmo
depois de um reinício do servidor (o motor o salva no arquivo do mundo), e outro clique direito o
faz sentar (animação `sit`) ou levantar. Vale também no mundo local. Animações: `idle`, `walk`,
`run`, `sit`. `mods/pets` é o exemplo completo.

Um tipo com luta `"worker"` é um **trabalhador**: não luta, não aparece sozinho e nunca some; o mod
dele o cria (`core.spawn_mob` devolve o id) e o conduz. O motor o faz andar e mostra o que ele faz:

```lua
local id = core.spawn_mob("mymod:farmer", x + 0.5, y, z + 0.5)
core.mob_goto(id, cx, cy, cz)          -- até aquela célula, pelo caminho de core.find_path
if not core.mob_going(id) then        -- chegou (ou parou onde não há caminho)
    core.mob_face(id, wx, wz)
    core.mob_act(id, "gather", "farm:hoe")  -- animação em loop e item na mão, para todos (protocolo 81)
end
core.mob_act(id, "", "")               -- idle e walk de novo
core.mob_place(id, x + 0.5, y + 1, z + 0.5)  -- posto ali, parado: para subir onde não se anda
core.mob_remove(id)                    -- some na hora, sem morrer: o mod o pega de volta
```

Um clique direito num trabalhador chega ao mod como `core:used_mob` (`player, target, held`); o mesmo
vale para qualquer outra criatura com que o motor não fez nada (nem tosquiou, nem alimentou, nem
domesticou): uma vaca ordenhada, uma ovelha tingida. `core.mob_kind(target)` diz que criatura é. O
item na mão é desenhado como o de um jogador: o modelo `<item>_held` no osso `hand_R`, ou o osso
`tool_<nome>` do próprio modelo. `core.node_yield(id)` diz o que um bloco quebrado dá, para quem cava
no lugar de um jogador. `mods/villagers` e `mods/helpers` são os exemplos.

Um `"pet"` sem `core.mob_tame` é um **animal selvagem**: aparece, pasta e deixa o seu drop, mas
ninguém o domestica. Atingido, ele **entra em pânico**: foge do golpe correndo por cinco segundos.
`mods/animals` (porco, vaca, ovelha, galinha) é o exemplo.

```lua
-- clique direito com a tesoura: 2 de lã no chão, modelo tosquiado por 300 s, e pânico
core.mob_shear("mymod:sheep", "mymod:shears", "mymod:wool", 2, SHEEP_SHORN, 300)
-- um ovo no chão a cada 120-300 s, e mais um entre os drops quando morre
core.mob_lays("mymod:chicken", "mymod:egg", 120, 300)
-- alimentada com grama fica apaixonada por 30 s (HEARTS gira sobre a cabeça): duas perto
-- uma da outra fazem um filhote (metade do tamanho, modelo próprio, fica perto de um dos
-- pais, adulto depois de 1200 s); não de novo antes de 300 s
core.mob_breed("mymod:cow", "core:grass", COW_BABY, 1200, HEARTS)
```

A tesoura se desgasta se tiver `durability` (register_item): uma tosquia gasta um ponto.

O corpo de toda criatura (com que ela anda, passa por vãos e é atingida) é tirado do seu modelo: tão
alto quanto o modelo, tão largo quanto ele é de um lado ao outro, nunca mais que 1,8 × 0,6 e nunca
menos que 0,01 × 0,01 (uma formiga de `mods/garden` tem 2 cm de altura). Duas criaturas não ficam uma
dentro da outra: elas se empurram até a distância entre os seus centros ser a soma das meias larguras
dos seus modelos (estas não limitadas a 0,3: uma vaca mantém 0,375).

As criaturas acham o caminho com `core.find_path`, que um mod pode usar para qualquer corpo que anda
(um animal seguindo o dono, um aldeão voltando para casa):

```lua
-- de, até, e o corpo: 2 células de altura, sobe 1, desce 3, nada; 2000 células no máximo
local way = core.find_path(x1, y1, z1, x2, y2, z2, 2, 1, 3, true, 2000)
for i = 1, #way, 3 do
    local x, y, z = way[i], way[i + 1], way[i + 2]   -- a célula dos pés, passo a passo
end
```

Sem caminho, ou com o orçamento gasto, a lista leva à célula **mais próxima** alcançada: confira se a
última é a que você pediu.

O caminho lê os blocos pela **forma**: meio bloco é um degrau que se sobe sem pular, um painel fino
(uma escada de mão, uma porta aberta) deixa passar ao lado. `core.node_climbable(id)` faz de um bloco
uma **escada de mão**: quem está dentro sobe com o pulo e se segura agachando, e o caminho sobe e
desce por ela. `core.node_door(closed, open)` diz que dois blocos são **uma porta**: o caminho passa
por uma aberta e para diante de uma fechada, a não ser para um corpo que abre portas (`opens_doors`
em `register_mob`). `mods/doors` e as escadas de mão de `mods/timber` são os exemplos.

O modelo de outro mod é procurado pelo nome com `core.model_id("theirmod:kart")`, como `core.node_id`
procura um bloco: 0 se não existe.

Um nome de arquivo que tenta sair da pasta do mod -- com uma barra, uma barra invertida ou um ponto no
começo -- **é recusado** e a chamada devolve 0. Um mod é conteúdo, e conteúdo vem de qualquer lugar.

Declarar o mesmo nome duas vezes **mantém o id**: uma recarga não despe os corpos que o vestem.

Do outro lado, o cliente nunca vê esse caminho. O mundo **serve** o modelo -- o `.gltf` e tudo o que
ele nomeia ao seu lado -- pelo mesmo caminho das texturas, e o cliente o reconstrói numa pasta sua. O
que o mundo não serve é procurado na pasta de modelos do jogador (`--models`), onde `mymod:person`
vira `mymod_person.gltf`. Um cliente sem nenhum dos dois desenha a caixa que desenhava antes --
nunca um buraco onde havia um jogador.

Um modelo viaja **inteiro ou nada**: um `.gltf` sem o buffer que ele nomeia não é meio personagem, é
um personagem com um buraco, e quem olha não tem como saber qual arquivo falta.

Um modelo feito de **partes rígidas** -- um veículo com rodas, como o Sketchfab exporta -- é montado
a partir dos seus nós: cada malha sem esqueleto pega a transformação do nó que a carrega, rotações,
escalas e espelhamentos incluídos. Uma malha com esqueleto é posicionada pelas suas juntas. A frente
de um corpo é -Z: um modelo exportado com o nariz para +Z precisa ser girado no arquivo. O kart de
`games/track`, como saiu do Sketchfab, já olha para -Z.

O mod `mods/chest` é isto, escrito por inteiro: `chest:box` é `rotatable`, e os seus dois handlers --
o que abre o baú e o que o esvazia quando é quebrado -- comparam `ev.id` com o id que
`core.register_node` devolveu, e só. **Um mod que acrescenta um bloco desloca os ids dos registrados
depois dele**: os ids são posicionais. Um arquivo de mundo escrito antes guarda os nomes da sua
tabela, e ao abrir os seus chunks são reescritos pelo nome na nova. Blocos que nenhum mod carregado
registra mais viram marcadores magenta com o seu nome (não dá para segurá-los), e voltam a ser eles
mesmos quando o mod volta; o servidor os lista ao iniciar. Então um id guardado numa variável precisa
ser pedido a `core.register_node` a cada início, nunca escrito à mão.

### Escrever muitas células de uma vez

`core.set_block` escreve o mundo e avisa quem está conectado. Como avisa depende de quantas células
de UM mesmo chunk você mudou naquele tick:

* poucas -- uma por uma, nove bytes mais o motivo, por sessão;
* mais de 150 -- o chunk é tirado do que aquela sessão tem e o streamer o envia de novo inteiro,
  calculado a partir do mundo como ele está agora.

Não há nada para escolher: o resultado é o mesmo mundo nos dois casos. Medido: 640 células de um
chunk são 640 mensagens contra 1 chunk, e quatrocentas células custam 6834 bytes como células e 233
como chunk. Uma sessão para a qual o mundo ainda está chegando não recebe nada até ele chegar -- as
células esperam, não se perdem.

A iluminação não se paga por célula: todo lugar de onde um mod escreve (um tick, um evento, o
carregamento) fica dentro de um `LightRun` do servidor, que refaz a luz uma vez só no fim, para todas
as células mudadas. Uma explosão que cava trezentas células num tick (`games/front`) é um recálculo,
não trezentos. O que sobra para pagar é a chamada e o chunk enviado de novo.

### Gerar uma parte do mundo: `core.register_mapgen`

Uma **etapa** é um pedaço da geração do mundo escrito em Lua. O motor registra uma sua --
`core:terrain` -- e um mod acrescenta outras:

```luau
local ORE = core.register_node("mine:iron", 180, 120, 90)

core.register_mapgen("mine:veins", function(ctx)
    -- ctx.seed, ctx.height, ctx.depth (a rocha abaixo de y 0, 0 se não há), e a coluna a gerar: ctx.min_x..ctx.max_x e
    -- ctx.min_z..ctx.max_z, inclusive, 32 por 32. Não há mais nada dentro,
    -- e nada se escreve dentro: uma etapa escreve com core.set_block e
    -- lê o terreno com core.terrain_height.
    for z = ctx.min_z, ctx.max_z, 8 do
        for x = ctx.min_x, ctx.max_x, 8 do
            local ground = core.terrain_height(x, z)
            if ground > 12 then
                core.set_block(x, ground - 6, z, ORE)
            end
        end
    end
end)
```

**Abaixo da rocha matriz (y 0) há espaço até y -64** (`kWorldBottom`): o motor não gera nada ali, mas
uma etapa pode construir ali, e esses blocos são iluminados, salvos e enviados como os outros; um
corpo só morre abaixo de y -80. `mods/deep` põe ali as suas cavidades.

**O mundo não tem borda, e uma etapa roda uma vez por coluna** de chunks (32×32 blocos, a altura
inteira), quando alguém chega perto dela pela primeira vez. O que ela escreve fora da sua coluna é
**descartado** sem erro: uma estrutura que fica entre duas colunas é escrita pelas duas, cada uma a
sua metade, e é o único jeito de ela sair inteira em qualquer ordem em que as duas colunas forem
geradas. Então uma estrutura que começa numa coluna vizinha (um veio, um túnel, a copa de uma árvore)
precisa ser *seguida* também daqui: `mods/ore` olha os começos de veio até quatro colunas para trás,
`mods/cave` os túneis dos quadrados até quatro mais adiante. E `core.get_block` fora da coluna lê o
que está lá agora, que pode ser nada: uma etapa decide a partir do que está **dentro** da coluna, ou
de funções da semente.

Quatro regras, e são elas que fazem uma etapa diferente de um mod que escreve blocos no
carregamento:

* **A ordem de registro é o contrato.** As etapas rodam na ordem em que foram pedidas -- ou seja, na
  ordem de carregamento dos mods, que `depends` decide -- e uma etapa vê o que as anteriores
  escreveram.
* **Você registra enquanto o mod carrega, não depois.** As etapas rodam em toda coluna nova, por toda
  a vida do servidor -- na região em volta do spawn logo depois que os mods carregam, no resto quando
  for preciso --, mas a lista fecha quando os mods terminam de carregar: pedir uma a partir de
  `on_step` é **recusado** com um erro, em vez de ser aceito e rodar só nas colunas geradas dali em
  diante.
* **Precisa ser determinística.** Mesma semente, mesmo mundo, em qualquer máquina. Nada de relógios,
  nada de `math.random` sem semente, nada que dependa de quem está conectado: um servidor e um cliente
  que discordam sobre como é o terreno são uma dessincronização antes do primeiro pacote.
* **`core.set_block` dentro de uma etapa não custa iluminação.** O mundo é iluminado uma vez só no fim
  da passada, e nenhum chunk precisa ser enviado de novo a ninguém: é o lugar para escrever dezenas de
  milhares de células.

Uma etapa que dá erro para **o seu** mod e nenhum outro: o que ela já tinha escrito fica no mundo, as
etapas registradas depois rodam do mesmo jeito, e o log diz que foi a etapa.

`mods/ore` é este mecanismo em produção e não um exemplo: ele registra uma etapa que enterra carvão e
ferro na pedra, o ferro no nível 1 -- ou seja, atrás da picareta de `mods/tools`. Vale a pena lê-lo
por duas coisas que o exemplo acima não mostra: o hash determinístico escrito à mão (um double guarda
53 bits exatos e o produto de dois números de 32 bits dá 64, então `a * b` joga fora justamente os
bits baixos de que um hash precisa) e a regra que substitui **só** as células que são `core:stone`,
que é o que mantém a etapa fora da superfície, do preenchimento do bioma, da rocha matriz e do que uma
etapa registrada antes construiu.

`mods/cave` é a segunda, e mostra o que a ordem dos mods decide de verdade: a pasta de mods carrega em
ordem de nome, as etapas rodam na ordem em que foram pedidas, e `cave` vem antes de `ore`. Então os
túneis são cavados e *depois* o minério preenche a pedra que sobrou -- e é assim que um veio acaba na
**parede** de uma caverna em vez de dentro de um bloco que ninguém nunca vai olhar. Trocar os dois
nomes muda o jogo sem mudar uma linha de código: é o contrato de que esta seção fala, visto de fora.

As outras duas regras que vale copiar dali: uma caverna por quadrado de 32×32 colunas, escolhida
*dentro* do quadrado em vez de "uma coluna em N", porque um mundo pequeno com um hash azarado seria um
mundo sem cavernas e um teste que só passa com algumas sementes; e o teto mantido seis blocos abaixo
da superfície, conferido a cada passo com `core.terrain_height` da coluna em que o túnel está agora e
não daquela de onde partiu.

`games/backrooms` é a terceira, e um mundo inteiro feito só de etapas (`core.mapgen_core(false)`):
uma grade de salas de 8 que cabe inteira dentro de uma coluna de 32, então cada sala é escrita
inteira e nenhuma estrutura precisa ser seguida a partir de uma coluna vizinha. Duas coisas para
copiar. A planta é uma função de x e z e **não da semente**, e é por isso que o tick também pode
lê-la: ele acha onde estão os portais sem ter visto a etapa rodar, o que num mundo carregado do
arquivo não acontece. E o que atravessa vários níveis (escadas, buracos no chão) é escrito **depois**
de todos os níveis da coluna: `core.replace_nodes(…, AIR, floor)` de um nível feito depois fecharia o
buraco aberto antes.

Uma coluna já gerada e depois deixada (ninguém passa por ali faz um tempo) não é gerada de novo: ela
volta do arquivo do mundo, ou da memória se o servidor não tem arquivo. Uma etapa mudada entre dois
inícios muda só as colunas que ninguém visitou ainda, e a fronteira com as velhas aparece. Um mod que
escreve com `core.set_block` numa coluna que ainda não está pronta não perde a escrita: ela espera a
coluna ser gerada e iluminada, e aí vai. `core.get_block` numa coluna que não existe responde ar.
TODO.md OPEN-35 e OPEN-38 têm os números.

### Como um corpo se move: `core.movement`

```luau
-- andar, correr, voar, pular, gravidade
core.movement(12, 60, 20, 10, -40)
```

Um mundo diz quão rápido um corpo anda, corre, voa e pula, e com que força cai. Isso é **do mundo**
e não do jogador: cada cliente que entra recebe esses números nas boas-vindas e se move com eles, e o
servidor julga as posições com limites tirados dos mesmos números. Então um corpo a 60 m/s não é
trapaça num mundo que declarou isso, e um a 150 é.

* **Enquanto o mod carrega.** A partir de um tick é tarde demais e é recusado: os números saem no
  handshake, e um mundo que os mudasse no meio teria jogadores a quem foram ditos outros.
* **Recusado, com o motivo no log:** uma velocidade negativa, que não é número ou acima de **256
  m/s**; uma gravidade que não é número ou **apontando para cima**. Devolve 1 se os números foram
  aceitos, 0 se não.
* **Voar dobra com a corrida**, como com os números do motor.
* **A velocidade terminal, 60 m/s, não é declarada**: é o que para qualquer queda, seja qual for a
  gravidade, também no cliente.
* **Se dois mods a chamam, ganha o último carregado**, e o log diz de quem ele substituiu os números.
* **Um cliente conectado usa os números do servidor**, mesmo que um mod dele declare outros: o
  servidor é o lado que confere.

O que **ainda não** existe: velocidade por corpo -- um veículo é um corpo que não é um jogador a pé
-- e um servidor que simula o movimento em vez de conferir o resultado. TODO.md OPEN-44.

### O tamanho de um corpo: `core.body_width`

```luau
core.body_width(1.4, 2.6)   -- um kart: 1,4 de largura, 2,6 de comprimento
```

A pé, o lado da caixa quadrada com que um corpo colide, em nós: 0,6 se o mundo não diz nada. A
altura continua sendo a do motor, 1,8. O segundo número só conta num mundo que dirige
(`core.drive`): ali o corpo é um **chassi sobre quatro rodas com suspensão**, de largura `width` e
comprimento `length` (0: tão comprido quanto largo), simulado como corpo rígido (Jolt,
`core/physics/vehicle.h`). Ele gira com o kart, mergulha nas freadas, se inclina nas curvas, e se
entra rápido demais numa curva os pneus não seguram e ele escorrega para fora. O chassi fica a menos
de 26 graus da vertical e as suas rodas não tomam como chão nada mais íngreme que 50 graus: um kart
que raspa numa parede não se deita nela nem a escala.

* **Enquanto o mod carrega**, como `core.movement`: sai nas mesmas boas-vindas, e `core.movement` e
  `core.body_movement` a mantêm.
* **O servidor confere com uma caixa quadrada**: de largura `width` a pé; para um corpo que dirige, o
  quadrado que cabe dentro do chassi girado como estiver, `width / 1.41`. Quem sai de um portal é
  empurrado para longe da parede ao lado em meia largura, ou meio comprimento se estiver dirigindo.
* **Estar DENTRO de um bloco** é a única pergunta que ele faz de outro jeito: a pé ele pergunta com
  essa caixa, dirigindo **só com o centro**. Um chassi gira, se inclina e tem as rodas fora da sua
  caixa, então as duas pontas não concordam sobre o que ele toca, e uma recusa que põe o kart de volta
  onde a caixa do servidor está livre deixa o chassi dentro da parede em que ele raspava -- e o
  servidor o recusa de novo, a cada posição alegada (TODO.md). Um kart pode se encostar numa parede;
  um com o centro dentro dela ainda é posto de volta.
* **Recusado:** uma largura abaixo de 0,2 ou acima de 4, um comprimento diferente de 0 abaixo de 0,2
  ou acima de 8, ou um número que não é número.
* **Um corpo mais largo que um portal no chão não passa por ele**: o servidor isenta só o nó da
  abertura, e a caixa larga afunda também nos nós ao lado.

**Quem não voa não sobe.** Num mundo que dirige (`core.drive`), ou que declara voo 0, o servidor
recusa um corpo que não está apoiado em nada e está mais alto do que onde se apoiou da última vez por
mais de um pulo (v² / 2g, mais 0,3). Um pulo, um degrau e uma aterrissagem num bloco mais alto
passam; uma subida a velocidade de pulo por cima das paredes de dois nós da pista não.

### Um corpo diferente dos outros: `core.body_movement`

```luau
core.event.on("core:net_message", function(ev)
    if ev.text == "entre no carro" then
        -- jogador, andar, correr, voar, pular, gravidade
        core.body_movement(ev.player, 12, 150, 20, 10, -40)
    end
end)
```

`core.movement` diz como os corpos do mundo se movem; esta diz como **um** corpo se move, e pode ser
chamada quando você quiser: um veículo, um power-up, um prisioneiro desacelerado. O jogador recebe os
números numa mensagem própria, e a partir daí o servidor confere aquele corpo com eles.

* **Um segundo de folga.** O cliente continua se movendo com os números velhos até a mensagem
  chegar, então por um segundo o servidor aceita o mais rápido entre os velhos e os novos:
  desacelerar alguém não corrige os passos que ele já tinha dado. O preço, dito: um corpo
  desacelerado tem um segundo em que ainda pode andar na velocidade antiga.
* **Os padrões são os números do motor, não os do mundo.** Para devolver um corpo ao que o mundo
  declarou, passe esses.
* **Recusado, com o motivo no log:** os números que `core.movement` recusa, um jogador que não está no
  mundo, e alguém que só está olhando.
* **Quão alto, o sétimo número (`rise`).** Sem ele, um corpo que não voa pode estar no máximo no topo
  do seu pulo acima de onde se apoiava (para um kart, mais a elevação do chassi sobre as rodas). Com
  ele, o que o mundo disser, até 64: a pista passa 10, e um kart que chega lá quica como contra um
  teto. Quem precisa ficar dentro de um percurso é o mod que decide, não a altura: `games/track` põe
  de volta na estrada mais próxima um kart que está fora dela há meio segundo (`core.player_place`).

O que **ainda não** existe: o servidor não confere direção e inércia. E os números de um corpo não
atravessam um portal para outro mundo, onde valem as boas-vindas daquele mundo, e não são salvos.
TODO.md OPEN-47.

### Voando: `core.body_glide` e `core.body_balloon`

```luau
core.body_glide(player, core.held_item(player) == "glider:glider")   -- mods/glider
core.body_balloon(player, true)                                       -- mods/balloon
```

Mais dois jeitos de **um** corpo se mover, a qualquer momento como `core.body_movement`, enviados ao
seu cliente e conferidos pelo servidor com eles.

* **Sob uma asa-delta**, um corpo que cai mais rápido que 2 m/s é pego pela asa: levado para onde
  olha a 9 m/s descendo 1,6, mergulhando até 18 m/s e 6,6 para baixo quando olha direto para baixo,
  desacelerado até 4 olhando para cima. Agachar fecha a asa e ele cai; o chão termina o voo.
* **Num balão**, segurar o pulo levanta o corpo a 2,5 m/s, agachar o faz descer a 3, e sem nada
  apertado ele desce a 0,25; as teclas o empurram a 2,5 e o vento o leva a 0,8 m/s para +x. No chão,
  ele anda.
* **O servidor os permite**: a uma asa-delta o seu mergulho mais rápido, a um balão qualquer altura.
  E uma descida com um ou com outro não mais rápida do que uma asa deixa descer **não é uma queda**
  (no servidor e num mundo jogado sozinho); uma asa fechada lá no alto cai de onde se fechou.
* Como eles parecem (`core.player_wear`), quando uma asa-delta está pronta, e entrar e sair de um balão
  são coisa do mod.
* **Uma corda sem parkour: `core.body_rope(player, on)`.** Num mundo que não faz parkour, deixa um
  jogador se pendurar em `core.rope` -- só a corda: o pulo solta, agachar a estica, correr a recolhe,
  e o servidor permite o balanço. A arma de corda de `mods/rewards`.

### Só um entra no kart: `core.body_drive` e `core.body_model`

```luau
core.event.on("core:used", function(ev)
    if ev.id ~= KART then return end
    ev.cancel = true
    core.set_block(ev.x, ev.y, ev.z, 0)                       -- o kart agora é ele
    core.body_drive(ev.player, 12, 30, 2.6, 0.6, 1.4, 2.6)    -- acelera, freia, vira, aderência, largura, comprimento
    core.body_model(ev.player, KART_MODEL)                    -- e é visto assim
end)
```

Um carrinho é `core.body_cart(player, true)`: um bloco declarado trilho
(`core.node_conveyor(id, speed, height, rise, true)`) não arrasta quem anda em cima dele, mas quem
está num carrinho corre por ele como num escorregador, no plano, na subida ou na descida, virando onde
um trilho no mesmo nível entra pelo lado; `speed` é a velocidade em que ele leva o carrinho, 0 um
freio (segurar para a frente arranca de novo). `mods/carts` e os trilhos de `mods/building`.

Um barco é `core.body_boat(player, true)`: num líquido o corpo flutua com os pés 0,35 abaixo da
superfície, W/S remam ao longo da proa (para a frente até `sprint`), A/D viram e a quilha tira a
deriva lateral; fora da água ele anda. O casco e a pose são do mod (`core.player_wear`,
`core.player_clip(p, "drive_idle")`): `mods/boats`.

`core.drive` e `core.body_width` dizem como **o mundo** dirige, e precisam ser chamadas enquanto os
mods carregam; `core.body_drive` diz como **um corpo** dirige, quando você quiser, e `accel` em 0 o
põe de volta a pé (passando também a largura de antes). `core.eye_height(h)` diz a que altura o olho
fica acima dos pés (1,62 se o mundo não diz nada): a câmera em primeira pessoa e o ponto de onde se
mede o alcance da mão, no cliente e no servidor. É chamada enquanto os mods carregam, como
`core.movement`, e vai nas boas-vindas (protocolo 50). De 0,2 a 1,8: a caixa do corpo continua com
1,8 de altura. `mods/robot` diz 1,5, o visor do robô.

`core.parkour(true)` diz que neste mundo os corpos a pé fazem parkour (`core/physics/player.cpp`), e
vai nas boas-vindas como o resto (protocolo 87):

* **corrida na parede**: no ar perto do alto do pulo, segurando para a frente a 4 m/s ou mais, com
  uma parede ao lado (à direita ou à esquerda da direção em que você vai), você corre ao longo dela
  no plano, sem gravidade, por no máximo 5 nós;
* **pulo na parede**: pulo apertado no ar com uma parede a menos de 0,45 empurra você para longe da
  parede, a 5,5 m/s e para cima com o pulo do mundo; nunca duas vezes seguidas do mesmo lado, então
  entre duas paredes a 2 de distância você sobe quicando e numa só não;
* **deslize**: agachar apertado enquanto corre (mais rápido que o passo) põe o corpo de costas, com
  0,8 de altura, a 1,15 vez a corrida desacelerando, por 0,9 s: ele passa por baixo de um bloco de 1
  de altura; embaixo de um teto baixo ele fica abaixado e anda no passo agachado;
* **mortal para trás**: pulo segurando para trás, do chão, sobe 2,2 nós indo para trás a 3 m/s: para
  cima de um degrau de 2 de altura atrás de você;
* **corda**: `core.rope(true)`, a partir do código do mundo no cliente, dispara a corda do olho até 40
  nós; o corpo balança em torno do ponto onde ela engancha (as mãos ficam a um comprimento de corda),
  as teclas empurram o balanço, agachar alonga, correr encurta, pulo ou `core.rope(false)` solta. O
  cliente diz ao servidor onde a corda segura e quando solta; o servidor só mantém o gancho num bloco
  sólido ao alcance das mãos, e enquanto o corpo fica pendurado dentro desse alcance a descida do
  balanço não é uma queda. Um renascimento ou `core.player_place` soltam a corda, nas duas pontas.

Num mundo assim as mãos **não** se agarram a beiradas (ali uma parede de 2 de altura é um mortal para
trás e uma alta é uma chaminé), e o servidor permite o que esses movimentos fazem: 20 m/s de balanço,
qualquer altura, um corpo deslizado por baixo de um bloco (`limits_for`). O robô tem as animações
para cada um: `wall_run_left`, `wall_run_right`, `wall_jump`, `power_slide`, `backflip`,
`rope_swing`. `games/parkour` é o mundo feito para isso.

`core.body_model` é `core.player_model` para um corpo só. É assim que se faz um mundo onde você chega
a pé e entra num kart estacionado: `games/track`.

* **Os números de velocidade continuam dele.** `core.body_drive` muda o que o corpo *é*, não a sua
  velocidade: `sprint` é a velocidade máxima e `walk` a da ré, e elas mudam com `core.body_movement`,
  que por sua vez deixa o kart em paz (uma contagem regressiva em zero não faz ninguém descer).
* **Entrar levanta o corpo em 1,07** -- um chassi fica sobre as suas molas --, então uma parada que
  também zera o pulo é corrigida pelo servidor: deixe o pulo do mundo.
* **O que o jogador vê do próprio corpo** com a câmera atrás (`core.view`) é o modelo de
  `core.player_model`: as boas-vindas levam um só. Os outros veem o de `core.body_model`. O código do
  mundo que roda no cliente pode chamar `core.body_model(0, id)` para mudar como **aquela máquina**
  desenha o próprio corpo (0 o põe de volta): `games/track` põe ali o kart enquanto se dirige.
* **Um corpo desenhado como bloco: `core.body_block(player, id)`.** Em vez de um modelo, um cubo
  com as faces daquele bloco, ou as caixas dele para um bloco feito de caixas (uma laje é meio cubo),
  com o fundo nos pés dele -- ou o próprio modelo do bloco, um baú --, sempre no esquadro da grade.
  Ele é mirado e atingido (um golpe, uma flecha) naquele bloco e em nenhum outro lugar, não no ar
  acima dele; 0 põe o modelo de volta. A caixa com que ele anda continua sendo a do motor, e o que ele
  segura e veste não é desenhado. Como para `core.body_model`, o código
  do cliente chama `core.body_block(0, id)` para o próprio desenho.
* **Rodas que giram e viram.** Um modelo com esqueleto as anima sozinho, pelo nome do osso: `steer_*`
  vira em torno do próprio eixo, `wheel_*` rola em torno do seu (filho de um `steer_*`, também vira),
  `steering_wheel` gira um quarto de volta com tudo virado. "O próprio eixo" é o Y do osso, ou seja,
  para onde aponta um osso do Blender. O raio de uma roda é a altura do seu centro acima do fundo do
  modelo. O cliente lê a direção e os metros percorridos a partir de como o corpo se move: nada para
  declarar, e vale também para os outros jogadores. Um bloco que veste o modelo fica parado.
* **Animações pelo nome.** Um modelo com animações toca uma escolhida pelo movimento do corpo, com um
  quinto de segundo de transição entre elas: `idle`, `walk` (até 5,5 m/s), `run`, `jump_loop` (no ar),
  `swim`/`swim_idle` (num líquido), `slide` (numa esteira em descida a partir de 3 m/s: um
  escorregador) e, para o próprio corpo, `fly`/`fly_hover` e, enquanto cava, `mine`, `dig` ou `chop`
  conforme o material do bloco (`stone`, `soil`, `wood`). Uma animação que o modelo não tem deixa a
  anterior.
* **Quem dirige.** Num corpo que veste um modelo com rodas (`wheel_*`), o cliente senta o modelo de
  `core.player_model`, na mesma origem: ele toca `kart_enter` uma vez, depois `drive_idle` inclinado
  para `drive_left` ou `drive_right` tanto quanto a direção estiver virada. As animações do motorista
  precisam então ser feitas em torno da origem do veículo. `mods/robot` e o kart de `games/track` são
  feitos assim.
* **Peças do veículo.** Uma peça que um mod faz o motorista vestir (`core.player_wear`) com o nome do
  veículo mais `_<parte>_<variante>` (`track_core:kart_wheel_slick` em `track_core:kart`) vai nos ossos
  do veículo, não nos de quem está nele, e os ossos do veículo chamados `<parte>` ou `<parte>_*` não
  são desenhados: a peça os substitui (as quatro `wheel_*`). O próprio veículo vestido como peça o
  pinta: a sua tinta colore os `tint_1..3` do veículo. As chamas do nitro saem das juntas `flame_*`
  (ao longo do osso) das peças que as têm, senão das do veículo; as marcas de derrapagem dos ossos
  `wheel_R*`, na estrada embaixo deles (numa peça de pista, na sua superfície).
* **Prévia num painel.** `core.ui_model(panel, x, y, w, h, model, tint, pieces)` põe num quadro do
  painel um modelo girando devagar, enquadrado pelo seu tamanho, com as suas cores e as peças (ids
  separados por espaços) nele como acima: a garagem de `games/track` mostra assim o kart escolhido.
  Não responde a cliques (protocolo 104). `tools/make_kart_parts.py` faz o kart dividido em partes
  (`nose`, `pods`, `pipes`) e as suas 14 peças. `tools/make_kart_meshy.py` acrescenta uma quinta
  variante a cada encaixe, `dragon`, feita com o Meshy (conceitos por image-to-image a partir de um
  render do kart, depois image-to-3D): cada modelo é ajustado à caixa do seu encaixe no mesmo rig, e
  as suas faces pegam cores chapadas pelo matiz da textura do Meshy, que viram `tint_1`/`tint_2`/
  `tint_3` e os materiais fixos do kart, para que a pintura o recolora. A roda é uma roda posta quatro
  vezes, a lateral uma lateral espelhada, e os escapamentos põem as suas próprias juntas `flame_*` nas
  bocas dos chifres.
* **Braços em primeira pessoa.** Um modelo registrado com o nome do modelo do jogador mais `_arms`
  (`robot:robot` -> `robot:robot_arms`) é desenhado na frente do olho quando a vista está em primeira
  pessoa. Ele é feito no espaço do olho: origem no olho, para a frente -Z, para cima +Y. Toca `swing`
  enquanto o botão esquerdo está apertado e `idle` no resto do tempo, e balança enquanto se anda. Um
  bloco na mão é um cubo das suas faces na palma do osso `hand_R`; uma peça de pista
  (`core.node_piece`) é a peça em miniatura, com o lado mais longo da sua estrada medindo 0,24
  antebraço; no chão, a mesma peça em miniatura. Vale também no mundo local.
* **Vidro.** Um material com alfa da cor base abaixo de 1 (`baseColorFactor`, no Blender o Alpha do
  Principled BSDF) é transparente: é desenhado depois do resto do modelo, misturado e sem escrever
  profundidade (as janelas dos veículos do front, 0,3). O alfa da textura, em vez disso, recorta:
  abaixo de 0,5 o pixel não existe.
* **As cores e os acessórios do jogador.** Os materiais `tint_1`, `tint_2`, `tint_3` do modelo do
  jogador (e dos seus `_arms`) pegam a Primária, a Secundária e o Detalhe escolhidos no menu
  Personagem (protocolo 93, `Hello::livery`); sem escolha, ficam com a cor do arquivo. Os acessórios
  são modelos com o nome do jogador mais `_cap`, `_tophat`, `_headphones`, `_scarf`, `_bowtie`,
  `_medal` (`robot:robot_cap`), feitos no esqueleto do jogador e desenhados como uma peça vestida; um
  mundo que não os registra simplesmente não os mostra. `tools/make_robot_extras.py` os faz. A
  espécie (protocolo 94, dois bits acima dos acessórios: humano, homem-toupeira, homem-sapo; protocolo
  119, um terceiro bit acima do estilo: o cavaleiro-dragão, por enquanto não oferecido) substitui o
  modelo do jogador pelo que tem o nome dele mais `_human`, `_mole`, `_frog`, `_knight`
  (`robot:robot_mole`, com os seus `_arms`), no mesmo esqueleto e com as mesmas animações; o estilo
  escolhido é uma peça vestida chamada `<espécie>_<estilo>` (`robot:robot_mole_lamp`). Os acessórios
  são os refeitos para a espécie (`robot:robot_mole_cap`) se o mundo os registra, senão os do jogador;
  um acessório de cabeça substitui os estilos que são chapéus (lâmpada, coroa, vitória-régia). Um
  mundo que não registra a espécie mostra o robô. As espécies de 4 a 7 são de add-ons, mods de fora do
  jogo: todo par `<modelo do jogador>_<nome>` e `<modelo do jogador>_<nome>_arms` que um mundo
  registra, com `<nome>` de a-z e 0-9 e diferente dos do jogo, é oferecido depois dos do jogo, em
  ordem de nome (quatro no máximo). O número que uma pintura lhe dá é o do mundo -- o mesmo add-on é 4
  num servidor que só tem ele e 5 num que tem outro antes --, então o cliente guarda a espécie
  escolhida pelo nome (`addon_species` nas opções, `--addon-species NOME` para uma entrada por
  script) e diz a cada mundo o número dela assim que conhece os modelos daquele mundo; até lá, o hello
  diz o robô, e um mundo sem essa espécie fica com o robô. Os seus estilos são as peças
  `<ela>_style_<estilo>` e `<ela>_hat_<estilo>` que ele registra, pelo nome do estilo, três no máximo
  (uma `_hat_` não é usada embaixo de um acessório de cabeça); ela sempre veste `<ela>_worn` se
  existir; veste os acessórios do robô, a não ser que registre os seus (`<ela>_cap`). Um mundo cujo
  modelo do jogador é um robô do jogo com o nome de outro mod (`track_core:robot`) acha uma espécie
  também como `robot:robot_<nome>`, então um add-on registra os seus modelos uma vez só e não precisa
  de nenhum outro mod; com `rooms = all` no seu `mod.conf`, todas as salas de um servidor o têm
  ([`lizard`](https://github.com/giacomo986/CubesAndPortals-addons/tree/main/lizard) diz isso). O corpo e os braços de uma espécie não precisam levar animações: uma
  que não tem nenhuma toca as do modelo do jogador (e as de `<modelo do jogador>_arms`), com os ossos
  achados pelo nome, então o arquivo é a sua malha e não um megabyte de animações do robô. **Um mundo
  cujos braços levam o que seguram** (os `robot_arms` do front e do assalto,
  `tools/make_front_models.py`, e os do yafps e do laboratório, `tools/make_fps_models.py`: as armas
  são malhas em ossos `tool_<arma>` abaixo de um osso `gun`, o osso da mão direita é `wrist_R` e não
  há `hand_R`) faz toda espécie segurá-lo onde o robô segura: os braços de uma espécie -- do jogo ou
  de um add-on, com animações próprias ou sem -- são movidos por todas as animações dos braços daquele
  mundo, e as armas, os clarões e os cartuchos são desenhados a partir dos braços do mundo, deixando
  de fora os próprios braços (todo osso chamado `fp_root`, `upper_arm_*`, `forearm_*`, `hand_*` ou
  `wrist_*`). O punho do robô é uma bola, então as suas animações giram o pulso de qualquer jeito (100
  graus ou mais em relação ao antebraço, que chega pelo lado): numa mão que continua o antebraço isso
  é uma mão dobrada para trás, e qualquer giro num pulso rasga uma pele que não tem vértices
  compartilhados entre mão e antebraço (a do humano, a do lagarto). Então um mundo registra ao lado
  dos seus braços **`<modelo do jogador>_arms_hands`**, o mesmo arquivo com as chaves dos ossos dos
  braços refeitas para mãos por `tools/make_front_hands.py`: cada mão orientada pela arma -- a
  direita ao longo do cano com o seu ponto de pegada (4,6 cm ao longo da mão, onde o rig do sandbox
  segura uma ferramenta) na empunhadura da arma, a esquerda embaixo da arma com a palma para cima
  enquanto o punho esquerdo do robô está na arma --, o antebraço seguindo reto a partir da mão **sem
  nenhum giro no pulso**, o cotovelo uma dobradiça dobrada em 40 e 25 graus fixos, o ombro onde isso
  o deixar (fora da vista); o osso da mão direita volta a se chamar `hand_R` e a malha dos braços do
  próprio robô é deixada de fora. Um arquivo que um servidor distribui tem no máximo 1 MiB
  (`kMaxMediaBytes`): a ferramenta compacta o buffer e recusa um maior -- um modelo acima disso não é
  servido, e o cliente recua sem dizer nada na tela (o log do servidor diz). Com esse arquivo, as
  animações da espécie e as armas vêm dele; sem ele, dos braços do mundo com `hand_R` no lugar do
  `wrist_R`. O robô sempre fica com os seus. Nada é então desenhado em `hand_R` (`<item>_held` é para
  corpos vistos de fora), então braços assim precisam levar tudo o que o seu mundo põe numa mão. Uma
  espécie não precisa de nada para isso: o mesmo arquivo `_arms` no rig do sandbox. Uma peça vestida é
  desenhada tão clara e tão dissolvida quanto o corpo em que está. Numa peça vestida, um osso
  `wag_<n>` balança de um lado para o outro em torno da vertical, cada elo atrás do anterior conforme
  o seu número, então uma corrente deles é uma cauda que abana: ela fica para trás numa curva, e
  correndo abana mais rápido e mais curto e se levanta. A tela de título acha os mesmos arquivos nas
  pastas de mods, incluindo a do jogador, `~/.cubes-and-portals/mods`. [`lizard`](https://github.com/giacomo986/CubesAndPortals-addons/tree/main/lizard) é um deles.
  `tools/make_species.py` faz todos: cada parte do corpo (cabeça, tronco, braço, perna) é uma única
  superfície, primitivas fundidas com um remesh de voxels, suavizadas e decimadas, com pesos
  misturados nas juntas; olhos e pequenos detalhes continuam nítidos. Os acessórios de uma espécie
  são construídos nas suas próprias medidas (`WEARS` ali, `extras()` em `tools/make_robot_extras.py`),
  não escalados a partir dos do robô. Um osso `sway_*` balança só para a frente, para longe do peito.
  Numa peça vestida, um osso que o jogador não tem segue o seu pai, e se se chama `sway_*` também
  balança em torno de onde está preso, cada corpo fora do compasso do outro conforme as suas cores. A
  cabeça dele é de onde ele pende: dada no espaço do mundo em repouso, ela é posta no próprio espaço do
  rig, que é escalado (`to_rig` nas duas ferramentas) -- deixada no espaço do mundo, ficava um terço
  longe demais e o que pendia dela balançava fora do corpo.
* **Ferramentas na mão.** Um osso `tool_<nome>` só aparece quando o item na mão se chama
  `<mod>:<nome>` (`tool_pick` para `tools:pick`), e os outros `tool_*` somem: nos braços e no próprio
  corpo em terceira pessoa, e nos outros jogadores (desde o protocolo 48 o servidor envia a todos o
  que cada um segura).
* **Itens na mão.** Se o mundo tem um modelo com o nome do item mais `_held`
  (`core.register_model("ore:diamond_held", "diamond_held.gltf")`), quem segura esse item o tem na
  palma de `hand_R` no lugar do cubo: um glTF rígido, sem esqueleto, feito em torno da pegada (origem
  no centro do punho, +Y para onde aponta uma ferramenta, ou seja, para cima em primeira pessoa, +Z
  para o olho em primeira pessoa: uma coisa chata fica no plano XY), em metros do robô de 1,357 e
  escalado com o antebraço como o bloco. Vale nos braços, no próprio corpo e nos outros jogadores.
  `tools/make_held_models.py` faz os 41 dos mods do projeto e os ícones dos que não tinham (a
  armadura e o kart de recompensa como miniaturas dos seus modelos vestidos);
  `tools/make_gun_icons.py` renderiza o ícone de uma arma a partir do seu modelo na mão.
* **Modelos do Meshy.** Um glTF com textura é desenhado com a sua textura (só a cor base, sem luzes
  embutidas). `tools/meshy_prop.py` transforma um `.glb` do Meshy num assim: dimensionado em metros,
  origem na base ou no centro, soldado e decimado (3000 triângulos por padrão), a cor embutida num
  unwrap novo de 128 px (o atlas do próprio Meshy é fragmentado demais para encolher) e reduzida a 24
  cores para ficar bem ao lado dos blocos de 32 px, e uma folha de quatro vistas ao lado do robô
  renderizada em `~/.cache/cnp_look/<nome>_sheet.png`. O caminho que funciona: primeiro uma imagem de
  conceito (text-to-image do Meshy, um objeto só, fundo liso, vista de três quartos, o estilo low-poly
  robusto do jogo), aprovada; depois image-to-3D a partir dela; depois a folha, aprovada; só então os
  arquivos vão para um mod. Serve para coisas rígidas -- objetos, decoração, estátuas, itens na mão.
  Um personagem é o contrário: dobrar um corpo gerado nas proporções do robô foi tentado com o
  homem-toupeira e desfeito; `tools/make_knight_meshy.py` em vez disso mantém o esqueleto do robô como
  está e traz para ele as peças do cavaleiro-dragão: a parte de cima aumentada em torno do quadril até
  os ombros serem os do robô, o elmo um pouco menor, os braços baixados da pose em A para os
  comprimentos de braço do robô e as pernas para as pernas dele, com mangas escuras onde as placas não
  se encontram mais; assim toda animação põe mãos e pés onde põe os do robô (o volante de um kart, uma
  ferramenta). As animações do robô são escritas no arquivo, um canal constante guardado como uma
  chave só para que ele fique abaixo de 1 MB. O cavaleiro é gerado na smart topology do Meshy, uma
  centena de peças separadas, e é montado como o robô: cada peça segue um osso inteiro e pega uma cor
  chapada lida da textura, vermelho tint_1 e dourado tint_2 para o jogador tingir; a textura em si não
  é mantida. Ele não tem estilos. [`lizard/make_lizard.py`](https://github.com/giacomo986/CubesAndPortals-addons/blob/main/lizard/make_lizard.py) faz o homem-lagarto, um add-on, de um
  terceiro jeito, para um modelo que o Meshy desenhou como se fosse de cubos mas cuja malha não tem
  nenhuma face plana (alto detalhe, uma malha só de 336792 triângulos, as escamas só na textura): as
  suas partes são separadas e levadas às juntas do robô -- os braços baixados da pose em A, braços e
  pernas esticados até os do robô -- e então, em repouso, preenchidas com cubos de 4 cm, cada um
  seguindo o osso que a sua altura lhe dá e cada lado visível pegando a cor que a textura tem mais
  perto dele (pele tint_1, barriga tint_2, arreio tint_3). Um ponto está dentro de uma parte quando a
  maioria de cinco raios saindo dele cruza a superfície um número ímpar de vezes: as partes estão
  abertas onde foram cortadas, e não dá para perguntar às normais da própria superfície. A cauda é a
  sua peça vestida, em quatro elos `wag_*`; os estilos (crista, gola, espinhos) são cubos postos sobre
  os do corpo, e os acessórios são medidos pelos seus cubos. [`fox/make_fox.py`](https://github.com/giacomo986/CubesAndPortals-addons/blob/main/fox/make_fox.py) faz o
  homem-raposa, outro add-on, de um quarto jeito, para um modelo que o Meshy fez com poucos triângulos
  (image-to-3D a partir de uma imagem, smart topology: 12486 triângulos em dezesseis peças que não se
  juntam): as suas próprias faces são mantidas, cada peça levada às juntas do robô como as do lagarto
  e reduzida a 7000 triângulos, cada face com um material chapado lido da textura e preso ao que a
  sua peça pode ter de pintura (pelo tint_1, focinho e peito tint_2, colete tint_3). Peças assim não
  são fechadas: cada ponta aberta ganha uma tampa, e a cabeça, uma casca rasgada em lascas, é refeita
  como o sólido fechado em volta dos seus pontos. A cauda é a sua peça vestida em quatro elos `wag_*`,
  os estilos (tapa-olho, aljava) são partes nos ossos do robô, e os acessórios são medidos pelo seu
  corpo.
* **Armadura vestida.** Um item com `wear` (`core.register_item`) vai num dos quatro espaços de
  vestir da bolsa (cabeça, peito, pernas, pés; shift-clique com a bolsa aberta veste e tira). Se o
  mundo tem um modelo com o nome do item (`core.register_model("armor:iron_helmet", ...)`), o corpo o
  veste: um glTF com skin em ossos com os mesmos nomes dos do jogador, desenhado com a sua pose. Desde
  o protocolo 58 o servidor envia a todos o que cada um veste. `tools/make_armor_models.py` faz as
  dezesseis de `mods/armor` no rig do robô.
* **Agachar.** Ctrl segurado a pé (voando continua sendo "descer"): o passo a 0,35 de `walk`, sem
  corrida, o olho a 0,8 da sua altura. O corpo toca `crouch_idle` ou `crouch_walk`, e desde o
  protocolo 51 os outros também veem.
* **Quem trabalha.** Com o botão esquerdo apertado o corpo toca a animação da sua ferramenta: `mine`
  com a picareta, `dig` com a pá, `chop` com o machado, `sword_slash` com a espada; de mãos vazias, a
  do material do bloco. Desde o protocolo 49 os outros também veem.

### Uma entidade que todos veem: `core.ecs.visible`

```luau
local sign = core.ecs.spawn("mymod:sign", 32.5, 22.5, 30.5)
core.ecs.visible(sign, true)    -- daqui em diante todo jogador perto a recebe
core.ecs.visible(sign, false)   -- e daqui em diante não mais
```

A entidade de um mod está no mundo do servidor e só, até o mod pedir que os outros a vejam. A partir
daí ela é enviada a todo jogador perto pelo mesmo caminho que envia pessoas, criaturas e itens no
chão, **como coisa**: o cliente a desenha como uma caixa própria, não como pessoa nem como criatura,
porque o motor não sabe o que ela é.

* **Só as entidades de um mod.** Um jogador, uma criatura ou um item no chão já têm um corpo que o
  motor lhes deu, e `visible` os recusa nos dois sentidos: um mod não pode esconder um jogador.
* **Escondê-la não é instantâneo para quem estava olhando**: um cliente esquece um corpo que não
  recebe mais depois de cinco segundos de silêncio.
* **Um modelo**: `core.ecs.model(id, core.register_model(...))` a desenha com o modelo em vez da
  caixa (0 volta para a caixa), com os pés no seu y. Só numa entidade tornada visível por `visible`.
* **Vida**: `core.ecs.health(id, 3)` lhe dá 3, e daí em diante o golpe de um jogador tira dela tanto
  quanto tira de uma criatura; `core.ecs.health(id)` a lê. Em 0 ela fica onde está: o que morrer quer
  dizer, o mod decide.
* **Uma animação**: `core.ecs.clip(id, "shoot")` toca essa animação do seu modelo no lugar da que o
  motor escolhe pelo movimento (idle, walk, run conforme a velocidade); `core.ecs.clip(id, "")` a
  devolve ao movimento. `"death"` toca uma vez e fica na última pose: chame, espere a sua duração,
  depois `core.ecs.kill`. Viaja para os clientes (protocolo 88).
* **Tamanho para um golpe**: `core.ecs.box(id, 1.1, 3.2)` -- meia largura em volta do centro, altura
  dos pés para cima. Sem isso, um golpe só a acha num cubinho de 0,6 nos seus pés.
* **Para onde olha**: `core.ecs.set(id, "yaw", a)` a gira como o olhar de um jogador (0 olha para
  -z); `core.ecs.get(id, "yaw")` o lê. "yaw" não pode mais ser o nome de um campo de um componente.
* **Não colide** com nada. TODO.md OPEN-20.

### Veículos: `core.craft`

A entidade de um mod tornada visível pode virar um **veículo**: um jipe, um tanque, um helicóptero,
um avião. O motor a move com Jolt (o mesmo do kart, `core/physics/craft.h`), só no servidor, num passo
fixo a 60 Hz; os jogadores sobem como passageiros de um corpo (`core.ride`) e quem dirige é quem o mod
escuta.

```luau
local id = core.ecs.spawn("front:vehicle", x, y, z)  -- os pés: o fundo do modelo
core.ecs.visible(id, true)
core.ecs.model(id, TANK)
core.ecs.set(id, "yaw", 0)                            -- antes de make: para onde olha
core.craft.make(id, "tracked", 3000, 1.6, 0.6, 3.2, 4, 9, 0.9)
--               tipo, kg, meia largura, meia altura, meio comprimento, m/s², m/s, rad/s
```

* **`core.craft.make(id, kind, mass, half_w, half_h, half_l, accel, top, turn)`**: `kind` é
  `"wheeled"` (tração nas quatro rodas, as duas da frente viram), `"tracked"` (duas esteiras, vira
  freando uma, parado gira no lugar), `"rotor"` (empurrado ao longo do seu alto, inclinado para onde
  precisa ir) ou `"wing"` (empurrado ao longo do nariz, sustentado pela velocidade: abaixo de 45% de
  `top` ele estola). As medidas são o casco em nós. Feito de novo, recomeça parado.
* **`core.craft.keys(id, forward, right, up, yaw)`**: o que o motorista pede, de -1 a 1, mantido até
  que se diga outra coisa. Rodas e esteiras: acelerador e direção. Rotor: `forward` e `right` o
  inclinam, `up` é o coletivo (0 paira), `yaw` o gira. Asa: `up` mexe no acelerador (que fica onde
  foi deixado), `forward` é o manche para a frente (nariz para baixo), `right` a rolagem, `yaw` o
  leme. O jogador aperta teclas no seu cliente: o mod as envia com `core.net` e as passa aqui.
* **`core.craft.pose(id)`**: uma tabela `x y z yaw pitch roll vx vy vz speed grounded impact
  throttle`. `speed` é ao longo do nariz, `impact` o golpe mais forte sofrido desde a última vez que
  se perguntou, em m/s (uma batida), e perguntar o zera.
* **`core.craft.push(id, vx, vy, vz)`**: somado à sua velocidade: uma explosão.
* **`core.craft.mount(id, on, x, y, z)`**: outra entidade visível carregada em `on` em x, y, z no
  referencial dela (x à direita, y para cima, z para a frente): uma torre num casco, um cano na torre.
  `on` 0 a solta. Recolocada no lugar a cada tick depois dos veículos, quem carrega antes de quem é
  carregado.
* **`core.craft.aim(id, yaw, pitch)`**: quanto uma coisa montada está girada sobre quem a carrega e
  levantada; numa não montada, para onde ela olha no mundo (um morteiro).
* **`core.craft.seat(player, id, x, y, z)`**: o jogador carregado naquela entidade, no assento x, y,
  z do referencial dela, até `core.ride(player, 0)` fazê-lo descer ou a entidade sumir. Sentado, ele
  olha para onde quiser (`core.player_look`) e o seu WASD não o move.

Cada veículo é um mundo Jolt próprio com os blocos em volta; o motor então separa dois veículos que
se sobrepõem (em proporção à massa) e empurra para fora do casco quem está a pé. Desde o protocolo
100 o casco vai para os clientes: o corpo de um jogador não entra nele, fica em cima e é levado junto.
A rolagem de uma entidade vai para os clientes desde o protocolo 98.

**No cliente de quem dirige**, `make`, `keys` e `pose` também rodam o veículo ali, para que ele se
mova no frame em que a tecla é apertada e não uma ida e volta depois. `core.craft.make(id, ...)` só
vale para o veículo em que o jogador está sentado (com `core.craft.seat`) e devolve `false` até o
assento ter chegado: chame de novo até ele dizer `true`. A partir daí, `keys` são as que o mod dá no
servidor (as mesmas, calculadas do mesmo jeito) e `pose` é onde o veículo vai estar. O motor o
desenha ali, põe o jogador nele e leva junto o que está em cima ou dentro dele. Ele o compara com as
poses do servidor: corrige devagar as diferenças pequenas e o põe de volta onde o servidor diz se ele
se afastar mais de 3 nós. Descer do veículo para a previsão. Os outros `core.craft` no cliente não
fazem nada. Exemplo: `drive_here` em `games/front/mods/front_game`.

### Código que um mundo envia ao cliente: `side`

```
# mod.conf
name = hud
side = both        # server | client | both; server se ausente
```

```luau
if core.side() == "server" then
    core.event.on("core:net_message", function(ev) core.net.send(ev.player, "olá") end)
else
    core.render.pass("hud:crosshair", 100, function() core.render.box(0, 60, 0) end)
end
```

Um mod diz **onde roda**. `server` é todo mod de antes: roda onde o mundo está. `client` o servidor
não roda: empacota e envia aos jogadores como mídia. `both` roda nos dois lugares, e é o que um mod
precisa para conversar com a sua outra metade no seu próprio canal -- o canal de um mod é o seu nome,
e o servidor o dá aos mods que roda. `core.side()` diz a uma metade qual ela é: `"server"`,
`"client"`, ou `"both"` num processo que é o seu próprio servidor.

Na máquina de um jogador, o código de um mundo:

* **não escreve o mundo e não registra nada**: blocos, formas, modelos, itens, receitas, etapas,
  modificadores, bolsas, baús, painéis, vida e movimento são do servidor. A função existe e não faz
  nada, ou dá um erro dizendo por quê;
* **não tem nenhuma capacidade**, nem `os`: um mod que declara uma não é carregado, e o log a nomeia;
* **não aumenta a sua própria** cota de memória ou orçamento de interrupção acima dos padrões;
* **morre com a sessão**: atravessar um portal é entrar em outro mundo, com o seu próprio código.

**O jogador pode recusá-lo**: `--world-code refuse` para todos os mundos, ou a chave de assinatura de
um mundo, em hexadecimal, uma por linha em `world_code_refused.txt` no cache de mídia. Um mundo
recusado é jogado do mesmo jeito: o servidor continua sendo a autoridade, então recusar não dá
vantagem nenhuma e só tira o que o mundo queria mostrar.

O código de um mundo **desenha na tela e ouve as teclas**: `core.hud` e `core:input`, abaixo. O que
**ainda não** existe: um mod só de cliente que fale com o servidor sem uma metade `both`. TODO.md
OPEN-49.

### Na tela e pelas teclas: `core.hud` e `core:input`

```luau
core.hud.pass("race:time", 100, function()
    core.hud.rect(0.5, 0.5, 0.026, 0.003, 223, 244, 255, 230)   -- mira
    core.hud.text(0.5, 0.9, `{laps} / 3`, 0.045)
end)

core.event.on("core:input", function(ev)
    if ev.action == "mouse_left" then
        ev.cancel = true            -- o clique é do mod: nada de cavar, nada de golpe
        core.net.send(0, "fire")
    end
end)
```

Um **hud pass** não é um render pass: esse roda uma vez por câmera, e um portal na tela desenharia o
tempo uma vez para cada mundo que mostra. Este roda **uma vez por frame**, por cima da barra e por
baixo de um painel. `x, y` são frações da tela e são **o centro** do que é desenhado (`align` 0 põe
ali a ponta esquerda do texto, 2 a direita); `w`, `h` e `size` são frações da **altura**, então um
quadrado continua quadrado em qualquer proporção. Fora de um pass, `text`, `rect` e `image` são
recusados e dizem isso.

`core.hud.image(name, x, y, w, h, a)` desenha uma imagem da mídia deste mundo -- um PNG entre as
texturas de um dos seus mods, pelo nome do arquivo -- centralizada em `x, y` e com as medidas como
`rect`; `a` é a opacidade, 0-255. Decodificada uma vez só; um nome que o mundo não tem é dito no log
uma vez e não desenha nada. O servidor envia como mídia só as texturas dos **blocos e itens
registrados**: um PNG que é só um desenho para o HUD precisa ser registrado como um item que ninguém
recebe (`core.register_item`), e é chamado pelo nome do seu arquivo. `games/track` faz assim com os
ícones dos power-ups e a mancha de tinta (`track_core:item_*`, `track_core_item_boost.png`). Do
tamanho das outras texturas do mundo (32 px): o atlas só aceita um, e uma imagem de outro tamanho é
um quadrado vermelho na bolsa.

`core.hud.item(name, x, y, side)` desenha o quadrado de um item como a bolsa o desenha (uma peça de
pista como a sua planta), com o centro em `x, y` e o lado em frações da altura; false para um item
que o mundo não tem. `core.hud.mouse()` diz onde está o cursor, em frações da tela como o `x, y` de
`text` (nada onde não há tela): com a câmera de `core.fixed_view` o cursor fica livre e os cliques
chegam a `core:input` mesmo assim, então um mod faz uma grade para clicar (o editor de pista).

`core.hud.aspect()` é a largura da tela dividida pela altura (0 onde não há tela). Serve para pôr
algo **alinhado com as barras do motor**, cujos tamanhos são frações da altura: a barra de fome de
`mods/hunger` é tão larga quanto a hotbar, e o centro de cada marca vai em `0.5 + dx / aspect`. Os
tamanhos do motor estão em `src/client/ui/hud.cpp` (um slot é 0,075 da altura, o espaço entre dois
0,0075, uma barra 0,0165 de altura).

`core.hud.scale()` é o tamanho da interface que o jogador escolheu em Opções > Vídeo, como o fator
pelo qual crescem os slots e as barras do motor: 1 normal, 0,8 pequena, 1,25 grande, algo no meio
em auto (1 onde não há tela). Os tamanhos acima são a 1: um mod alinhado com as barras os
multiplica por ele, como faz `mods/hunger`. A margem sob a hotbar (0,02) não cresce.

`core.view(distance)` põe a câmera deste mundo **atrás** do jogador, a essa distância em nós (0 é o
olho, no máximo 16), com o seu corpo desenhado no modelo de `core.player_model`. Vale para o mundo que
pede: voltar a um mundo que não disse nada volta para o olho. A tecla V faz o mesmo à mão.
`games/track` a chama com 5.

`core.engine_hud_on()` diz se a hotbar e as barras do motor são desenhadas neste mundo: um mod que
desenha uma barra ao lado delas (`mods/hunger`, `mods/enchant`) a esconde junto com elas quando o
código de um mundo disse `core.engine_hud(false)`.

`core.fixed_view(x, y, z, distance, yaw, pitch, fov, seconds)` é a câmera de `core.iso_view` presa a
**um ponto** em vez da cabeça: ela voa do olho até lá em `seconds`, o cursor fica livre
(`core.cursor_ray`), o corpo fica parado (as teclas não andam) e não é visto; `distance` 0 a traz de
volta para o olho. É a vista de `mods/chess` sobre o tabuleiro. Pedida de novo com a câmera já lá,
ela vai na hora para os números novos: pedida a cada frame, é uma câmera que o próprio mod move, e é
assim que `games/shmup` segue um caça pelo pátio e gira por cima dele enquanto ele sobe. Para todas
essas câmeras `distance` é no máximo 64 (40 até 2026-11-14).

`core.side_view(distance, yaw, fov, seconds)` é a mesma câmera sem inclinação, **de lado**, para um
jogo de plataforma: A e D andam ao longo da imagem, W pula, S agacha, e o corpo é mantido no plano em
que estava quando a câmera foi pedida -- nada anda para dentro da imagem. A imagem acompanha como a
tela de um jogo de plataforma: o corpo se move um pouco para cada lado antes de ela rolar, depois ela
rola suave, e um pulo a move menos que uma queda. `yaw` é para onde a câmera olha (0 põe +x à
direita). `games/platformer` constrói o seu nível ao longo desse plano.
`core.top_view(distance, yaw, fov, seconds)` é a câmera **de cima**, para um shooter de fliperama: o
corpo continua virado para o alto da tela (`yaw`), W A S D o deslizam sem girá-lo, e onde o mundo voa
ele voa nivelado -- as teclas de pulo e de agachar ficam para o mod (`games/shmup` atira com a barra
de espaço). `distance` 0 traz qualquer uma das duas de volta para o olho.
`core.top_view_at(x, y, z, distance, yaw, fov, seconds)` prende essa câmera em cima de **um ponto**
enquanto o corpo se move embaixo dela: uma tela que não rola, como a de um fliperama (`games/shmup`,
onde o que rola é o cenário que o mod desenha embaixo).

`core:input` chega quando o jogador **aperta** e quando **solta**, enquanto joga: `mouse_left`,
`mouse_right`, e as teclas `key_w`, `key_a`, `key_s`, `key_d`, `key_space`, `key_shift`, `key_ctrl`,
`key_f`, `key_r`, `key_e`, `key_q`, `key_v`, `key_c`, `key_b`, `key_f4`, `key_f8`, `key_i`, `key_k`,
`key_h`, `key_1` … `key_9`; a soltura é o mesmo nome com `_up` (`key_w_up`, `mouse_left_up`). Cada uma
delas é a tecla do jogo pelo que ela faz, não pela letra impressa: um jogador que mudou Frente para a
seta para cima em Opções > Controles ainda envia `key_w`. Um mod com uma tecla própria a registra:
`core.register_key("mymod:dash", "G", "Dash")` põe "Dash" na página Controles, começando em G e
mudável ali como as do jogo, e os seus apertos chegam como `key_mymod:dash` e `key_mymod:dash_up`
(`mods/torches` mostra os seus pontos escuros com uma tecla própria, `games/platformer` lança com
uma). A tecla também pode ser um botão do mouse: `"MOUSE3"` o do meio, `"MOUSE4"` e `"MOUSE5"` os
laterais. Chame do lado do jogador de um mod `both`; num servidor ela não faz nada. Com um baú aberto
sobre a bolsa chegam também `wheel_up` e `wheel_down`, a rodinha (`mods/creative` rola as linhas). Uma
tecla segurada é uma apertada e ainda não solta. Um handler que põe `ev.cancel = true` num **aperto**
do mouse o **tomou para si**: o motor não cava, não golpeia, não coloca e não usa com aquele aperto,
até o botão voltar. As teclas só são ouvidas: o motor anda, voa e abre a bolsa com as suas mesmo assim
(`mods/creative` abre a sua lista com C). A exceção é `key_1` … `key_9`: tomadas para si, elas não
trocam o item na mão (o front as usa para os assentos dos veículos). Só no cliente: o servidor não vê
teclas, vê o que a metade cliente de um mod decidiu enviar -- então um mod `both` escuta `core:input`
só do lado que não é `"server"`.

### Um tiro decidido pelo servidor: `core.player_look` e `core.raycast`

```luau
local x, y, z = core.player_pos(player)
local dx, dy, dz = core.player_look(player)
local _, _, _, wall = core.raycast(x, y + 1.62, z, dx, dy, dz, 60)
```

`player_look` é para onde um jogador olha **segundo o servidor**: o olhar que chegou com a última
posição aceita. Com `player_pos`, é um tiro que o servidor dispara sem pegar a mira do cliente -- um
cliente só pode mentir sobre *quando* apertou. `raycast` devolve o primeiro bloco sólido ao longo do
raio e a que distância, dentro de `max` (no máximo 256), ou nada. **Só blocos**.

Um projétil que acerta **corpos** é `core.shoot`: o primeiro corpo que pode ser ferido (uma criatura,
ou a entidade de um mod com `core.ecs.health`) antes do primeiro bloco sólido perde `damage` e é
empurrado para trás; devolve a que distância e se morreu. `core.blast` é uma explosão: todo corpo
dentro de `r`, metade do dano na borda, e devolve quantos matou. Nenhum dos dois toca nunca um jogador.

```luau
local dist, dead = core.shoot(x, y + 1.5, z, dx, dy, dz, 80, 6)   -- nada se errou
local killed = core.blast(x, y, z, 7, 140)
core.mob_sight("mymod:imp", 64)   -- de que distância um tipo vê quem perseguir (16)
```

No cliente, `core.arms_clip("fire_shotgun")` toca uma vez, do começo, uma animação dos braços em
primeira pessoa (o modelo `<jogador>_arms`), mesmo que já esteja tocando, e depois os deixa para o
motor: o tiro de uma arma, uma arma sendo levantada. `core.held_item` no cliente responde pelo jogador
desta máquina (id 1).

Os braços em repouso tocam `idle_<item>` quando o têm (`idle_shotgun` com `yafps_game:shotgun` na
mão), senão `idle`: cada arma é segurada do seu jeito.

`games/yafps` é o exemplo inteiro: uma base de várias salas no estilo dos arena shooters modernos com
pistola, escopeta de bombeamento, espingarda de cano duplo, metralhadora e canhão de plasma (cada arma
um osso `tool_<arma>` abaixo da mão direita dos braços do robô, as mãos postas nas empunhaduras com IK
em `tools/make_fps_models.py`), ondas de demônios feitos com `core.register_mob`, munição e HUD.

### O que existe: `core.item_count`, `core.item_name`, `core.item_stack`

```luau
for id = 1, core.item_count() - 1 do
    local name = core.item_name(id)
    if name ~= "" then
        print(name, core.item_stack(id))   -- quantos cabem num slot
    end
end
```

`core.item_id` responde sobre um nome que o mod **já tem**; estas três respondem no outro sentido, "o
que existe neste mundo". Servem para um inventário criativo, uma caixa de busca, um mod que lista o
que um mundo contém -- coisas que a partir do Lua nem dava para perguntar.

`item_name` responde **vazio** para um id que ninguém registrou e para um **órfão**, um item cujo mod
foi embora: o id fica (uma pilha num baú continua valendo o que valia), mas ele não pode mais ser dado
a ninguém, então andar de 1 até `item_count` pulando os vazios dá exatamente os itens que um jogador
pode receber. `mods/creative` é o exemplo: um painel com um botão por item, e um clique dá uma pilha
cheia -- `item_stack` e não 64, senão toda ferramenta estaria errada.

`core.item_kind(id)` diz que tipo de coisa é um item, pelo que ele faz: `"block"`, `"wear"`,
`"tool"` (cava), `"weapon"` (golpeia e não cava) ou `"item"`. O catálogo ordena as suas abas por
ele: `core.container_tabs(x, y, z, "All|Blocks|Tools", active)` põe uma coluna de abas sob a
caixa de busca, e um clique volta como `core:ui_click` com painel `"core:chest"`, id `"tab"` e o
número da aba como texto. Os rótulos são desenhados traduzidos: escreva-os como chaves de
`assets/lang/*.txt`.

### Contêineres, cinábrio e outras coisas acrescentadas em 2026-09-25

Um resumo para quem escreve um mod; assinaturas e padrões em [API.md](API.md).

**Contêineres entre si.** `core.container_fill(x, y, z)` diz quão cheio está um contêiner, 0..1, como
um comparador o lê (cada slot vale a sua pilha dividida por quantos cabem numa pilha), -1 se não há
nenhum ali. `core.container_revision` é um número que muda a cada mudança do conteúdo, à mão ou por um
mod: um comparador o observa a cada 0,25 s. `core.container_move(from, to, count)` move a primeira
pilha de um contêiner para outro **como ela está** (desgaste e encantamentos incluídos), e
`core.collect_drops(cell, container)` põe num contêiner o que está caído numa célula: são as duas
metades de um funil (`mods/cinnabar`).

**Observar uma célula.** `core.watch_cell(x, y, z, on)` pede para saber quando **qualquer coisa**
escreve naquela célula: um mod (água correndo, trigo crescendo), um jogador colocando ou quebrando, o
motor (uma porta aberta por uma criatura). Isso é ouvido como `core:watched`. As observações não são
salvas: um observador as pede de novo com um LBM quando o seu chunk carrega.

```luau
core.register_lbm("mymod:rewatch", { "mymod:sensor" }, function(cell)
    core.watch_cell(cell.x - 1, cell.y, cell.z, true)
end)
core.event.on("core:watched", function(ev) --[[ a célula mudou ]] end)
```

**Mover corpos.** `core.push_mobs(x, y, z, dx, dy, dz)` move as criaturas com os pés naquela célula;
um jogador é movido com `core.player_place` (o pistão o desliza em quatro ticks de 0,25).

**Blocos.** `core.node_solid(id)` responde true ou false (1 ou 0 com `api = 1`, onde precisa ser
comparado com 1: em Luau 0 também é verdadeiro). Um bloco `wallmounted` olha para fora da face contra
a qual foi colocado, em seis direções: é assim que pistões, observadores e funis apontam também para
cima e para baixo (+X, -X, +Y, -Y, +Z, -Z para as faces 0..5, e as caixas são escritas com a frente
para +Z). **A imagem de uma caixa é lida com a primeira linha embaixo**: uma tocha desenhada com a
cabeça nas primeiras linhas sai de cabeça para baixo.

**Renomear.** `core.node_alias(old, new)` carrega como `new` as células salvas com o nome velho, em
vez do marcador magenta; `core.item_alias` faz o mesmo para pilhas em bolsas e baús. Eles só são
lidos quando um mundo abre.

**Hora, renascimento, regeneração.** `core.time_of_day()` (0 meia-noite, 0,25 amanhecer, 0,5
meio-dia, 0,75 anoitecer) e `core.set_time(t)`, que todo cliente ouve na hora: a cama.
`core.player_spawn(player, x, y, z, on)` onde aquele jogador renasce, enquanto estiver conectado.
`core.player_regen(player, on)` para ou recomeça a vida que volta sozinha: a fome.

**Criaturas.** `core.mob_drop(name, item, max)` é um segundo drop, 0..max: o couro da vaca.
`core.monsters_near(x, y, z, r)` conta as criaturas de combate (não `"pet"`) dentro de r: a cama não
deixa dormir se houver uma a menos de 8.

**Luz.** `core.light(x, y, z)` é quão iluminada uma célula está agora, 0..15: a luz dos blocos ou a do
céu para a hora (cheia ao meio-dia, um quinto à meia-noite, nenhuma embaixo de `core.world_sky`). O
trigo cresce de 9 para cima.

**Andar em cima de outro: `core.ride`.** `core.ride(player, host, x, y, z)` põe `player` no corpo de
`host`, a x à direita dele, y para cima e z à frente (cada um até 8), e o mantém ali girando com ele:
um jogador nos ombros de outro. O servidor o recoloca no lugar a cada tick e ignora as posições dele; ele
olha, cava, constrói e usa como sempre. `core.ride(player, 0)` o faz descer onde está; ele desce
sozinho se `host` for embora. Só no servidor (protocolo 63).

**Girar, e saber o que há ali.** `core.node_turns(id)` diz quantos valores `core.set_facing` aceita
para aquele bloco: 1 sem frente, 4 em pé, 6 na parede, 24 um tronco; `mods/screwdriver` os percorre
um por clique. `core.loaded(x, y, z)` é 0 para uma célula cuja coluna não está na memória, que
`core.get_block` lê como ar.

**Encantamentos.** Uma pilha guarda até quatro encantamentos num byte, dois bits de nível por tipo: 1
Efficiency (cava 1 + 0,5 por nível mais rápido), 2 Sharpness (um ponto a mais por nível), 3 Unbreaking
(um uso é gasto a cada nível + 1), 4 Protection (um ponto de armadura por nível, vestida).
`core.enchant_held(player, level, kind)` põe um tipo na coisa que está na mão (só se for uma coisa só)
deixando os outros; `core.held_enchant(player)` responde com o byte: o nível do tipo k é
`(v // 4^(k-1)) % 4`.

**Eventos entre mods.** Um mod declara um evento próprio e outro escuta, sem que nenhum dos dois chame
o outro: `mods/furnace` guarda a experiência da fundição nos meta da fornalha e a dá com `furnace:xp`
a quem a abre; `mods/enchant` (que depende de `furnace`) a conta.

```luau
core.event.define("furnace:xp", { fields = { player = "number", points = "number" }, cancellable = false })
core.event.emit("furnace:xp", { player = ev.player, points = 3 })
```

`mods/building` escuta `building:rail_laid {x, y, z}`: um trilho que um mod colocou com
`core.set_block` (que não dispara nenhum `core:placed`) pega a forma que pegaria colocado à mão --
subida, curva -- com os trilhos em volta, assim que a sua célula puder ser lida (também em
`core:world_ready`).

**Entre servidores amigos.** `core.fed_send(server, text)` envia uma linha, até 4000 bytes, ao mod de
mesmo nome num servidor amigo (`--federation`, `/friend`, que compartilha `mods`), que a ouve como
`core:fed_message` na sua primeira sala; ela é selada e assinada como toda linha entre amigos, e não é
guardada -- um amigo desligado nunca a ouve. `core:arrived` diz quem um portal trouxe e de qual amigo.
`mods/passport` usa os dois: carimbos, e pacotes e cartas por uma caixa de correio.

Alguns que outros mods usam:

* **`cinnabar:source { name }`** (mods/cinnabar): um bloco de outro mod dá energia como uma alavanca
  ligada, depois que esse mod disse o nome dele; o pó ao lado desenha uma linha até ele.
  **`cinnabar:changed { x, y, z }`**: um mod mudou ali um bloco que pode estar num circuito -- o
  circuito em volta é calculado de novo. `mods/cameras` declara `cameras:camera_on` e avisa a cada
  mudança.
* **`cinnabar:family { name, find, way }`** (mods/cinnabar): muitos blocos de um mod de uma vez.
  `find` é um `prefixo*` procurado uma vez só para todos eles. `way` é `all` (dá energia por todos os
  lados, sem apertar o bloco embaixo), `out` (dá como um repetidor, pelo lado oposto à frente) ou
  `join` (o pó só desenha uma linha até ele; `find` vazio). **`cinnabar:put { x, y, z }`**: o
  cinábrio mudou o bloco ali; pode chegar no meio do seu cálculo, então anote e aja no tick seguinte.
  `mods/circuits` usa os dois.
* **`weather:darken { amount }`** (mods/weather): o dia do servidor mais escuro nessa medida, 0..1,
  seja qual for o tempo (vale o mais escuro dos dois): o eclipse de `mods/sky`, que solta as criaturas
  da noite.
* **`farm:season { name, left }`** (mods/farm, avisado por mods/weather): a estação, e os dias que
  faltam nela.
* **`keys:ended { player }`** (mods/keys): um jogador que venceu l'Ospite entrou no sandbox
  (`mods/rewards`).
* **`museum:photo { player, tag, id }`** (mods/villagers): uma foto oferecida de um álbum (mods/album)
  de um lugar raro (`peak`, `hollows`, `relic`, `clouds`); o museu da vila onde o jogador está fica com
  a primeira de cada um.

### Páginas: `core.page_register` e `core.page_show`

Uma página é uma tela no estilo de uma página web, diagramada pelo motor no servidor e desenhada com
os mesmos elementos de um painel: nenhuma mensagem nova na rede. Um mod a registra com um nome; quando
um jogador a abre (`/page nome`, ou um link `page:nome`), o mod recebe `core:page_open` e responde com
`core.page_show`. A marcação, uma construção por linha:

```
# Título                  uma barra
## Subtítulo              com uma linha embaixo
texto com [um link](page:outra) dentro, que quebra a linha sozinho
| nome | papel | [kick](cmd:/kick bob) |       linhas de tabela, colunas tão largas quanto precisar
?[escreva aqui](act:enviar)                    um campo: Enter executa o destino
> texto mais discreto
---
```

Os destinos: `page:NOME` abre outra página, `cmd:/linha` executa um comando como o jogador que clicou
(com as permissões dele, como se ele tivesse digitado; depois, a página reabre atualizada), `act:ID`
chega ao mod como `core:page_action`. Num campo, `{text}` vira o que foi escrito. Além de uma tela
aparecem "back" e "next". O texto é medido com as larguras dos glifos da fonte do cliente, então ele
quebra onde é desenhado. No cliente de um só processo (`--quick`) as páginas dos mods existem do mesmo
jeito, com `/page`. O exemplo completo é `mods/board`.

Um jogador com `/mute` também não fala com os outros por meio dos mods: enquanto roda um comando dele,
uma ação de página ou um clique num painel, o `core.chat` do mod para qualquer outra pessoa é retido,
e ele lê uma vez que está silenciado. `core.is_muted` serve só para responder melhor (como `/msg`
faz).

### Como uma função é acrescentada à API

Não à mão. As assinaturas moram **uma vez só** em `src/script/api.luau`, e `tools/api_gen` (um gerador
escrito em Luau, nenhuma toolchain nova) produz três arquivos na hora da build:

| arquivo | para que serve |
|---|---|
| `src/script/api_generated.inc` | a cola C++: `luaL_check*`, padrões, valores de retorno, registro |
| `docs/api.d.luau` | os tipos para o language server |
| `docs/API.md` | esta documentação |

Acrescentar `core.node_name` custou seis linhas na tabela descritiva mais a função do host: a cola,
os tipos e a documentação se atualizaram sozinhos. O custo real de bindings sem FFI (DESIGN.md §5.4)
se paga com ferramentas, não à mão.

`core.event.*`, `core.register_mapgen`, `core.register_abm`/`register_lbm`, `core.ecs`,
`core.craft`, `core.net`, `core.render` e `core.hud` continuam escritas à mão: recebem tabelas ou
funções em vez de escalares, e um gerador que as cobrisse seria maior do que o que ele substitui. Os
tipos delas também são escritos à mão, na parte `.d.luau` de `tools/api_gen/gen.luau`: **uma função
acrescentada a uma delas é acrescentada ali também**, ou o verificador de tipos não sabe que ela
existe.

### Verificar os tipos: `tools/typecheck.sh`

```sh
tools/typecheck.sh                         # todo .luau do repositório
tools/typecheck.sh mods/pipes/init.luau    # só estes
ALL=1 tools/typecheck.sh                   # com as sobras esperadas (abaixo)
```

Duas passadas, código de saída 1 se qualquer uma achar algo:

1. **Os tipos**, contra `docs/api.d.luau` -- o que o language server de um editor mostra, para o
   repositório inteiro de uma vez. O `luau-analyze` do Luau não aceita receber um arquivo de
   definições, então o script compila a sua própria cópia a partir de `third_party/luau` em
   `build-typecheck/` com uma opção `--definitions` a mais (alguns minutos na primeira vez, depois só
   o que mudou).
2. **A compilação em `-O2`**, o nível do motor. O verificador não conta registradores: um chunk
   principal com mais de 200 locais passa limpo na verificação de tipos e depois não carrega.

O que ele deixa de fora a não ser com `ALL=1`, porque não é culpa do mod:

- **`tests/mods/parts`**: um require cíclico, uma global não declarada e um `require("./x")` antigo a
  partir de um `init.luau`, ali de propósito -- é o teste de que eles são recusados, ou ainda
  carregam.
- **`tools/api_gen/gen.luau`**: as suas globais são postas ali por `cnp_api_gen`.

**O que os tipos prometem**, onde é menos óbvio que uma assinatura:

- **Uma chamada que responde com vários valores** responde com **todos ou nenhum**: `core.player_pos`
  é `(number?, number, number)`. O primeiro é opcional e os outros não, porque um verificador de
  tipos estreita o valor sobre o qual se perguntou e nunca os irmãos dele: `if not x then return end`
  é a verificação, e depois dela `y` e `z` são números.
- **O payload de um evento** é `{ [string]: any }`: os seus campos são o que `core.event.define`
  declarou, que nenhum tipo pode saber. Diga o que eles são onde importa: `ev.player :: number`.
- `core.register_abm`/`register_lbm` passam à ação uma `ModifierCell` (`x, y, z, id`),
  `core.ecs.system` um `EcsBatch` (`n, dt, tick, id` e uma coluna por campo), uma etapa de mapgen um
  `MapgenStage` (`seed, height, depth, min_x, min_z, max_x, max_z`), `core.craft.pose` uma
  `CraftPose` ou nil. Um mod pode nomeá-los: `local function stage(ctx: MapgenStage)`.

**Os erros que um mod `--!strict` mais encontra**, e o que os resolve sem mudar o que o código faz:

| o verificador diz | porque | escreva |
|---|---|---|
| `Operator '-' could not be applied to operands of types unknown and number` | uma tabela literal usada como mapa: `SIZE[name]` | `local SIZE: { [string]: number } = { ... }` |
| `Expected type table, got 'string \| {number} \| ...'` | uma lista de tuplas misturadas | `local LOOT: { { any } } = { ... }` |
| `Cannot call a value of the union type` | uma função declarada antes (`local f: (number) -> () = function() end`) e atribuída depois, chamada com um `ev.*` | faça o cast do argumento na chamada: `f(ev.player :: number)` |
| `Value of type 'X?' could be nil` depois de um `if v then` | `v` é um upvalue, e uma verificação num upvalue não o estreita | copie antes: `local b = boss; if b then ... b.id` |
| `Expected this to be 'number', but got 'number?'` | um valor que o código sabe que está lá (o campo de uma entidade definido no spawn, o irmão de um valor verificado) | `core.ecs.get(e, "vx") :: number` |
| `Unknown global 'x'` | um nome usado **acima** do seu `local` é uma global, e outra | suba o `local`: isto é um bug, não um tipo |
| `Unknown require: unsupported path` | `require("./x")` num `init.luau` | `require("@self/x")` |
| `Module does not return exactly 1 value` | o arquivo pedido é executado pelo que faz | `return nil` no fim dele |

A linha `Unknown global` não é tipo nenhum: o handler de `keys:level` de `backrooms_exit` escrevia em
`level_d` acima da linha que o declara, então indexava uma global que não existe e parava ali, antes
de pôr o jogador no nível -- e as luzes que se apagam a partir do nível 5 nunca se apagaram. É para
isso que serve uma verificação limpa: os erros de tipo escondem os poucos que não são.

**O que uma verificação limpa não vê**, achado no mesmo dia em código que passava nela:

- **0 é verdadeiro.** Com `api = 1` as chamadas que respondem sim ou não (`core.loaded`,
  `core.node_solid`...) respondem 1 ou 0, e em Luau só `nil` e `false` são falsos:
  `if core.loaded(x, y, z) then` sempre entra. As bandeiras do front subiam em chão que ainda não
  existia, uma vez, da 0.9 à 0.18 -- e é por isso que `api = 2` responde true ou false.
- **O texto de um cliente não é um número até ser verificado.** `tonumber(w[i]) :: number` diz ao
  verificador que é; um cliente que envia outra coisa faz o handler falhar, e um handler que falha
  desliga o mod -- para todos naquele mundo. Verifique cada um e descarte a mensagem.

### O modo sobrevivência: níveis atrás dos portais

Um mundo novo pode ser **sandbox** (a praça com os seus portais) ou **sobrevivência** (sem praça,
`cnp_server --survival` na sala do sandbox, o arquivo `mode` do mundo). **Um mod nunca pergunta o
modo**: ele olha os dados. Assim uma correção num mod vale para os dois.

- `core.plaza()` dá o `lo, hi` da praça (20, 100) ou `nil` onde não há praça: quem a mantinha livre
  (árvores, vilas, ruínas, ...) ou construía sobre ela (degraus, banco, xadrez, ...) segue essa
  resposta.
- `core.seed()` é a semente também fora de uma etapa de mapgen; `core.world_spawn()` já responde
  enquanto os mods carregam, num mundo sem praça.
- `core:died {player, x, y, z}`: um jogador morreu (já posto de volta na cama ou no spawn).
  `mods/grave` põe a bolsa numa sepultura ali (`core.bag_to_container`).
- `core.mob_health(id, hp)`: uma criatura mais resistente do que `core.register_mob` a faz.

**Os locais** são de `mods/gates`, uma função da semente e de células de 256 nós: onde ficam, de qual
mundo, com qual dificuldade `d` de 1 a 10 (uma a mais a cada 300 nós a partir do spawn). As bocas de
cada mundo (duas para os sete com chave) vão para os locais mais próximos dos jogadores; quem entra
leva a linha `core.player_carried`:

```
site=<id> d=<1..10> s=<semente do local> t=<sala>
```

Outros mods pedem locais com `gates:ask {x, z, r, tag, player}` e recebem na hora
`gates:sites {tag, list = "sala d x z;..."}`, sem os que `player` já fez (`mods/villagers` faz deles
boatos e direções). `maps:pin {player, label, x, z, colour}` põe um ponto no mapa (1 roxo, 2 verde).

**O protocolo dos níveis** está em `mods/keys`, ligado em todo mundo temático. Quem chega de um local
recebe um **slot** daquele mundo (0 é a prova escrita à mão, sempre onde estava). O slot `n` é um
quadrado de 2048 que começa em `x = 8192 + (n % 16) * 2048, z = (n // 16) * 2048`. Um mundo com um
nível próprio responde aos eventos assim:

| evento | emitido por | campos | o que quer dizer |
|---|---|---|---|
| `keys:level` | `mods/keys` | `player, slot, d, seed, ox, oz` | construa o nível do slot e leve o jogador até lá; cancelável: quem o cancela o tomou para si, senão o jogador faz a prova do mundo |
| `keys:start` | o mundo | `player, slot, ex, ey, ez` | a saída: um buraco de 1 de largura e 2 de altura em `ex ey ez`, virado para +x, a 8-20 do começo |
| `keys:cleared` | o mundo | `player, slot` | o nível foi superado |
| `keys:earned` | o mundo | `player, key` | a prova do mundo foi superada (vale também para um nível) |
| `keys:reward` | `mods/keys` (sandbox) | `player, list` | as recompensas ganhas, para `mods/gates`, que as põe num baú no local |

**A saída escondida.** As duas células em `ex + 1` (na frente do buraco) são uma parede do nível, e a
célula acima delas, em `ex + 1, ey + 2`, é preenchida por `mods/keys` com uma lâmpada apagada
(`keys:hint`): o único sinal. Em `keys:cleared` as duas células viram ar, a lâmpada acende e a
primeira boca do mundo para o sandbox vai para o buraco, aberta só para quem terminou. O nível traz o
jogador de volta com uma peça de mão única, em geral um portal do próprio mundo (`core.portal_open`,
aberto só depois que o nível foi superado: dois portais ligados vão nos dois sentidos). Um nível com
`d >= 4` dá a chave do mundo; todo nível dá uma recompensa.

A etapa de mapgen do mundo reconhece as colunas de um slot pelo x (`>= 8192`) e constrói a planta
dele, uma função do slot, de `d` e da semente salvos no armazenamento do mod na chegada. Assim as
colunas são feitas quando precisam e ficam no arquivo do mundo. Exemplos: `games/parkour/mods/parkour`
(estações e saltos), `games/backrooms/mods/backrooms_exit`, `games/arpg/mods/arpg_game`
(`L.add_level`), `games/track/mods/track_core` (um circuito, `track_core:level` / `track_core:laid`),
`games/chambers/mods/chambers_game` (uma cópia da fileira de câmaras a partir de x 20000).

### Níveis feitos de peças salvas: `mods/zones`

Uma **zona** é uma caixa do mundo salva como peça de um nível; um **nível** são zonas unidas passagem
a passagem. O mod guarda as zonas (no armazenamento compartilhado do servidor, então toda sala tem as
mesmas) e monta os níveis; o mod próprio de um mundo pede um e põe as suas coisas onde estão os
marcadores.

**Fazer uma**, no jogo: construa a peça, depois na caixa dela ponha

- blocos `zones:doorway` (**Passagem**), um retângulo cheio deles num dos quatro lados (não num
  canto, não dentro): salvos como o buraco que marcam, com o seu lado, a sua largura e altura, e o
  bloco embaixo deles, que é o que fecha a passagem quando nada se junta a ela;
- `zones:start`, `zones:exit`, `zones:mob`, `zones:loot`, `zones:key`, `zones:trigger`: salvos como
  ar, com qual marcador era e para que lado olhava (eles giram `upright`);

e `/zone save MUNDO NOME X1 Y1 Z1 X2 Y2 Z2 [start|room|end]`. A zona é `MUNDO.NOME`, no máximo 64 x 64
e 48 de altura. `/zone set ID d MIN MAX | weight N | mirror on|off | role start|room|end` diz para
quais dificuldades ela serve (de 1 a 10), com que frequência aparece, se pode ser montada espelhada, e
se começa um nível, termina um ou é uma sala no caminho.

**De quem ela é.** Uma zona pertence à chave que a salvou (`core.player_key`) e é `private`: só os
níveis pedidos com essa chave como `owner` a usam. `/zone propose ID`, de quem a fez, a deixa
`proposed`: os admins na sala são avisados, um admin que entra depois ouve quantas estão esperando, e
`/page zones` as lista, cada uma com **show** (`/zone show ID`: aquela zona sozinha, montada ao lado
do admin como foi salva), **approve** e **refuse**. `/zone approve ID` de um admin a deixa
`approved`, em todo nível do seu mundo; `/zone refuse ID` (ou `private`) a deixa privada de novo, e o
mesmo acontece quando quem a fez salva por cima ou a muda: o que foi olhado era a zona como ela era.

**A oficina** (`mods/workshop`) é onde um jogador faz tudo isso sem coordenadas: `/workshop` o leva a
uma sala própria, `workshop-` e o começo da sua chave, feita a partir do modelo `[room workshop-*]` do
arquivo de salas (os mundos da tela de título e `tools/track.sh` o escrevem; um servidor sem ele diz
isso). A sala é plana, pacífica, sem fome nem tempo; o dono voa, não se machuca, cava rápido, um
bloco colocado não é gasto e um cavado não dá nada. Ali não há catálogo: a bolsa é a mesma em todas
as salas. Em vez disso, um **Pincel**, dado na entrada, coloca os blocos próprios de um mundo:
`/workshop blocks`, o botão **Blocos** do painel ou o pincel num bloco sem nenhum escolhido abrem a
sua paleta -- os blocos de mundo que a oficina tem (`games/workshop/mods`, links para os mods de
blocos dos mundos, que numa sala `workshop-*` deixam de fora as regras do seu mundo; os das câmaras de
testes, das terras devastadas, dos backrooms, do parkour, do Inferno, do front e do assalto) e os
marcadores. Um clique direito põe o escolhido na face clicada; uma placa, uma saída de ar, um alvo ou
o fundo de um poço toma o lugar do bloco clicado; uma porta são as suas quatro células atravessadas
na direção para onde o jogador olha, um botão ou uma placa os seus dois por dois. Uma **Varinha**,
dada na entrada, escolhe a caixa por dois cantos (desenhada na tela) e abre um painel: nome, mundo,
start/room/end, dificuldades, peso, espelho, depois **Salvar**, **Testar** (um nível com as zonas
daquele mundo, as aprovadas e as do próprio jogador, montado num terreno de testes 400 ao sul do
spawn, tirando antes o anterior, com o jogador posto no começo dele), **Voltar** e **Propor**.
`/zone try` e `/zone show` são de qualquer um na própria oficina, e de um admin em outros lugares.
Outro mod executa uma linha de `/zone` como se um jogador a tivesse dito com
`core.event.emit("zones:do", { player = p, line = "propose chambers.hall" })`.

**As zonas que vêm com o jogo** estão em `mods/zones/shipped.luau`, que uma ferramenta escreve a
partir de um arquivo de mundo (o da primeira sala, onde fica o armazenamento compartilhado do
servidor), num mundo que nenhum servidor tem aberto:

```
python3 tools/zones_ship.py WORLD_FILE --list        # o que há ali
python3 tools/zones_ship.py WORLD_FILE               # toda zona aprovada, no lugar das do arquivo
python3 tools/zones_ship.py WORLD_FILE --keep chambers.hall   # essa, acrescentada às do arquivo
```

`--all` pega também as privadas, `--world NOME` as de um mundo, `-o ARQUIVO` escreve em outro lugar.
Uma zona distribuída com o jogo é aprovada e de ninguém, em todo servidor. Se alguém salva por cima
com `/zone save`, a do próprio servidor fica no lugar dela ali; `/zone delete` dessa traz de volta a
do jogo, e a do jogo em si não pode ser apagada.

**Pedir um nível**, a partir de outro mod:

```luau
core.event.on("zones:mark", function(ev)
    -- ev.tag, ev.kind ("start", "exit", "mob", "loot", "key", "trigger"), ev.x, ev.y, ev.z,
    -- ev.facing (de 0 a 3, girado como a sua zona)
end)
core.event.on("zones:laid", function(ev)
    -- ev.tag, ev.pieces (0: o mundo não tem zona para começar), e a caixa do nível,
    -- de ev.x1 ev.y1 ev.z1 a ev.x2 ev.y2 ev.z2
end)
core.event.emit("zones:lay", { tag = "mine 7", world = "chambers", owner = core.player_key(player),
                               x = 9000, y = 20, z = 100, seed = 42, d = 3, count = 6 })
```

`x y z` é onde vai o canto mais baixo da primeira zona, sem giro; as outras crescem a partir das
passagens dela, para onde quer que estejam viradas. O nível é escrito a 4000 células por tick, cada
`zones:mark` chega quando a sua zona está de pé, depois `zones:laid`. `tag` é uma palavra de quem
pede, para reconhecer as respostas.

Um mundo cujos blocos próprios fazem algo (uma porta, um botão) os nomeia uma vez, enquanto o seu mod
carrega, e ouve onde cada um foi montado, com a orientação girada como a sua zona, antes de
`zones:laid`:

```luau
core.event.emit("zones:watch", { names = "chambers_core:door chambers_core:button" })
core.event.on("zones:block", function(ev)
    -- ev.tag, ev.name, ev.x, ev.y, ev.z, ev.facing
end)
```

**As câmaras de testes feitas de zonas** (`games/chambers/mods/chambers_game`). As quinze câmaras
continuam como estão; o nível de um local de sobrevivência é montado com as zonas de câmaras
aprovadas quando há alguma (senão com a cópia da fileira, como antes), o botão de pressão da última
sala monta um nível delas para o jogador que o apertou (num lugar próprio, mais difícil a cada um
superado, de volta à última sala depois da saída), e `/zone try chambers` também funciona ali. O que
há num nível funciona por regra:

| Construído | Faz |
|---|---|
| `chambers_core:door` (quatro células) | abre quando alguém chega perto, a não ser que uma das quatro coisas abaixo a comande: cada uma comanda a porta mais próxima |
| um botão de chão (dois por dois) | todos os botões de uma porta apertados (por um corpo ou um cubo): ela está aberta |
| um botão de pressão | a porta aberta por 12 s, o relógio em cima dela |
| um alvo | um corpo em cima abre a porta de vez; uma placa de lançamento joga no mais próximo |
| marcador `zones:mob` | uma torreta sentinela, virada para o começo; ela acorda quando alguém chega a menos de 12; todas as de uma porta derrubadas: ela está aberta |
| uma saída de ar (no teto) | solta um cubo, e de novo quando ele cai para fora do nível ou fica no fundo de um poço |
| uma placa de lançamento (dois por dois, no chão) | joga um corpo no alvo mais próximo a menos de 24, direto para cima se não houver |
| um pedestal | a arma inteira |
| `chambers_core:pit` (no chão) | um segundo em cima dele, ou embaixo do nível, e um corpo volta ao começo |
| marcadores `zones:start`, `zones:exit` | onde um jogador é posto; o nível superado (o de um local: a saída dele se abre, com um portal até lá) |

Os painéis brancos aceitam portais como em todo lugar nas câmaras. O campo dissolvente e os portais
fixos ainda não estão nas zonas.

**As terras devastadas feitas de zonas** (`games/arpg/mods/arpg_game`). As terras de um local de
sobrevivência são montadas com as zonas arpg aprovadas quando há alguma (senão com as áreas próprias
do arpg, como antes), `count` 3 + d; `/zone try arpg` também funciona ali. No arpg tudo fica numa
altura só, então o chão de uma zona arpg é a sua camada mais baixa: montado no chão das terras, o que
está em cima dele fica onde ficam as criaturas e os baús do arpg. O nível inteiro é uma área das
terras, no nível do local (2 d - 3); por marcador:

| Marcador | É |
|---|---|
| `zones:start` | onde o jogador entra; a saída do local é um bloco de rocha quatro a oeste dele |
| `zones:mob` | um bando das criaturas da zona, maior conforme o nível; um em cada cinco é um campeão com os seus três |
| `zones:exit` | o senhor (o senhor das tumbas, a asa de brasa a partir de d 6) e quatro dos seus: derrubado, as terras foram superadas e um portal leva de volta ao começo |
| `zones:loot` | um baú |
| `zones:trigger` | uma coisa para quebrar: um barril, um caixote ou uma urna |

`/zone try` monta um nível com a sua camada mais baixa no nível do chão embaixo dos pés do jogador.

**Os backrooms feitos de zonas** (`games/backrooms/mods/backrooms_exit`). O nível de um local de
sobrevivência novo é montado com as zonas backrooms aprovadas quando há alguma, 2 + d delas no Nível 0
(o chão, a camada mais baixa de uma zona, em y 20); um local já jogado mantém o nível que tinha, e sem
zonas ele é o das próprias salas, como antes. O jogador entra em `zones:start`, a saída do local é um
bloco de papel de parede quatro a oeste dele, e a sala de saída é onde está `zones:exit`: o rádio leva
até lá e chegar lá supera o nível. Todo o resto de uma zona são os seus blocos -- as suas próprias
luzes -- e o escuro traz as criaturas como em todo lugar ali.

**O parkour feito de zonas** (`games/parkour/mods/parkour`). O nível de um local de sobrevivência novo
é montado com as zonas parkour aprovadas quando há alguma, 3 + d delas, com a camada mais baixa no
chão do percurso (y 30); um local já jogado mantém o seu. O jogador entra em `zones:start` (a saída
num bloco de concreto a leste dele), cada `parkour:pad` montado é um lugar para onde uma queda leva de
volta -- primeiro o mais perto do começo -- e `zones:exit` é o cume: alcançado, o nível foi superado
e uma passagem ali leva para baixo, ao começo. Trampolins e lajes que cedem funcionam como em todo o
percurso; um trampolim numa zona joga para onde o corpo olha.

**O Inferno feito de zonas** (`games/inferno/mods/inferno_heart`). A fortaleza de um local de
sobrevivência novo é montada com as zonas inferno aprovadas quando há alguma, 2 + d delas, com a
camada mais baixa no chão (y 29); um local já jogado mantém a sua. O jogador entra em `zones:start` (a
saída num bloco de tijolo a oeste dele); cada `inferno_heart:brazier` montado é um braseiro para
acender e cada bloco `inferno_heart:gate` montado é o portão que eles abrem; o guardião se ergue em
`zones:exit` quando todos ardem, com d - 1 dos da fortaleza, e cada `zones:mob` é um brute ou um
espectro que sai quando alguém chega a menos de 16. O guardião derrubado supera o nível, e uma célula
de fogo perto da saída leva de volta ao começo.

**Zonas espalhadas por um campo.** Um mundo de terreno aberto pede zonas lado a lado em vez de unidas:
fileiras ao longo de z, cada uma preenchida ao longo de x com zonas sorteadas pelo peso, giradas e (se
puderem) espelhadas pela semente, com duas células do chão do próprio campo entre elas, toda passagem
fechada. O campo é aplainado antes -- `ground` em y - 1, ar por `h` para cima --, mas só quando há
zonas para montar; sem nenhuma, `zones:laid` diz 0 peças e o campo fica como está.

```luau
core.event.emit("zones:scatter", { tag = "field", world = "front", owner = "", x = 8, y = 20, z = 44,
                                   w = 240, len = 168, h = 16, ground = "front_core:grass", seed = 7, d = 5 })
```

**O front** (`games/front/mods/front_game`) tem a faixa entre os pátios das duas bases (z de 44 a
211, x de 8 a 247) feita das zonas front aprovadas no começo e a cada rodada nova: uma rodada é
lutada em terreno novo, e inteiro. Os seus marcadores `zones:key` são os pontos a tomar -- o mais a
oeste, o mais perto do meio, o mais a leste; se forem menos de três, os do próprio campo ficam no
lugar deles. **O assalto** (`games/assault/mods/assault_game`) mantém as suas fortalezas, o seu salão
e a sua fuga como estão, e espalha as zonas assault aprovadas no vale diante de cada uma das três
primeiras fortalezas, dos dois lados da estrada, quando aquela fortaleza é guarnecida (montadas à
frente dos aliados: a dimensão não fica guardada em disco nenhum, e uma escrita numa coluna ainda não
feita espera por ela).

**Como um nível é montado** (`layout.luau`, que não lê nem escreve mundo nenhum: as mesmas zonas,
semente, quantidade e `d` dão o mesmo nível em qualquer lugar). Primeiro uma zona "start"; depois uma
"room" unida à passagem aberta mais recente em que ela se encaixa -- mesma largura, altura e tipo, as
duas faces opostas --, testando cada uma das suas passagens e, se puder ser espelhada, dos dois
jeitos, numa ordem sorteada pela semente, peso 3 três vezes mais provável de vir primeiro que peso 1;
duas zonas nunca dividem uma célula; uma zona "end" por último. Uma tentativa que se enrola sobre si
mesma e não tem espaço para o fim é descartada pela próxima, dezesseis no máximo. Um lado é 0 +x, 1
-z, 2 -x, 3 +z, para que um quarto de volta leve cada um ao seguinte.

**O giro de um bloco gira com a sua zona** (`turns.luau`): `upright` e `facedir` por duas tabelas
tiradas das próprias matrizes do motor (`src/core/media/face_tiles.h`), `wallmounted` pela sua face. Um
bloco espelhado fica certo quando a sua forma é igual dos dois lados do seu meio -- uma escada, um
tronco, um baú; uma zona com um bloco que não é assim não deve ser posta em `mirror on`.

**Ainda não existe**: um nível montado por uma etapa de mapgen, que um slot de sobrevivência quer
(TODO.md); zonas distribuídas com o jogo como arquivos; tipos de passagem além de "door".

---

## Exemplo mínimo

```lua
--!strict
local BLUE = core.register_node("mine:blue", 60, 90, 220)

for i = 0, 9 do
    core.set_block(50 + i, 20, 50, BLUE)
end

local t = 0.0
function on_step(dt: number)
    t += dt
end
```

com

```
name = mine
version = 0.1.0
capabilities =
```

Os mods em `mods/` são exemplos que funcionam:

- `spike` -- nenhuma capacidade, uma torre e um portal que se move a cada frame;
- `beacons` -- `capabilities = os`, `depends = spike`, um farol que pulsa;
- `palette` -- vinte blocos declarados num laço, que também é a prova de que o atlas cresce sozinho
  (de 16 para 64 tiles) sem mexer em nada no motor;
- `guard` -- cancela `core:pre_place` dentro da praça dos portais, troca o bloco acima de y=30, e
  dispara um evento próprio que `beacons` escuta a partir da sua própria VM. Nenhuma linha do motor
  foi escrita para a regra "aqui não se constrói";
- `garden` -- dois pares que escalam (`core.portal_scale`) num mundo só: uma abertura do tamanho de uma
  porta para uma de um oitavo de bloco de altura, então quem entra sai com um dezesseis avos do
  tamanho, e uma abertura pequena sob um vidro para uma doze vezes mais larga pendurada de cara para
  baixo sobre o jardim, uma miniatura para olhar dentro; a pequena num batente de peças `core.node_box`
  com um trinta e dois avos de nó de espessura; um corpo pequeno ali anda conforme o seu tamanho faz
  andar (sem aumento: a mesma sensação do tamanho normal); e insetos no seu tamanho real,
  trabalhadores que o mod conduz: formigas entre um formigueiro e as flores (cada uma na sua própria
  faixa ao longo de um caminho reto, `core.mob_place` a cada tick), joaninhas passeando, borboletas
  voando de flor em flor (`core.mob_place` a cada tick, `hover`), cada um com os seus sons `_say`,
  `_hurt` e `_death` (`tools/make_garden_sounds.py`);
- `portal` -- uma moldura de `portal:frame` em volta de um buraco de ar, em pé ou deitada, abre um
  portal (`core.portal_open`, quantas molduras você quiser: o servidor dá as 40 aberturas reais para
  as que estão perto de jogadores) e o liga à última que ficou sozinha (`core.portal_link`); clique
  direito na moldura abre o seu menu, com as outras molduras às quais ligá-la. Até ser ligada, o
  cliente a fecha com uma membrana verde animada, que se abre do centro para a imagem quando a outra
  ponta chega (0,9 s);
- `chambers_game` e `lab` -- a arma de portais: cada tiro é um `core.portal_open` com a cor do botão
  (1 azul, 2 laranja), então o primeiro portal disparado fica fechado na sua cor até o segundo
  existir;
- `games/track` (`track_core`) e `games/inferno` (`inferno_track`) -- a pista do inferno, entre duas
  salas com três bocas cada (até 4 por mundo): a moldura A da pista (abertura 1,
  `--portal-to @inferno/2`) leva a B, a abertura 2 do inferno, que leva de volta a A (`@pista/1`); C
  (a abertura 1 do inferno, `@pista/2`) leva a D, a abertura 2 da pista, que leva de volta a C
  (`@inferno/1`). As aberturas 2 e 3 são postas com `core.set_portal` e `core.portal_place` como as
  duas primeiras. Um corpo que dirige (`core.body_drive`) passa para a outra sala ainda no seu kart,
  se aquela sala conhece o modelo que ele veste, e junto com ele a linha de `core.player_carry` (aqui:
  a volta em andamento), que é lida do outro lado com `core.player_carried`; a sala deixada sabe disso
  por `core.body_taken` e não estaciona um kart que foi embora. `track_game` roda nas duas salas e
  cronometra só os seus próprios portões (`inferno = true`). Atrás de uma moldura que um kart
  atravessa precisa haver espaço: o kart passa do plano alguns nós antes de a travessia ser
  respondida;
- `tools` -- um bloco que pede nível 1, a picareta que o abre e a pá que não abre, com as receitas
  para fazer mais. É o lugar de uma ferramenta: o motor põe segundos e materiais nos seus próprios
  blocos e deixa níveis e ferramentas para o conteúdo.
