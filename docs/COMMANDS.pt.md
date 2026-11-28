# Comandos

*[Read in English](COMMANDS.md) · [Leggi in italiano](COMMANDS.it.md)*

Todo comando que um jogador pode digitar no chat (T, depois `/nome`), o que é e o que faz.
`/help` no jogo lista os que você pode usar onde está; a lista muda com o mundo, porque a
maioria dos comandos vem dos mods dele.

O console do servidor também aceita os comandos de administração, com ou sem a `/`, como
dono. As durações são `45s`, `30m`, `2h`, `7d`, `1w`. NOME é um jogador online em qualquer
sala, o dono desse nome neste mundo, ou só o nome.

**Permissão** é quem pode usar. "Qualquer um" não precisa de nada; os outros dizem uma
permissão e o cargo que a tem por padrão (helper < moderator < admin < owner, cada um com
tudo o que está abaixo). `/perm` mostra e muda qual cargo uma permissão pede;
[`ADMIN.pt.md`](ADMIN.pt.md) explica os cargos.

## Para todos

### Chat e informações

| Comando | Permissão | O que faz |
|---|---|---|
| `/help` | qualquer um | Os comandos que você pode usar neste mundo, um por linha |
| `/list` | qualquer um | Quem está neste mundo |
| `/me <ação>` | qualquer um | Diz `* nome ação` para todos (`mods/whisper`) |
| `/msg <jogador> <texto>` | qualquer um | Manda o texto só para esse jogador (`mods/whisper`) |
| `/seen NOME` | qualquer um | Quando esse jogador esteve aqui pela última vez, e quanto jogou no total |
| `/where NOME` | qualquer um | Se esse jogador está aqui e em qual sala, ou em qual servidor amigo |
| `/fed TEXTO` | qualquer um | Chat com os jogadores de todos os servidores amigos |
| `/report NOME o que aconteceu` | qualquer um | Uma denúncia para a equipe, avisada na hora a quem está online |
| `/perms` | qualquer um | O seu cargo aqui |
| `/rules` | qualquer um | As regras do servidor |
| `/accept` | qualquer um | Aceita as regras do servidor, que um servidor que as tem pede a todos |
| `/leave` | qualquer um | Sai do servidor sem aceitar as regras |
| `/page [NOME]` | qualquer um | As páginas do jogo: sem nome a lista (`server`, `mods`, `here`, `rules` e as dos mods), com um nome essa página |

### Jogar

| Comando | Permissão | O que faz |
|---|---|---|
| `/kill` | qualquer um | Você morre e volta à sua cama ou ao spawn do mundo; a armadura se gasta um pouco, como num golpe qualquer. Para um corpo preso em algum lugar. Funciona também no modo criativo (`mods/creative`) |
| `/giveup` | qualquer um, num mundo com uma prova | Desiste da prova desse mundo: você morre, depois é levado para fora pela entrada do mundo e volta ao sandbox, do lado de fora do portão desse mundo. Um corpo que não pode ser ferido só é levado para fora. O que a prova já contou (uma chave ganha) fica (`mods/keys`) |
| `/journal` | qualquer um | Abre as páginas do engenheiro das chaves que você carrega, uma página por chave e uma última quando você tem as sete (`mods/keys`) |
| `/town [town\|city]` | qualquer um | Leva você à praça da vila mais próxima; com qualquer palavra depois, da cidadezinha mais próxima (uma vila maior, procurada ao dobro da distância); com `city`, da cidade mais próxima (`mods/villages`) |
| `/creative TEXTO` | creative (moderator) | Abre o catálogo criativo filtrado para os itens cujo nome tem o texto (`mods/creative`) |
| `/zone [save\|set\|delete\|propose\|approve\|refuse\|try\|show] ...` | qualquer um; `approve`, `refuse`, `try` e `show` admin (`try` e `show` qualquer um na própria oficina) | Pedaços de níveis (`mods/zones`). Sozinho, lista as suas zonas e as aprovadas. `save MUNDO NOME X1 Y1 Z1 X2 Y2 Z2 [start\|room\|end]` salva esse volume como a zona MUNDO.NOME, sua: blocos de passagem nos lados viram as passagens dela, blocos de marcador os marcadores. `set ID d MIN MAX \| weight N \| mirror on\|off \| role ...` diz para que ela serve, `delete ID` a remove. O `approve ID` de um admin a coloca nos níveis de todos e `refuse ID` (ou `private`) a retira; `propose ID` a pede aos admins (`/page zones`); `try MUNDO [QUANTAS [SEMENTE [D]]]` monta ao seu lado um nível com as zonas desse mundo, `show ID` só essa zona |
| `/workshop [leave\|blocks]` | qualquer um com uma chave | Para a sua oficina (`mods/workshop`), uma sala plana onde as zonas são construídas com uma varinha, um pincel e um painel, testadas e propostas; `leave` sai para o spawn da primeira sala; `blocks` abre a paleta do pincel |
| `/myroom [whitelist\|blacklist\|add NOME\|remove NOME\|look yes\|no]` | qualquer um com uma chave | Quem pode entrar na sua sala própria (`mods/home`): só os nomes da lista (whitelist) ou todos menos eles (blacklist, o padrão), e se os outros podem olhar para dentro pela sua moldura. Sozinho, diz a regra |
| `/myroom leave` | qualquer um, na sala própria de um jogador | Para fora da sala, ao spawn da primeira sala do servidor: a saída quando a moldura dela não existe mais |

