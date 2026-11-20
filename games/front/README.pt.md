# O front

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

Um campo de batalha no estilo dos grandes shooters de equipe: duas bases, três pontos para tomar,
vermelho contra azul. Dois mods: `mods/front_core` (só servidor: o campo, os blocos, como os
corpos se movem) e `mods/front_game` (`side = both`: equipes, armas, granadas, bots e pontos no
servidor, o HUD e os efeitos no cliente).

## Como chegar lá

`tools/track.sh` (ou o menu de título) inicia o sandbox com suas salas; a porta do front, uma porta
de bunker entre sacos de areia sob uma bandeira vermelha e uma azul, fica em 90.5 21 58.5
(`mods/gates`, a décima sexta boca do sandbox). Você entra na **galeria**: uma sala na crista
oeste, a meio caminho entre as duas bases e 16 blocos acima do campo, com uma janela de onde se
assiste à batalha. Quem está na galeria só assiste: não é de nenhuma equipe e os bots não o veem.
Na parede do fundo há dois portais, um vermelho e um azul, e dentro de cada um se vê a sua base.
Entrando no vermelho, você sai no bunker da base vermelha e luta pelos vermelhos; o mesmo com o
azul. O portal do bunker leva de volta à galeria e para fora da luta; de lá você pode escolher de
novo. Sob o piso de vidro da galeria fica o campo em miniatura, 1:32, visto só da galeria: ninguém
no campo o vê. A porta por onde você entrou, na ponta sul da galeria, leva de volta à praça. O
front não dá chave.

No jogo final a porta só abre para quem venceu o Hóspede no laboratório (`mods/keys`: o
laboratório manda `keys:ending`, guardado para cada sala como `ending:<quem>`).
`FRONT_OPEN = true` em `mods/keys/init.luau` a abre para todos, para testar; `false` é o jogo como
ele é distribuído. Num mundo de sobrevivência o front fica atrás de portais espalhados além de
1500 do spawn, fechados até o final, e a dificuldade do local define os bots por equipe (4 + d / 2)
e os tickets (100 + 10 d); uma rodada vencida pela equipe do jogador é o nível concluído.

## O campo

256 × 256 blocos fechados por uma crista. A base vermelha ao sul (128, 24), a azul ao norte (128,
232): um pátio de concreto com as listras da equipe, sacos de areia, um bunker e o mastro da
bandeira. No meio, na estrada entre as bases:

| ponto | onde | o que é |
|---|---|---|
| A | 48, 128 | uma fazenda entre muros de pedra, um celeiro de madeira |
| B | 128, 128 | a vila no cruzamento: seis casas de tijolo, uma praça com um poço |
| C | 208, 128 | um bunker de concreto num morro, sacos de areia em volta |

Dois muros de pedra de 2 de altura cruzam o campo em z 86 e 170, com passagens na estrada e aqui e
ali. Tudo pode ser explodido, menos o fundo e a crista.

## Como se joga

* **Tickets**: 150 por equipe. Cada morte custa um; a equipe que segura mais pontos faz a outra
  perder um ticket a cada 4 s (2 s com os três). Em zero a equipe perde a rodada, e 10 s depois
  começa outra.
* **Pontos**: conta quem está a menos de 10 blocos; cada soldado a mais que a outra equipe avança
  a captura em 10 por segundo, em 100 o ponto é tomado e a bandeira muda de cor.
* **Armas** (na mão desde o começo): o fuzil e a metralhadora atiram enquanto o botão esquerdo
  está apertado, a pistola e o fuzil de precisão um tiro por clique, R recarrega. Uma cabeça conta
  em dobro. Atirando sem mirar os tiros se espalham (a mira mostra isso); com o botão **direito**
  você mira: a arma sobe ao meio da vista, a vista se fecha, os tiros vão para onde você olha e
  você anda a 1.8 m/s. O fuzil de precisão, mirando, olha pela luneta (5x) e não erra. Sem fogo
  amigo. No pátio da sua própria base, munição e granadas recarregam.
* **Granadas**: quicam, explodem depois de 3 s, cavam uma esfera de 2.6 e derrubam o que ficou
  pendurado sobre o buraco.
