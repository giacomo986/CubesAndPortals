# Administrar um servidor

*[Read in English](ADMIN.md) · [Leggi in italiano](ADMIN.it.md)*

Tudo o que um dono ou um membro da equipe pode fazer com um `cnp_server`: os comandos, quem pode
usá-los, as opções e as chaves do `server.conf`, os scripts, e o que cada um guarda no disco. O
código é [`src/server/admin.h`](../src/server/admin.h) e `admin.cpp`; todo comando passa por uma
única função ali, seja qual for o jeito como foi digitado.

## Índice

- [De onde vêm os comandos](#de-onde-vêm-os-comandos)
- [Cargos e permissões](#cargos-e-permissões)
- [Comandos](#comandos)
- [Páginas no jogo](#páginas-no-jogo)
- [Opções e server.conf](#opções-e-serverconf)
- [Mantê-lo rodando](#mantê-lo-rodando)
- [Reinícios, paradas e manutenção](#reinícios-paradas-e-manutenção)
- [O que o silenciamento segura](#o-que-o-silenciamento-segura)
- [Regras para aceitar](#regras-para-aceitar)
- [Contas alternativas](#contas-alternativas)
- [Verificação de atualizações](#verificação-de-atualizações)
- [Visitas, logs e denúncias](#visitas-logs-e-denúncias)
- [Blocos: inspeção e rollback](#blocos-inspeção-e-rollback)
- [Backups](#backups)
- [Webhooks](#webhooks)
- [Portais entre servidores](#portais-entre-servidores)
- [Servidores amigos](#servidores-amigos)
- [O que é guardado, e onde](#o-que-é-guardado-e-onde)
- [Limites](#limites)

## De onde vêm os comandos

| Entrada | Como | Cargo |
|---|---|---|
| Chat | `/ban bob 2h griefing` digitado no jogo | o do jogador |
| Console | o stdin do servidor, sem a barra: `ban bob 2h griefing` | acima de todos os cargos |
| RCON | `--rcon PORTA --rcon-password SENHA`, TCP só em 127.0.0.1; `CNP_RCON_PASSWORD=SENHA tools/admin.sh -p PORTA ban bob 2h` | acima de todos os cargos |
| Páginas | `/admin` no jogo, botões que rodam os mesmos comandos | o do jogador |

O protocolo RCON é feito de linhas simples: primeiro a senha (resposta `ok`), depois um comando
por linha, cada um respondido com as suas linhas e uma linha com um `.` sozinho. Coloque a senha
no `server.conf`, não na linha de comando, onde o `ps` a mostra.

`/help` lista os comandos que quem pergunta pode usar. Um mod não pode declarar um comando com o
nome de um comando de administração, nem `page` ou `admin`.

## Cargos e permissões

`guest < player < helper < moderator < admin < owner`.

- **guest** (convidado): um jogador sem identidade (sem chave). Não se pode dar nada a um guest,
  porque um nome sem chave é um rótulo que qualquer um pode digitar.
- **player** (jogador): todo mundo que tem uma chave, se não recebeu mais.
- **owner** (dono): `--owner NOME` (o cliente passa isso para os mundos que ele mesmo inicia), ou
  `/role NOME owner` no console.

Os cargos são dados por chave: um jogador mantém o seu com outro nome. Ninguém dá um cargo igual
ou acima do próprio, e ninguém age sobre um jogador do mesmo cargo ou acima (menos o console).

`/role NOME CARGO SALA` dá um cargo só em uma sala: nessa sala o jogador tem o mais alto dos dois.
Um moderador da arena é um jogador no sandbox.

Cada comando pede uma permissão, e cada permissão quer um cargo mínimo. O padrão:

| Cargo | Permissões |
|---|---|
| helper | `mute`, `pages`, `staffchat`, `warn`, `reports`, `notes`, `players`, `inspect` |
| moderator | `kick`, `ban`, `creative`, `announce`, `whois`, `rollback`, `bag`, `alts` |
| admin | `ban.ip`, `access`, `roles`, `restart`, `maintenance`, `motd`, `rules`, `logs`, `backup`, `time`, `portals`, `give`, `oldbags` |
| owner | `perms`, `federation` |

`/perm PERMISSÃO CARGO` (owner) passa uma permissão para outro cargo; `/perm` lista todas.
`/perm PERMISSÃO nobody` fecha a permissão para todos, o dono também (só o console fica acima).
Um mundo de sobrevivência (`--survival`) começa com `creative` em `nobody`: ninguém tem o
catálogo (C); `/perm creative owner` (no console) o devolve.
O nome de um cargo também é uma permissão (`moderator`), para um mod que não tem palavra melhor.

Mods: `core.command(nome, ajuda, permissão)` protege um comando de um mod; `core.has_perm(jogador,
permissão)` e `core.role(jogador)` perguntam (veja [`API.md`](API.md)).

## Comandos

Todo comando de chat, os dos jogadores e dos mods também, com o que faz: [`COMMANDS.pt.md`](COMMANDS.pt.md).

As durações são `45s`, `30m`, `2h`, `7d`, `1w`. NOME é um jogador online em qualquer sala, ou o
dono desse nome neste mundo, ou só o nome.

### Quem pode entrar

| Comando | Permissão | O que faz |
|---|---|---|
| `/perms` | qualquer um | O seu cargo aqui |
| `/role [NOME [CARGO [SALA]]]` | roles | Lista os cargos dados; mostra o de um jogador; dá um (em uma sala, com SALA). `player` tira o cargo |
| `/kick NOME [motivo]` | kick | Fora de todas as salas, das janelas dos portais também; o cliente volta ao título dizendo por quê |
| `/ban NOME [tempo] [motivo]` | ban | Fora e mantido fora, por um tempo ou para sempre. Um nome com dono bane a chave do dono; um nome sem dono bane também o endereço de onde veio da última vez |
| `/banip NOME\|ENDEREÇO [tempo] [motivo]` | ban.ip | Mantém um endereço fora |
| `/unban NOME\|ENDEREÇO` | ban | Tira um banimento. Tirar aqui o banimento de um servidor amigo o mantém tirado |
| `/bans` | ban | Quem está banido, por quem, até quando |
| `/mute NOME [tempo] [motivo]` `/unmute NOME` | mute | Segura o chat de um jogador, e as palavras dele onde quer que um mod as repasse (abaixo). Um nome que ninguém tem ainda é silenciado pelo nome, e o silenciamento continua quando ele chega com uma chave |
| `/whitelist on\|off\|list\|add NOME\|remove NOME` | access | Só os jogadores da lista, e a equipe, entram |
| `/perm [PERMISSÃO CARGO]` | perms | Que cargo cada permissão quer |
| `/warn NOME motivo` | warn | Uma advertência; três em 30 dias são um dia de banimento |
| `/rules [set TEXTO\|off]` | qualquer um (rules para mudar) | As regras que todos aceitam; definir regras novas pede a todos de novo |
| `/accept` `/leave` | qualquer um | Aceita as regras, ou sai do servidor sem aceitá-las |

### A vida do servidor

| Comando | Permissão | O que faz |
|---|---|---|
| `/restart [5m\|now\|cancel] [motivo]` | restart | Reinicia com avisos em todas as telas (10m, 5m, 1m, 30s, 10s e os últimos segundos). O servidor sai com o código 75 para o `tools/serve.sh`; os clientes postos para fora como "restarting" entram de novo sozinhos |
| `/stop [5m\|now\|cancel] [motivo]` | restart | O mesmo, e o servidor fica parado |
| `/maintenance on [texto]\|off` | maintenance | Só a equipe entra; os outros são postos para fora com o texto. A lista de servidores no cliente mostra "maintenance" |
| `/motd [TEXTO\|off]` | motd (para mudar) | A mensagem do dia, dita no chat a cada jogador que entra |
| `/announce TEXTO` | announce | Um aviso em todas as telas |
| `/backup` | backup | Copia agora todos os arquivos de mundo |
| `/metrics` | logs | Tempo ligado, jogadores e janelas por sala, o maior intervalo entre dois ticks |
| `/weather [clear\|rain\|storm] [MINUTOS]` | admin | O céu do sandbox (`mods/weather`): sem palavra, como está e quanto dura; com uma, esse céu agora, por MINUTOS ou pelo tempo de sempre |
| `/time [day\|noon\|dusk\|night\|midnight\|dawn\|HH:MM]` | time (admin) | O relógio do mundo onde você está, em todo mundo: sem palavra, a hora; com uma, essa hora na hora para todos lá. No sandbox, voltar o relógio para antes da meia-noite conta um dia, como uma noite dormida |
| `/story NOME [keys all\|MUNDO...\|ending\|clear]` | admin | O progresso da história de um jogador que está no seu mundo: sem palavra, quantas chaves e se l'Ospite foi derrotado. `keys all` ou `keys pista inferno ...` registra essas chaves e as põe na bolsa (mundos: pista inferno arena arpg chambers backrooms parkour); `ending` são as sete mais l'Ospite derrotado, o que abre o front e o assalto e dá as recompensas; `clear` tira tudo (`mods/keys`) |
| `/season [spring\|summer\|autumn\|winter]` | admin | A estação do sandbox (`mods/weather`, cinco dias cada): sem palavra, qual é e que dia dela; com uma, o primeiro dia dessa estação neste ano |
| `/difficulty [peaceful\|easy\|normal\|hard]` `/peaceful [on\|off]` | admin | Quão difícil é o mundo -- a sala onde o admin está, cada sala de um servidor por conta própria (`mods/rules`, core.set_difficulty): pacífico manda embora toda criatura que luta e não deixa nenhuma sair; fácil e difícil ajustam os golpes delas, o número delas e a fome. Guardado em WORLD.difficulty (`--difficulty`) |
| `/rule [NOME on\|off]` | admin | As regras do mundo (`mods/rules`, core.set_rule): keep_bag, day_cycle, weather_cycle, hunger, e as que os outros mods do mundo declararem (o evento `rules:declare`, explicado em `mods/rules/init.luau`); guardadas em WORLD.rules. O botão Mundo do menu de pausa mostra todas em `/page world`, com o tempo, a hora e a estação, como botões para um admin |
| `/trophies sites` `/trophies go ID [place] [NOME]` `/trophies home [NOME]` | admin | Onde estão os cinco portais escondidos e se cada um está construído; você ou NOME na frente de um (`well`, `summit`, `lake`, `hill`, `circle`) ou, com `place`, no lugar para onde leva; NOME (ou você) no próprio quarto, para ver a estante de troféus (`mods/trophies`) |
| `/village siege\|feast\|caravan\|incoming` | admin | Começa agora um dos eventos das vilas na vila onde o admin está (`mods/villagers`): um cerco anunciado agora que chega ao anoitecer, a festa da colheita na praça dela, uma caravana para a outra vila mais próxima (nenhuma se já há uma na estrada ou se os baús não têm nada sobrando), ou uma que vem de uma vila a 100-900 que ninguém visitou, pelos caminhos (nenhuma se nenhuma vila assim está ligada a eles) |

### Jogadores

| Comando | Permissão | O que faz |
|---|---|---|
| `/seen NOME` | qualquer um | Quando um jogador esteve aqui pela última vez, e por quanto tempo no total |
| `/players [7d]` | players | Quem entrou nos últimos tempos, visitas e tempo jogado |
| `/whois NOME` | whois | Chave, nomes usados, endereços, advertências, notas, banimentos |
| `/alts NOME` | alts | Outros jogadores que entraram de um endereço de onde este entrou, e quais estão banidos |
| `/bag NOME` | bag | O que um jogador carrega, casa por casa, e o que as mochilas dele guardam; offline, a bolsa guardada aqui; e se ela foi para um servidor amigo |
| `/kill [NOME]` | qualquer um (a si mesmo), moderator (outra pessoa) | Morre e volta à sua cama ou ao spawn, em modo criativo ou não (`mods/creative`); a armadura se desgasta um pouco, como com qualquer golpe |
| `/giveup` | qualquer um, num mundo com uma prova | Desiste da prova (`mods/keys`): morre, depois passa pela entrada do mundo (abertura 0) e sai no sandbox ao lado do portão daquele mundo; um corpo que não pode ser ferido só passa |
| `/creative on\|off` | admin | Modo criativo para si mesmo (`mods/creative`): voar sem o jetpack, nenhum dano, cavar dez vezes mais rápido. Guardado pelo nome entre reinícios. Num mundo da tela de título o jogador é o dono, então lá é livre. `/creative TEXTO` continua sendo a busca do catálogo, sob a permissão `creative` |
| `/give NOME ITEM [QUANTIDADE]` | give | Itens pelo nome (`chest:box`) na bolsa de um jogador online |
| `/report NOME o que aconteceu` | qualquer um | Uma denúncia para a equipe, contada na hora a quem está online |
| `/reports` | reports | As últimas denúncias |
| `/note NOME texto` | notes | Uma nota sobre um jogador, mostrada no `/whois` |
| `/sc TEXTO` | staffchat | Chat entre a equipe online |
| `/logs [N] [AÇÃO]` | logs | As últimas ações de administração, todas ou de um tipo (`ban`, `rollback`...) |

### Blocos

| Comando | Permissão | O que faz |
|---|---|---|
| `/inspect X Y Z [SALA]` | inspect | Os últimos dez jogadores que mudaram aquele bloco, de quê para quê, quando |
| `/rollback NOME DURAÇÃO [SALA]` | rollback | Põe de volta todo bloco que esse jogador mudou nesse tempo |
| `/oldbags NOME [N]` | oldbags | As cópias de uma bolsa que perderam a disputa entre servidores amigos; com N, essa é devolvida (a bolsa que ela substitui é guardada de lado por sua vez) |

### Portais e servidores amigos

| Comando | Permissão | O que faz |
|---|---|---|
| `/portals` | portals | Aonde levam as aberturas de cada sala, os portais entre salas construídos no mundo e que boca cada um tem agora, e quais servidores mandam jogadores, quantos, de onde |
| `/friend add NOME HOST:PORTA CHAVE [o que]` | federation | Um servidor amigo; `o que` é qualquer um de `bans,presence,chat,bags,portals,mods`, todos por padrão; um NOME que já existe é substituído |
| `/friend remove NOME` `/friend list` | federation | Deixar de ser amigos (os banimentos dele são tirados aqui); os amigos, e quando cada um deu notícia pela última vez |
| `/fed TEXTO` | qualquer um | Chat com os jogadores de todos os servidores amigos |
| `/where NOME` | qualquer um | Aqui (e em qual sala), ou em qual servidor amigo |

### Páginas

| Comando | Permissão | O que faz |
|---|---|---|
| `/page [NOME]` | qualquer um | As páginas do jogo, ou uma delas |
| `/admin` | pages | A página de administração |

## Páginas no jogo

`/admin` abre uma página com quem está online e botões para kick, ban e mute; de lá, `bans`,
`whitelist`, `roles`, `perms`, `running` (reinícios, manutenção, mensagem do dia, backups),
`players`, `reports` e `log`. Todos têm `server` (as salas e quem está nelas), `mods`, `here` (onde
você está e de quem é o terreno), `rules` e as páginas que os mods adicionam. Uma página mostra só
o que quem a vê pode fazer, e cada botão roda o comando que digitá-lo rodaria.

As páginas são montadas pelo servidor em inglês. Um cliente que traduz um rótulo ou um botão
desenha a tradução menor quando ela é mais larga que o espaço que recebeu, até dois terços do
tamanho, e além disso as palavras em inglês, que cabem por construção.

## Opções e server.conf

`--config ARQUIVO` lê as opções de um arquivo no ponto em que ele está na linha de comando, então
as opções depois dele prevalecem. Uma linha `chave valor` ou `chave = valor` por opção, que quer
dizer `--chave valor`; `chave = true` é uma opção sem valor; `#` começa um comentário;
`[room NOME]` começa as opções de uma sala.

```
# server.conf
listen = 25300
world-file = worlds/main.world
owner = alice
rcon = 25399
rcon-password = change-me
motd = Welcome! /help lists the commands.
rules-file = rules.txt
restart-at = 04:00
backup-every = 6h
backup-keep = 8
max-per-address = 3
watchdog = 60
federation = 25480
webhook = https://example.org/hooks/server
```

| Opção | O que faz |
|---|---|
| `--owner NOME` | Quem joga como NOME com uma chave é o dono |
| `--rcon PORTA` `--rcon-password SENHA` | Comandos de administração por TCP em 127.0.0.1 |
| `--motd TEXTO` | A mensagem do dia |
| `--rules-file ARQUIVO` | As regras para aceitar, de um arquivo (várias linhas); veja abaixo |
| `--no-update-check` | Não perguntar ao GitHub se saiu uma versão mais nova |
| `--restart-at HH:MM` | Um reinício todo dia nessa hora local, com dez minutos de avisos |
| `--backup-every 6h` `--backup-keep N` | Backups com o servidor rodando; ficam os N mais novos (5) |
| `--block-history` | Anota todo bloco que um jogador muda, para `/inspect` e `/rollback` (desligado) |
| `--max-per-address N` | No máximo N jogadores de um mesmo endereço |
| `--watchdog S` | Uma sala travada por S segundos derruba o servidor com a pilha impressa |
| `--federation PORTA` | A porta TCP dos servidores amigos, sob a chave dos tickets (`--ticket-key`, ou WORLD.sign) |
| `--webhook URL` | Ações de administração, início, parada e denúncias enviadas por POST para lá |
| `--user-mods PASTA` (repetível) | Mods seus ao lado dos do jogo, carregados com eles como um só conjunto; numa `[room NOME]` só para essa sala, menos um mod com `rooms = all`, que todas as salas recebem ([MODDING](MODDING.pt.md#os-mods-do-próprio-jogador)) |
| `--no-mod NOME` (repetível) | Esse mod é encontrado e não é carregado; os blocos dele no mundo ficam magenta até ele voltar |
| `--max-players N` `--require-identity` `--no-observers` | Vagas; os jogadores precisam ter uma chave; nenhuma janela de portal de outros mundos |
| `--report ARQUIVO` `--since 7d` `--json` | Sem servidor: o que um arquivo de mundo lembra, impresso; `--json` como um objeto JSON |
| `--names` `--free-name NOME` | Sem servidor: quem é dono de cada nome; liberar um |
| `--map-tiles PASTA` | Sem servidor: o arquivo do mundo visto de cima, tiles PNG e `index.html` em PASTA, depois sai. Para um mundo ligado, use numa cópia do `/backup` |
| `--survival` | De uma sala (a do sandbox): sem praça, o spawn em terra firme escolhida pela semente, os mundos temáticos atrás de portais espalhados pela terra; `creative` começa em `nobody`. O "Modo: Sobrevivência" do menu de título escreve `survival` no arquivo `mode` do mundo e passa a opção; o `tools/track.sh --survival` também |
| `--no-plaza` | De uma sala: sem praça (`core.plaza()` é nil, então os mods não põem nada deles nela) e nada mais do `--survival`; o modelo das oficinas usa |
| `--difficulty peaceful\|easy\|normal\|hard` `--peaceful` | De uma sala (a do sandbox): essa dificuldade desde o início (`--peaceful` é pacífico), e guardada assim em WORLD.difficulty ao lado do arquivo do mundo, o nome dela numa linha (sem arquivo é normal) -- o que a sala é no próximo início sem a opção. `/difficulty` e o botão "Monstros" do menu de título (mundo novo, ou Editar de um mundo salvo) escrevem o mesmo arquivo. As regras (`/rule`) ficam ao lado, em WORLD.rules, uma `nome = valor` por linha |

As outras opções (mundo, mods, portais, salas) estão no [README](../README.pt.md#servidor-cnp_server).

## Mantê-lo rodando

```
tools/serve.sh -l logs -k 14 -w https://example.org/hooks/server \
    build-release/src/server/cnp_server --config server.conf
```

- Inicia o servidor de novo quando ele reinicia (código de saída 75: `/restart`, `--restart-at`)
  ou trava (um aborto do `--watchdog` incluído), cinco segundos depois de um travamento.
- Deixa o servidor parado depois de `/stop`, Ctrl+C ou SIGTERM (código de saída 0), e desiste
  depois de cinco travamentos em dez minutos.
- Escreve tudo o que o servidor imprime em `LOGDIR/server-AAAA-MM-DD.log`, um arquivo por dia, com
  a hora em cada linha; arquivos com mais de `-k` dias (14) são apagados.
- `-w URL` envia por POST cada travamento e a desistência, com os mesmos campos do `--webhook`.
- O stdin dele é o do servidor, então o console continua funcionando por baixo.

## Reinícios, paradas e manutenção

Um reinício ou uma parada faz uma contagem regressiva com um aviso em todas as telas, depois põe
todo mundo para fora com o motivo "restarting" ou "stopping" e salva. Um cliente posto para fora
por um reinício volta ao título e entra de novo sozinho no mesmo servidor assim que ele responde
(por três minutos). `/restart cancel` para a contagem.

A manutenção mantém fora todo mundo menos a equipe; quem está dentro e não é da equipe é posto
para fora com o texto dela. Ela sobrevive a um reinício, até `/maintenance off`.

## O que o silenciamento segura

O chat de um jogador silenciado é segurado, e também o que um mod diz aos outros enquanto roda um
comando, uma ação de página, um clique num painel ou uma mensagem dele. E as palavras que ele deu
a um mod ficam anotadas: uma linha de chat, um aviso ou uma linha de página de um mod que traz
três dessas palavras seguidas (doze letras ou mais, ou tudo o que ele disse se for menos) chega só
a ele, na hora ou depois -- uma carta entregue num tick seguinte, um bilhete preso num quadro. Uma
linha de página segurada diz `(held: NOME is muted)`. As palavras são anotadas enquanto o jogador
está silenciado e guardadas enquanto ele continua silenciado e por uma hora depois; o que ele
disse com menos de seis letras não. O log diz cada vez que uma linha é segurada.

O silenciamento também barra o que um jogador faz em direção aos outros: as barracas do mercado
(`trade`), sentar para uma nova partida de xadrez ou de cartas (uma partida já em andamento
continua), adicionar donos a um território, e a caixa de correio (`passport`). Um mod que adiciona
um jeito de alcançar outros jogadores pergunta `core.is_muted`.

**Bolsas entre servidores amigos.** Um jogador que continua jogando aqui enquanto o amigo que tem
a bolsa dele está inalcançável fica com duas cópias. Quando as duas se encontram, fica a que mais
se moveu entre servidores e a outra é guardada nas linhas `bagold`; `/oldbags NOME` lista essas
cópias e `/oldbags NOME N` devolve uma.

## Regras para aceitar

Com `--rules-file ARQUIVO` (ou `/rules set TEXTO`), cada jogador que entra vê a página `rules` com
o texto e dois botões, Aceitar e Sair. Até aceitar, ele não pode falar no chat, rodar comandos
além de `/rules`, `/accept`, `/leave` e `/help`, nem quebrar ou colocar blocos; a página volta cada
vez que ele tenta. A equipe conta como tendo aceitado. Definir regras diferentes pede a todos de
novo; o mesmo texto lido de novo num reinício não. `/rules off` tira as regras.

## Contas alternativas

`/alts NOME` lista os outros jogadores (chaves) que entraram de qualquer endereço de onde aquele
jogador entrou, e quais deles estão banidos. Quando um jogador entra de um endereço que um jogador
banido usou, a equipe online fica sabendo na hora (`[alts] bob came in from where eve did, who is
banned`). Um endereço não é uma pessoa: uma casa, uma escola ou uma rede de celular compartilham
um só.

## Verificação de atualizações

Um servidor que escuta pergunta ao GitHub uma vez, ao iniciar, se saiu uma versão mais nova que a
dele (a mesma pergunta que a tela de título do cliente faz). Quando saiu, o log diz, e cada admin
online e cada admin que entra fica sabendo no chat. `--no-update-check` desliga isso.

## Visitas, logs e denúncias

Cada visita é uma linha: quem, de qual endereço, em qual sala, de quando até quando, e por que
terminou. Um travamento deixa visitas abertas; o início seguinte as fecha e registra uma parada
suja. Toda ação de administração vai para o log de administração com quem fez, a quem, e os
detalhes.

**Um mod que um erro desliga** -- um handler, um step ou um sistema que falha para o seu mod para
todos naquele mundo -- é dito no chat de cada jogador ali que tem a permissão `logs` (os admins,
por padrão): `[mod] keys switched off: handler for 'keys:level': ...`. A mesma linha vai para o
stderr do servidor, e `/page mods` lista o mod como parado, com o motivo.

`cnp_server --report worlds/main.world --since 7d` imprime, sem parar o servidor: jogadores e o
tempo deles, o máximo ao mesmo tempo, entradas por hora, salas, quem foi recusado e por quê, ações
da equipe, reinícios e travamentos, e chegadas por portais de outros servidores. Com `--json` os
mesmos números vêm como um objeto JSON (`players`, `visits`, `seconds_played`, `most_at_once`,
`joins_by_hour`, `most_played`, `rooms`, `refused`, `staff_actions`, `server`, `crossings_in`),
para um programa ler.

## Blocos: inspeção e rollback

Só com `--block-history` (`block-history = true` no server.conf): desligado por padrão, e então
nada é anotado e `/inspect` e `/rollback` só dizem isso. Com a flag, todo bloco que um jogador
quebra ou coloca fica anotado: a sala, a célula, o que havia, o que virou, quem e quando. As anotações vão juntas para o arquivo a cada cinco segundos; um mês fica
guardado.

`/rollback NOME 2h` põe cada célula que esse jogador mudou nas últimas duas horas de volta como
estava antes da primeira mudança dele nesse tempo, a não ser que alguém a tenha mudado depois
dele: essa mudança fica. Essas linhas então somem, então um segundo rollback não faz nada. O que o
jogador ganhou com os blocos fica na bolsa dele; para isso existem o `/bag` e os comandos de itens.

## Backups

`--backup-every 6h` e `/backup` copiam cada arquivo de mundo ao lado dele como
`NOME.backup-AAAAMMDD-HHMMSS`, com o backup online do SQLite numa thread própria, com o servidor
rodando. Ficam os `--backup-keep` mais novos.

Para voltar a um: pare o servidor, guarde de lado o arquivo atual, copie o backup por cima do
arquivo de mundo (os arquivos `.key`, `.sign` e `.lock` ao lado ficam como estão), e inicie de
novo.

## Webhooks

Com `--webhook URL`, cada ação de administração, cada `/report`, e o início do servidor (`unclean
start` depois de um travamento) e a parada são enviados por POST como JSON:

```json
{"event":"ban","by":"alice","target":"bob","detail":"until 2026-10-28 18:00, griefing",
 "at":1793120997,"content":"[ban] alice bob: until 2026-10-28 18:00, griefing",
 "text":"[ban] alice bob: until 2026-10-28 18:00, griefing"}
```

`content` e `text` são a mesma frase, que os webhooks de entrada dos serviços de chat mostram como
está. O envio passa pelo `curl`, cinco segundos no máximo, numa thread própria: um webhook fora do
ar não custa nada. A URL precisa ser `http://` ou `https://` com caracteres simples.

## Portais entre servidores

`--portal-to HOST:PORTA[/N]` diz aonde leva uma abertura controlada pelo servidor; `--trust HEX`
aceita os tickets que outro servidor assina, `--require-ticket` não aceita mais ninguém. Um
cliente que atravessa confere se a chave do servidor distante é a que a ligação nomeia, tanto por
uma janela de portal quanto pelo redirecionamento lento. `/portals` mostra as duas direções.

**Entre as salas de um servidor** o ticket é sempre necessário, com ou sem `--require-ticket`: numa
sala que não é a primeira do servidor só se entra direto para olhar dentro dela (a janela de um
portal), e só se entra de verdade com um ticket que a sala onde o jogador está assina para aquela
sala pelo nome -- um ticket para uma sala não vale para outra, mesmo que todas tenham a mesma
chave. Na primeira sala se entra como sempre. Um cliente mais antigo que o protocolo 127 não pede
esse ticket e não entra.

### Portais entre salas, construídos no mundo

Num servidor com várias salas, uma moldura de `portal:frame` (oito molduras em volta de um lingote
de ferro) pode levar a uma moldura em outra sala, configurada no jogo:

1. Construa uma moldura em pé em cada uma das duas salas, do mesmo tamanho (de 1 a 8 de largura,
   de 2 a 8 de altura). As salas que carregam `mods/portal` têm molduras: o sandbox, o Inferno, os
   salões de bolso.
2. Com a permissão `portals` (admin), clique direito em uma: **outras salas: sim** a anuncia às
   outras salas. Faça o mesmo na outra.
3. O menu de qualquer uma das duas agora lista as molduras das outras salas; um clique numa liga as
   duas, um clique naquela a que ela leva desfaz a ligação. Uma moldura de outro tamanho diz por
   que não pode ser ligada.

Um portal assim ocupa uma das 28 bocas da sua sala só enquanto um jogador está a menos de 160
blocos de uma das pontas: o servidor dá as bocas que tem sobrando aos portais ligados mais perto
de um jogador e as tira dos outros, então o buraco fica vazio quando não há ninguém por perto e um
mundo pode ter muito mais molduras que bocas. Sobram as bocas além das do próprio mundo
(`--portal-to`) e as do próprio mundo que um mod estacionou (`core.portal_park`): num sandbox com
praça, as sete segundas entradas e as bocas das portas de bolso que não estão numa porta.
`/portals` lista cada um e a boca que tem agora; o log diz `mouths: N given to NAME` e
`given back`.

### Salas iniciadas com o servidor rodando

`--rooms-file ARQUIVO` nomeia salas com que o servidor não começa. O arquivo é um `server.conf`
com uma linha `[room NOME]` antes das opções de cada sala, e precisa de um servidor que já tenha
salas (pelo menos dois `--room`):

```
[room caverna]
world-file worlds/caverna.world
seed 7
mods games/inferno/mods
```

Uma sala assim recebe o que vem antes do primeiro `--room` na linha de comando, como as outras, e
depois as suas próprias opções. Ela **inicia quando algo leva até ela**: um jogador chega perto de
um portal ligado a um dos portais dela, um jogador a pede pelo nome ao entrar, ou
`/room start NOME`. Enquanto ela inicia, as outras salas continuam rodando: ela lhes dá a vez
depois de cada um dos seus mods e de cada coluna que os seus mods geram, então elas ficam paradas
no máximo cerca de um quarto de segundo. Ela **salva e para** um minuto depois que o último jogador
saiu dela e ninguém está perto de um portal que leva a ela, ou com `/room stop NOME` quando está
vazia; um portal que levava a ela mostra "espera por …" e a inicia de novo quando alguém chega
perto. As salas com que o servidor começou nunca param antes do servidor.

`/room add NOME [OPÇÃO [VALOR]...]` escreve uma sala no arquivo com o servidor rodando (opções com
ou sem o `--`), `/room remove NOME` tira uma sala parada e deixa o arquivo de mundo dela onde está,
`/room list` diz quais estão ligadas. Todo o `/room` é do dono (permissão `rooms`). Um servidor
comporta 64 salas ao mesmo tempo: quando todos os lugares estão ocupados, uma sala parada cede o
seu lugar para a que está iniciando, e inicia de novo do seu arquivo de mundo quando for
necessária. Duas salas não podem rodar no mesmo arquivo de mundo.

**Um modelo** é uma seção cujo nome termina com um asterisco:

```
[room home-*]
world-file worlds/{name}.world
survival
```

Toda sala cujo nome começa assim (`home-anna`, `home-7`) é feita a partir dele na primeira vez que
algo leva até ela, com `{name}` nas opções no lugar do nome da sala. Sem `world-file` o mundo dela
é `NOME.world` ao lado do arquivo de salas, e sem `seed` a semente sai do nome, então cada sala
assim é um mundo diferente e o mesmo a cada início. Um portal ligado a uma sala assim a cria; um
jogador que só a pede pelo nome ao entrar a recebe quando o arquivo de mundo dela já existe.
`/room add home-* ...` e `/room remove home-*` escrevem e tiram um modelo; as salas feitas a partir
dele não aparecem na lista das molduras das outras salas.

**A sala própria de cada jogador.** O modelo `home-*` é o que `mods/portal` e `mods/home` usam:
quem construiu uma moldura em pé, de um de largura e dois de altura, encontra "minha sala" no menu
dela, e a moldura passa a levar à porta de uma sala só dele, chamada `home-` mais o começo da sua
chave. Não precisa de permissão; dizer isso de outra moldura muda a entrada para lá. O jogo escreve
esse modelo sozinho para um mundo iniciado pelo menu (`rooms.conf` na pasta do mundo: um mundo como
o sandbox, com os mesmos mods), e o `tools/track.sh` faz o mesmo. Um servidor sem o modelo não dá
essas salas, e o botão diz isso.

**Quem pode entrar** decide o dono, de qualquer sala, com `/myroom`: uma **blacklist** (todos
menos os nomes da lista; é o padrão, vazia) ou uma **whitelist** (só os nomes da lista), e se os
jogadores deixados de fora ainda podem **olhar dentro** pela moldura (`/myroom look yes`) ou veem
uma membrana vermelha (`look no`). Quem pode olhar e não pode entrar vê a sala na moldura e passa
pelo buraco como por uma moldura vazia. A regra fica no armazenamento compartilhado do servidor,
que a aplica em cada moldura que leva à sala e na própria sala (um cliente que pede a sala pelo
nome ao entrar também é recusado); o dono, e quem tem a permissão `portals`, sempre entram. Alguém
lá dentro que não é mais aceito é mandado para o spawn.

**A saída** de uma sala própria cuja moldura foi quebrada, ou mudada de lugar, enquanto alguém
estava dentro: `/myroom leave`, dito na sala, manda quem diz para o spawn da primeira sala do
servidor (o sandbox), como por um portal; a sala avisa quem está dentro quando a entrada dela some.

**A oficina de cada jogador.** O modelo `workshop-*` é o de `mods/workshop`: `/workshop`, dito por
qualquer um com uma chave em qualquer sala, inicia a sala `workshop-` mais o começo da sua chave e
leva a pessoa até lá (core.room_send inicia uma sala do arquivo que está parada). Só o dono entra.
Ela é plana, pacífica, sem fome nem tempo: é onde as zonas dos níveis são construídas, testadas e
propostas ([MODDING](MODDING.pt.md#níveis-feitos-de-peças-salvas-modszones)). `/workshop leave`
volta ao spawn da primeira sala; o menu de qualquer moldura de portal também tem "minha oficina"
(ou "sair da oficina"). Quem configura os portais do servidor (`portals`, o dono) pode entrar na
oficina de qualquer um. Os mundos da tela de título e o `tools/track.sh` escrevem o modelo ao lado
do `home-*`, com os mesmos mods, nunca sobrevivência e sem praça (`no-plaza`: senão os mods
poriam as mesas e caixas deles no piso), e com os blocos dos mundos de que as zonas são feitas
(`games/workshop`):

```
[room workshop-*]
world-file worlds/{name}.world
no-plaza
user-mods games/workshop/mods
textures games/workshop/textures
fallback-textures assets/textures
```

As zonas propostas esperam um admin em `/page zones`.

### Portais construídos no mundo

Entre [servidores amigos](#servidores-amigos) que compartilham **portals**, um portal pode ser
construído e configurado no jogo em vez de na linha de comando:

1. Construa um anel de **portal:server_frame** (ouro; oito `portal:frame` em volta de um rubi) em
   volta de um buraco em pé, de 1 a 8 de largura e de 2 a 8 de altura. Qualquer um pode
   construí-lo; ele não leva a lugar nenhum até um admin configurá-lo.
2. O clique direito de um admin na moldura abre o menu dela: o nome (o campo no alto; o servidor
   distante conhece o portal por ele, então ele só muda enquanto está desligado), **disponível
   para**, um botão por servidor amigo que compartilha portais (`[x]` quer dizer que está
   oferecido a esse servidor), e os portais que esses servidores oferecem a este, cada um um botão
   que se conecta a ele.
3. No outro servidor um admin faz o mesmo: oferece o seu portal a este servidor. Quando os dois
   estão oferecidos, um clique no portal distante em qualquer um dos menus liga os dois; o outro
   servidor aceita a ligação sozinho em poucos segundos. O clique direito de qualquer outra pessoa
   só diz aonde o portal leva.

Antes de pedir uma ligação, e de novo no servidor distante antes de aceitá-la: o amigo está na
lista, compartilha portais e deu notícia nos últimos 90 s; o portal está oferecido a ele e o dele
a este servidor; o portal distante não está ligado a nenhum outro; os dois servidores falam um
protocolo em comum (a faixa do distante vem em cada oferta); os dois buracos têm o mesmo tamanho.
Uma ligação não aceita em um minuto se desfaz, com o motivo no menu; um servidor que recusa uma diz
por quê, e o menu de quem pediu mostra. Depois o cliente confere o que sempre confere quando
atravessa: a chave do servidor distante contra a chave do amigo, o seu próprio protocolo no
handshake, e o ticket que este servidor assina para o jogador.

Um portal ligado é uma das bocas da sala (28, em comum com `--portal-to`): uma abertura para o
outro servidor, atravessada com um ticket, e enquanto está ligado a sala aceita os tickets desse
amigo.

**Uma ligação feita dura**: ela fica no arquivo de mundo (linha `farportals`) com as chaves de
assinatura dos dois servidores, e continua valendo enquanto um dos servidores, ou os dois, estão
desligados ou inalcançáveis -- a boca fica, no endereço e na porta que o amigo tinha, e se abre de
novo para ele quando ele volta. Ela só se desfaz quando:

- um admin a desliga, quebra a moldura, para de oferecê-la a esse amigo ou muda o tamanho dela, de
  qualquer um dos lados: o outro fica sabendo na hora, ou, se estava desligado, na primeira vez
  que disser que ainda está ligado;
- a chave de assinatura de um dos servidores (WORLD.sign ou `--ticket-key`) não é aquela com que a
  ligação foi feita: um servidor cuja chave mudou larga as suas ligações ao iniciar, e o outro as
  larga assim que chega uma linha da chave nova -- que ele aceita quando recebe de novo
  `/friend add` com o mesmo nome e a chave nova (a chave antiga é então esquecida);
- o amigo é removido (`/friend remove`).

#### Dispensar uma verificação

Três caixas no menu fazem um portal dispensar uma verificação cada, para pares mais fáceis de
fazer e de manter. Cada uma dá um aviso quando é marcada, e o menu o mantém embaixo das caixas:

| Caixa | Dispensa | O risco |
|---|---|---|
| não precisar do outro servidor online | o amigo ter dado notícia nos últimos 90 s | uma ligação pedida enquanto ele está desligado espera por ele para sempre, em vez de se desfazer depois de um minuto; os portais distantes listados são os últimos que ele informou |
| não precisar de um protocolo em comum | algum protocolo falado pelos dois servidores | um jogador cujo cliente não fala o protocolo do servidor distante é recusado na travessia |
| não precisar de buracos do mesmo tamanho | os dois buracos do mesmo tamanho | um corpo vindo de um buraco menor pode sair dentro da moldura e ficar preso |

Cada servidor confere pelas caixas do seu próprio portal: para buracos de dois tamanhos, os dois
portais precisam da caixa. `/portals` lista as molduras, a quem cada uma está oferecida e com o
que está ligada, e as verificações que ela dispensa.

O servidor distante é alcançado no host que o `/friend add` dele nomeia, com a porta UDP que ele
diz nas suas ofertas: adicione um amigo pelo endereço com que os jogadores o alcançam.

## Servidores amigos

Dois servidores que se adicionam um ao outro compartilham, pela porta `--federation`:

- **bans**: os banimentos permanentes por chave. O banimento de um amigo vale aqui até `/unban`
  tirá-lo (e então fica tirado) ou o amigo ser removido.
- **presence**: quem está online, para o `/where`.
- **chat**: `/fed`.
- **bags**: uma bolsa só para um jogador em todos eles (abaixo).
- **portals**: os portais construídos no mundo que cada um oferece ao outro ([acima](#portais-construídos-no-mundo)).
  Um amigo adicionado antes de essa partilha existir (0.11.0-alpha.1 e anteriores) é adicionado de novo (`/friend add` com o mesmo nome) para compartilhá-los.
- **mods**: o que um mod diz ao mesmo mod no outro (`core.fed_send`): os pacotes e as cartas de
  `mods/passport`. Um amigo adicionado antes de essa partilha existir é adicionado de novo para
  compartilhá-la.

Para tornar dois servidores amigos:

1. Inicie cada um com `--federation PORTA`; o log diz `this server's key HEX`, e `/friend list`
   repete.
2. No primeiro: `/friend add second second.example.org:25480 HEX_DO_SEGUNDO`.
3. No segundo: `/friend add first first.example.org:25480 HEX_DO_PRIMEIRO`.

Cada linha vai numa conexão TCP própria. Quem recebe fala primeiro, com uma chave feita só para
aquela conexão; quem envia responde com uma sua e a linha, selada para quem recebe
(XChaCha20-Poly1305, com uma chave feita das chaves de longo prazo dos dois servidores e das duas
descartáveis) e assinada com a chave de tickets de quem envia. As chaves descartáveis são
esquecidas na hora, então uma linha gravada hoje não pode ser aberta amanhã nem com as chaves dos
dois servidores: forward secrecy. Uma linha mais antiga que a última de quem envia, com cinco
minutos de diferença deste relógio, ou de uma chave que não está na lista, é descartada.

### Uma bolsa só

A bolsa de um jogador vive em um servidor amigo por vez e vai aonde ele vai:

- **Por um portal** vindo de um amigo: o servidor aonde ele chega pede ao de onde ele vem, que
  larga a bolsa (o jogador lá agora é uma janela) e a envia.
- **Entrando** pelo menu: o servidor pergunta a cada amigo de quem é a cópia mais nova. A mais
  nova, se for mais nova que a sua, é enviada e largada. Mais nova é a cópia que mais vezes passou
  entre amigos, o que não precisa de relógio (só quem a segura recebe uma contagem maior); duas
  cópias que nunca se moveram, de antes de os servidores serem amigos, são distinguidas por quando
  cada uma foi guardada.
- Quem enviou lembra para onde ela foi: uma visita seguinte ali pergunta primeiro àquele servidor,
  e `/bag` ali diz isso.
- Enquanto ela chega (poucos milissegundos, no máximo cinco segundos) o jogador fica com uma bolsa
  vazia, e nada é salvo por cima da que está no disco. Um amigo que não responde a tempo deixa o
  jogador com a bolsa guardada aqui; se a bolsa do amigo chegar depois mesmo assim, ela toma o
  lugar dessa cópia antiga, e o que o jogador pegou nesse meio-tempo entra nela.
- **As mochilas** (`mods/backpack`) vão com o conteúdo. O conteúdo de uma mochila é guardado sob o
  número dela, que o mod de cada servidor conta a partir de 1, então o servidor aonde uma mochila
  chega lhe dá um número seu de 100000 para cima (mantido quando ela volta a um servidor onde
  aquele número está livre).
- **As coisas que um mundo não conhece** (um item de um mod que ele não tem) ficam escondidas:
  guardadas de lado, fora da bolsa, e enviadas com ela, uma mochila escondida com o seu conteúdo;
  elas voltam num servidor que as conhece, na casa que tinham (uma mochila nas costas) ou numa
  livre. `/bag` diz quando um jogador tem algumas.

## O que é guardado, e onde

No arquivo de mundo da primeira sala (um armazenamento para todas as salas), ao lado do mundo:

| Tabela | O quê |
|---|---|
| `admin` | Uma linha por coisa (`farportals`: os portais entre servidores e as ligações deles): `role`, `ban`, `mute`, `allow` (whitelist), `setting` (whitelist ligada, manutenção, mensagem do dia, regras), `accepted` (quais regras um jogador aceitou), `perm`, `addr` (último endereço), `friend`, `fedskip` (o banimento de um amigo tirado aqui), `bagaway` (a bolsa de quem foi para qual amigo), `baggen` (quantas vezes uma bolsa se moveu), `hidden` e `packs` (o que um jogador carrega que este mundo não conhece, e o que as mochilas dele guardam) |
| `admin_log` | Toda ação de administração, início, parada, parada suja, denúncia, nota, advertência |
| `sessions` | Uma linha por visita |
| `edits` | Todo bloco que um jogador mudou, por um mês (só com `--block-history`) |

As bolsas mesmo ficam no arquivo de mundo de cada sala, como sempre. Ao lado do arquivo de mundo
de cada sala, dois pequenos arquivos de texto: WORLD.difficulty (o nome dela, nenhum para normal)
e WORLD.rules (uma `nome = valor` por linha), os dois escritos por `/difficulty`, `/rule` e pelo
menu de título, lidos ao iniciar; e WORLD.shape (`profundidade topo`, de `--world-depth` e
`--world-top` de quando o mundo foi criado): mova-o junto com o mundo.

## Limites

- `/mute` reconhece as palavras de um jogador silenciado por três seguidas, sejam quais forem as
  maiúsculas, a pontuação ou as aspas em volta: um mod que as reescreve, ou uma linha com menos de
  seis letras, passa; outro jogador que repete a frase dele dentro da hora também é segurado. Um
  mod que repassa palavras pergunta `core.is_muted` por conta própria, como fazem `/msg` e `/me`.
- Um amigo que continua inalcançável enquanto um jogador segue jogando com a cópia guardada aqui
  também mantém a sua; vence a que se moveu mais vezes na próxima vez que as duas forem
  comparadas.
- A verificação do cliente de que a chave do servidor distante é a que um portal nomeia fica atrás
  do ticket: um servidor que não é o nomeado recusa o ticket antes que o cliente leia a chave dele,
  então a verificação só importa para um servidor que mentiria também sobre os tickets.