### Num mundo só

| Comando | Mundo | Permissão | O que faz |
|---|---|---|---|
| `/garage` | pista | qualquer um | Abre a garagem: as cores e as peças do seu kart; um clique muda na hora, também na pista, e fica salvo |
| `/ghost [record\|mine\|NOME\|off]` | pista | qualquer um | Qual kart fantasma guia as suas voltas: o recorde da pista (o padrão; o do engenheiro na pista do Inferno), a sua melhor volta, a melhor volta de outro jogador, ou nenhum. Fica salvo |
| `/laps` | pista | qualquer um | A melhor volta de todos na última pista que você guiou: os dez primeiros e o seu lugar, e quem tem fantasma |
| `/pieces` | pista | qualquer um | Coloca na sua bolsa uma pilha de cada peça de pista, o editor de pista (B com ele na mão escolhe de longe a próxima peça), e as ferramentas de linha de largada, checkpoint, fora da pista, caixa de itens e portal |
| `/manta` | mundo das nuvens | qualquer um | Quantas vezes você derrotou a Arraia das Nuvens |
| `/vehicle <jeep\|tank\|heli\|plane\|bomber\|nest\|mortar\|howitzer\|aa>` | front, assalto | moderator | Coloca essa máquina seis blocos à sua frente, para testes |
| `/arpg <town\|butcher\|grave\|emberwing\|crypt\|floor1\|floor2\|keeper\|level N\|gold [N]>` | arpg | admin | Atalhos de teste: à cidade, ao chefe de uma área, à cripta ou a um dos seus pisos; o seu nível até N (escolhida antes uma classe); N moedas (1000) |
| `/backrooms <pink\|hotel\|party\|pool\|slide\|portal\|exit\|suburb\|shift\|kit>` | backrooms | admin | Atalhos de teste: à porta rosa, ao bairro hotel ou festa mais próximo, ao piso da piscina, a um escorrega, a um local de portais, à saída, a uma rua do subúrbio sem fim (mundo de altura 96); `shift` muda já as ligações dos portais; `kit` dá as duas bússolas, a tocha e os materiais |
| `/chamber <0..18>` | câmaras de teste | admin | Para essa câmara, com a arma de que precisa (azul a partir da 03, as duas a partir da 04) |
| `/defense [kit\|turrets\|wave N]` | tower defense | qualquer um (recordes), admin (o resto) | Sem palavra, os recordes: a melhor onda do campo e de quem era, e a melhor de cada jogador aqui. `kit` os materiais de construção, `turrets` duas de cada torre e barricadas, `wave N` limpa o campo e começa a onda N |
| `/inferno <hall\|brazier 1..4\|light>` | inferno | admin | Para a sala do coração ou um braseiro; `light` acende os quatro e abre o portão |
| `/labphase <shield\|siege\|climb\|pit\|fire\|dark\|frost\|race>` | laboratório | admin | A luta com l'Ospite a partir dessa fase, a sala preparada (o percurso da subida, a lava, as lâmpadas apagadas, o kart para a corrida) e você no lugar; `pit` é a subida a partir do parkour do fosso |
| `/parkour <1..18\|tower 1..7\|summit>` | parkour | admin | Para essa plataforma (do nível em que está, em sobrevivência), uma estação da torre ou o cume, contada como alcançada |
| `/plat <room\|1-1\|1-2\|1-3\|bonus\|boss\|big\|fire>` | jogo de plataformas | admin | Para fora na sala, para dentro de um nível, ao cano bónus do 1-1 ou à sala do chefe do 1-3; `big` e `fire` dão esse poder num nível |
| `/shmup <wave N\|power\|drops>` | jogo de tiro | admin | `wave N` começa já essa onda com 30 vidas (ou na próxima descolagem); `power` enche os potenciadores do caça; `drops` faz cada abate soltar um, ou não |
| `/skies <manta\|village\|updraft\|phase2\|phase3\|kit\|reset>` | mundo das nuvens | admin | Para a ilha da Manta, a aldeia ou corrente mais próxima; a fase baixa da Manta ou a sua queda já; `kit` os planadores, cristais e armas; `reset` a luta do início |
| `/track <start\|ring\|grand\|inferno\|loops\|portal\|power KIND\|kart>` | pista | admin | Para a linha de partida dessa pista (na sua sala); `power KIND` (boost, boost3, banana, shell, homing, ink, star, bolt) no seu kart; `kart` um kart no saco |