* **Bots**: cada equipe é completada até 8 com soldados que o mod comanda (dá para ver a arma que
  eles seguram); vão de ponto em ponto e de vez em quando lançam uma granada. Só veem à frente (um
  cone de 110 graus, até 48 blocos, 90 com o fuzil de precisão) ou a menos de 4 blocos; quem é
  atingido se vira para o tiro. Passa um instante antes do primeiro tiro (0.45-1.1 s), eles atiram
  em rajadas, abertas no começo e depois mais fechadas, e um tiro deles faz três quartos do de um
  jogador. Quatro jeitos de lutar, em rodízio: a **sentinela** para de vez em quando para olhar em
  volta, prefere o fuzil de precisão e assume os ninhos, morteiros e obuses que encontra; o
  **atacante** corre para cima de quem vê com a metralhadora; o **cauteloso** vai primeiro para o
  abrigo, se inclina para fora para uma rajada com o fuzil e volta para trás; o **anticarro**
  começa com o lança-foguetes. Os morteiros e obuses dos bots atiram no que um bot da equipe viu
  nos últimos 3 s. Cada um fica com a sua arma enquanto tem munição, depois passa para a próxima
  (no fim, uma pistola apanhada do chão).
* **Bots nos veículos**: dirigem jipes e tanques até o seu ponto, esperando um instante para que
  os companheiros próximos entrem; o jipe os deixa no ponto, o tanque fica e para para disparar o
  canhão em quem vê a menos de 70 (um pouco desviado, a cada 5.5-7 s). Pilotam o helicóptero (18
  acima do ponto, foguetes no que está à frente), o avião e o bombardeiro (dando voltas sobre o
  ponto, bombas onde cairiam sobre o que a equipe vê). Assumem as armas e os assentos livres de um
  veículo da sua equipe parado por perto, mesmo um que um jogador dirige, e descem se ninguém
  dirige; assumem a antiaérea quando um inimigo voa perto. Deixam jipes e tanques para um jogador
  da equipe a menos de 25 blocos.

## Modelos

`tools/make_front_models.py` (Blender, com os auxiliares de `tools/make_fps_models.py`): os braços
do robô em primeira pessoa com pistola, fuzil, metralhadora e granada; as armas na mão; os
soldados vermelho e azul com idle, walk, run, fire, throw e death.

## Veículos e armas pesadas

Você entra e sai com **F** perto de um veículo, no assento livre mais próximo (por trás do jipe
você entra atrás); **1-9** trocam de assento, também dirigindo, sem trocar a arma na mão.
`/vehicle <tipo>` põe um veículo na sua frente, para testes. O primeiro assento dirige e tem a
arma do veículo; WASD dirige no chão, espaço e shift sobem e descem, o mouse guia o que voa.

| | onde | assentos | armas |
|---|---|---|---|
| jipe | dois por base | 4 | metralhadora atrás |
| tanque | um por base | 2 | canhão (o motorista), metralhadora na torre; derruba muros empurrando contra eles |
| helicóptero | a oeste da base | 3 | foguetes (o piloto), metralhadora na porta |
| avião | na estrada ao norte da base | 1 | metralhadoras, bombas com o botão direito |
| bombardeiro | no fim da estrada norte, atrás do avião | 2 | o piloto solta uma fileira de seis bombas (um poste vermelho marca onde cairiam), uma torre de metralhadora no dorso |
| antiaérea | duas no pátio da base | 1 | mísseis só no que voa: segure a mira num helicóptero ou avião inimigo por 1.2 s (LOCKING), depois LOCKED e quatro mísseis que o perseguem |
| ninho de metralhadora | nos sacos de areia ao longo das estradas | 1 | |
| morteiro, obus | nas bases | 1 | atiram para onde a mira olha (um poste vermelho marca o lugar) |

A infantaria também tem o lança-foguetes: mirando um veículo inimigo pela mira de vidro por 1.2 s
(LOCKING, depois LOCKED), o foguete o persegue. Um veículo destruído explode: os destroços queimam,
soltam peças (a torre do tanque voa longe, o jipe capota) e afundam em 5 s; um novo volta para casa
depois de 30 s.

## Ainda por fazer

Dois veículos não colidem entre si, e um jogador a pé atravessa um veículo: TODO.md, "The front's
machines". Um bot num veículo é posto de volta no seu assento pelo mod a cada tick (os assentos do
motor são dos jogadores).

## Sons

`tools/make_front_sounds.py` os sintetiza (tiros, explosões, motores, o rotor, as vozes dos
soldados) em arquivos ogg ao lado do mod.