## Para a equipe

### Mundo e tempo

| Comando | Permissão | O que faz |
|---|---|---|
| `/kill NOME` | moderator | Mata outro jogador, que volta à cama dele ou ao spawn (`mods/creative`) |
| `/creative on\|off` | admin | O modo criativo para você: voar sem o jetpack, nenhum dano, cavar dez vezes mais rápido; guardado pelo nome entre reinícios. Livre para o dono de um mundo iniciado pelo menu de título (`mods/creative`) |
| `/give NOME ITEM [QUANTIDADE]` | give (admin) | Coloca itens, pelo nome (`chest:box`), na bolsa de um jogador online |
| `/weather [clear\|rain\|storm] [MINUTOS]` | admin | O céu do sandbox: sem palavra, como está e quanto dura; com uma, esse céu agora, por MINUTOS ou pelo tempo de sempre (`mods/weather`) |
| `/time [day\|noon\|dusk\|night\|midnight\|dawn\|HH:MM]` | time (admin) | O relógio do mundo em que você está, em todo mundo: sem palavra, a hora; com uma, essa hora imediatamente para todos ali. No sandbox, voltar o relógio para antes da meia-noite conta um dia, como uma noite dormida |
| `/story NOME [keys all\|MUNDO...\|ending\|clear]` | admin | O progresso de um jogador na história, com o jogador no seu mundo: sem palavra, quantas chaves e se o Hóspede foi derrotado. `keys all` ou `keys pista inferno ...` registra essas chaves e as coloca na bolsa (mundos: pista inferno arena arpg chambers backrooms parkour); `ending` são as sete e o Hóspede derrotado, o que abre o front e o assalto e dá as recompensas; `clear` tira tudo (`mods/keys`) |
| `/season [spring\|summer\|autumn\|winter]` | admin | A estação do sandbox (cinco dias cada): sem palavra, qual é e que dia dela; com uma, o primeiro dia dessa estação (`mods/weather`) |
| `/difficulty [peaceful\|easy\|normal\|hard]` | admin | Quão difícil é o mundo, na hora e entre reinícios (WORLD.difficulty): pacífico, nenhuma criatura que luta aparece ou fica -- o gerador de uma masmorra deixa o que as criaturas dele deixariam, o altar das profundezas dá o coração e os fragmentos sem luta, as vilas não têm cercos; fácil corta pela metade o golpe de uma criatura e quantas aparecem, e a fome; difícil aumenta os dois pela metade. Sem palavra, qual é (`mods/rules`) |
| `/peaceful [on\|off]` | admin | `/difficulty peaceful`, ou normal de novo (`mods/rules`) |
| `/rule [NOME on\|off]` | admin | As regras do mundo, entre reinícios (WORLD.rules): `keep_bag` (num mundo de sobrevivência a bolsa fica com você na morte, sem túmulo), `day_cycle` (o sol se move), `weather_cycle` (o tempo muda sozinho), `hunger` (fome). Sem palavra, a lista (`mods/rules`) |
| `/goals` | qualquer um | Quantos objetivos do sandbox você alcançou; `/page goals` lista todos com dicas para os próximos (`mods/goals`) |
| `/trophies` | qualquer um | Quantos troféus você ganhou; `/page trophies` lista todos, com dicas e para que lado ficam os portais escondidos (`mods/trophies`) |
| `/trophies sites` `/trophies go ID [place] [NOME]` `/trophies home [NOME]` | admin | Onde estão os cinco portais escondidos e se cada um está construído; você ou NOME na frente de um (`well`, `summit`, `lake`, `hill`, `circle`) ou, com `place`, no lugar para onde leva; NOME (ou você) no próprio quarto, para ver a estante de troféus (`mods/trophies`) |
| `/village siege\|feast\|caravan\|incoming` | admin | Começa agora um evento na vila em que você está: um cerco avisado agora e que chega ao anoitecer, a festa da colheita, uma caravana para a outra vila mais próxima, ou uma que chega de uma vila não visitada pelos caminhos (`mods/villagers`) |
| `/go [ROOM]` | go (admin) | Sem palavra, as salas deste servidor e se cada uma está ligada ou arranca a pedido; com uma, você no spawn dessa sala (uma a pedido é iniciada). Depois `/tp` para um ponto dentro dela |
| `/tp X Y Z` `/tp X Z` `/tp NAME` | tp (admin) | Você nesse ponto da sala em que está; só com X Z, no chão ali (o piso mais alto com dois blocos livres acima, ou a sua altura numa coluna não carregada); com um nome, até esse jogador, ou ao spawn da sala em que ele está |
| `/cursedghost` | admin | Um fantasma amaldiçoado quatro blocos à sua frente, para testar a pegada (`mods/ghost`) |

### Jogadores

| Comando | Permissão | O que faz |
|---|---|---|
| `/role [NOME [CARGO [SALA]]]` | roles (admin) | Lista os cargos dados; mostra o de um jogador; dá um (numa sala com SALA). `player` o tira |
| `/kick NOME [motivo]` | kick (moderator) | Põe o jogador para fora de todas as salas, janelas de portal também; o cliente dele volta ao título dizendo por quê |
| `/ban NOME [tempo] [motivo]` | ban (moderator) | Fora e mantido fora, por um tempo ou para sempre. Um nome com dono bane a chave do dono; um nome sem dono bane também o endereço de onde veio por último |
| `/banip NOME\|ENDEREÇO [tempo] [motivo]` | ban.ip (admin) | Mantém um endereço fora |
| `/unban NOME\|ENDEREÇO` | ban (moderator) | Tira um banimento; tirar aqui o banimento de um servidor amigo o mantém tirado |
| `/bans` | ban (moderator) | Quem está banido, por quem, até quando |
| `/mute NOME [tempo] [motivo]` | mute (helper) | Segura o chat do jogador, e as palavras dele onde quer que um mod as repasse. Um nome que ninguém tem ainda é silenciado pelo nome |
| `/unmute NOME` | mute (helper) | Deixa o jogador falar de novo |
| `/warn NOME motivo` | warn (helper) | Uma advertência; três em 30 dias são um dia de banimento |
| `/note NOME texto` | notes (helper) | Uma nota sobre o jogador, mostrada no `/whois` |
| `/whois NOME` | whois (moderator) | Chave, nomes usados, endereços, advertências, notas, banimentos |
| `/alts NOME` | alts (moderator) | Outros jogadores que entraram de um endereço que este usou, e quais estão banidos |
| `/players [7d]` | players (helper) | Quem entrou nos últimos tempos, visitas e tempo jogado |
| `/bag NOME` | bag (moderator) | O que o jogador carrega, casa por casa, e as mochilas dele; offline, a bolsa guardada aqui |
| `/reports` | reports (helper) | As últimas denúncias |
| `/sc TEXTO` | staffchat (helper) | Chat entre a equipe online |

### Quem pode entrar

| Comando | Permissão | O que faz |
|---|---|---|
| `/whitelist on\|off\|list\|add NOME\|remove NOME` | access (admin) | Com ela ligada, só os jogadores da lista e a equipe entram |
| `/rules set TEXTO\|off` | rules (admin) | Regras novas, que todos precisam aceitar de novo com `/accept`; `off` as tira |
| `/perm [PERMISSÃO CARGO]` | perms (owner) | Qual cargo cada permissão pede; com duas palavras, muda uma |

### O servidor

| Comando | Permissão | O que faz |
|---|---|---|
| `/restart [5m\|now\|cancel] [motivo]` | restart (admin) | Reinicia com avisos em todas as telas (10m, 5m, 1m, 30s, 10s, os últimos segundos); os clientes entram de novo sozinhos |
| `/stop [5m\|now\|cancel] [motivo]` | restart (admin) | O mesmo, e o servidor fica parado |
| `/maintenance on [texto]\|off` | maintenance (admin) | Só a equipe entra; os outros são postos para fora com o texto |
| `/motd [TEXTO\|off]` | motd (admin, para mudar) | A mensagem do dia, dita a cada jogador que entra; sem palavra, mostra a mensagem |
| `/announce TEXTO` | announce (moderator) | Um aviso em todas as telas |
| `/backup` | backup (admin) | Copia agora todos os arquivos de mundo |
| `/metrics` | logs (admin) | Tempo ligado, jogadores e janelas por sala, a maior pausa entre dois ticks |
| `/logs [N] [AÇÃO]` | logs (admin) | As últimas ações de administração, todas ou de um tipo (`ban`, `rollback`...) |
| `/admin` | pages (helper) | A página de administração: quem está online, com botões para expulsar, banir e silenciar, e as páginas por trás dela |

### Blocos

| Comando | Permissão | O que faz |
|---|---|---|
| `/inspect X Y Z [SALA]` | inspect (helper) | Os últimos dez jogadores que mudaram esse bloco, de quê para quê, quando |
| `/rollback NOME DURAÇÃO [SALA]` | rollback (moderator) | Põe de volta todo bloco que esse jogador mudou nesse tempo (precisa de `--block-history`) |
| `/oldbags NOME [N]` | oldbags (admin) | As cópias de uma bolsa que ficaram para trás entre servidores amigos; com N, devolve essa |

### Portais e servidores amigos

| Comando | Permissão | O que faz |
|---|---|---|
| `/portals` | portals (admin) | Aonde levam as aberturas de cada sala, e quais servidores mandam jogadores para cá |
| `/room [list]` | rooms (owner) | As salas do servidor: ligadas ou desligadas, quantos em cada uma, quais iniciam quando são pedidas |
| `/room add NOME [FLAG [VALOR]...]` | rooms (owner) | Uma sala nova no arquivo de salas (`--rooms-file`), com as flags dela; inicia quando algo leva até ela |
| `/room start NOME` `/room stop NOME` | rooms (owner) | Inicia agora uma sala do arquivo de salas; salva e para uma vazia |
| `/room remove NOME` | rooms (owner) | Tira do arquivo de salas uma sala parada; o arquivo de mundo dela fica |
| `/friend add NOME HOST:PORTA CHAVE [compartilha]` | federation (owner) | Um servidor amigo; `compartilha` é qualquer um de `bans,presence,chat,bags,portals,mods`, todos por padrão; um NOME que já existe é substituído (um amigo cuja chave mudou) |
| `/friend remove NOME` | federation (owner) | Não são mais amigos; os banimentos dele são tirados aqui |
| `/friend list` | federation (owner) | Os amigos, e quando cada um foi ouvido pela última vez |

## Para quem faz mods

Um mod adiciona um comando com `core.command(nome, ajuda, permissão)` e responde a ele em
`core:command`; o nome é em inglês, a ajuda uma linha curta depois de `--`. Veja
[`API.md`](API.md).
