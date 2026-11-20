# Cubes and Portals — um sandbox de voxels feito do zero sobre raylib, com portais nativos

*[Read in English](DESIGN.md) · [Leggi in italiano](DESIGN.it.md)*

> Documento de projeto. Análise de viabilidade (gráficos, servidor, cliente multi-instância,
> scripting, extensibilidade) e um plano por marcos. Ainda não é um compromisso de escrever código:
> o §8 tem um *kill switch* explícito depois de 3 semanas de spike.
>
> Escrito em 2026-08-14, a partir da experiência de um fork anterior
> (um fork do motor de partida com portais + cliente duplo entre servidores, funcionando).

---

## Contexto

O fork anterior hoje é: ~9.700 linhas de Lua (`yaportal/init.lua` 6911, `yaportal_link/init.lua`
2221, `yafps/` 552) em cima de um **fork do motor de partida** (77 arquivos, +6.369/−309 linhas sobre
uma base de ~214 mil linhas de C++). Sete capacidades do fork são *invisíveis* para um mod comum e,
por isso, não dá para mandá-las upstream em bloco: RTT a partir de uma segunda câmera, near plane
oblíquo, `gl_ClipDistance` por vértice, teletransporte atômico num pacote só, roll da câmera,
deslocamento da caixa de colisão, **uma segunda conexão ao vivo** com outro servidor, desenhada
dentro do portal.

O custo dessa montagem não é escrever os recursos: é **correr atrás do upstream** e brigar com
arquiteturas que não foram pensadas para dois mundos vivos no mesmo processo (singletons, céu
global, uma `ViewArea` do lado da CPU que fica suja se você mexe na projeção, identidade do cliente
pelo endereço em vez de pelo mundo). Como a chance de um merge upstream é ~0, a pergunta é: **vale a
pena recomeçar de um motor próprio, com a raylib como camada de plataforma, projetado desde o dia 0
em torno de portais e multissessão?**

Este documento responde em quatro eixos (gráficos, servidor, cliente multi-instância, scripting),
com prós e contras medidos e um plano por marcos.

---

## 0. TL;DR — veredito

| Pergunta | Resposta curta |
|---|---|
| Portais com oblique near clipping são viáveis na raylib? | **Sim, e mais limpos do que hoje.** `rlSetMatrixProjection` aceita a matriz de Lengyel sem mexer no culling da CPU: o hack `setRenderProjectionMatrix` do fork some por construção. |
| Planos de corte laterais são viáveis? | **Sim.** Shaders nossos → `gl_ClipDistance[0..4]`; `glEnable(GL_CLIP_DISTANCE0+i)` é GL core, basta declarar (a raylib não expõe, mas também não bloqueia). Alternativa com `discard` para GLES/Web. |
| Um servidor em C/C++ puro é viável? | **Sim.** ENet + zstd + SQLite + FastNoiseLite. Nenhuma toolchain nova, só bibliotecas C. |
| Um cliente multi-instância para portais entre servidores? | **Sim, e é o argumento mais forte a favor da reescrita.** Hoje funciona, mas foi arrancado à força dos singletons; do zero é "uma `struct Session`, sem globais". Uma medida já em mãos: **desenhar o mundo B custa só 3-7% do frame** — a multissessão não é o gargalo. |
| Linguagem de scripting? | **Luau** (um Lua moderno: tipos graduais, geração de código nativo, uma sandbox *projetada* para código não confiável) atrás de uma interface `script_*`. O **LuaJIT** estava previsto como segundo backend para os mods legados do motor de partida e para código confiável com FFI: com a compatibilidade abandonada (2026-09-12), sobra um runtime só e a FFI não está nos planos. Também avaliados QuickJS-ng, WASM, Wren, Umka, C#: §5. |
| Estender as *mecânicas*, não só os blocos? | ECS no servidor com componentes/sistemas definidos em Lua + traits nos nós + um render graph com passes registráveis + canais de protocolo por mod. §6. |
| **Custo real** | Spike de validação ~3 semanas. Cliente+servidor jogável com portais entre servidores: **~7 meses**. Motor + jogo mínimo: **~11 meses**. Paridade de conteúdo no estilo do clássico jogo de blocos: **anos**, a não ser pelo §7.5. |
| **Recomendação** | Spike M0 como portão. Para o conteúdo, a recomendação era a **camada de compatibilidade com a API do motor de partida** (§7.5), **cancelada em 2026-09-12**: o motor de partida foi a inspiração, não o objetivo. A metade incômoda da frase continua verdadeira — um projeto como este morre no conteúdo, não nos gráficos. **Escopo fixado no mesmo dia:** um clone do clássico jogo de blocos de onde portais levam a outros mundos, cada um um servidor com seus próprios mods, até mundos que são outro jogo — uma corrida acrobática, um FPS no estilo arena. Não é menor do que o escopo que o §7.5 chamava de fatal; o que o mantém de pé é que cada modo é um mundo pequeno atrás de um portal, e não um pedaço do sandbox (M12, M13). |

### Decisões tomadas (definem o escopo deste plano)

| Decisão | Escolha | Consequência no plano |
|---|---|---|
| Objetivo | **Um clone do clássico jogo de blocos, com portais para outros mundos com mods próprios — até modos de jogo diferentes (corrida acrobática, FPS no estilo arena)** (2026-09-12) | O §7.5 está cancelado, então o conteúdo do sandbox é escrito aqui (M12) e a paridade continua a anos de distância. Cada modo é um servidor com seus próprios mods, e pede ao motor coisas que um sandbox não pede: o comportamento de um mundo chegando ao cliente, movimento rápido, acertos à distância (M13) |
| Fork do motor de partida | **Congelado** | Nenhum produto vivo durante a reescrita → M-1 obrigatório (release com tag + bugs documentados antes do congelamento) |
| Camada gráfica | **raylib atrás de `gfx_*`** (~30 funções) | Migração para `sokol_gfx` possível em dias, se for preciso no M9 |
| Multijogador | **2-8 jogadores por mundo** | SQLite basta, anti-cheat leve, interest management simples (−1 semana no M3) |
| Scripting | **Luau**, runtime único | Sandbox e tipos vêm do runtime; o custo dos bindings é pago com um gerador (§5.4). O segundo backend LuaJIT caiu junto com a compatibilidade com o motor de partida (§5.3) |
| Local | **`/home/giacomo/dev/Cubes and portals/`** | Repositório novo e independente; o fork anterior não é tocado, a não ser para o congelamento |

---

## 1. Ponto de partida — o que existe e quanto pesa

| Componente | Tamanho | Reaproveitável num motor novo? |
|---|---|---|
| `yaportal/init.lua` | 6911 linhas de Lua | **Sim, ~70%.** Geometria do portal, transformação, gatilhos, máquina de estados, os blocos clássicos dos quebra-cabeças com portais. Só mudam as chamadas `core.*`. |
| `yaportal_link/init.lua` | 2221 linhas de Lua | **Sim, ~50%.** O registro em arquivos JSON e a lógica de pareamento sobrevivem; o handshake é reprojetado (ver §4.4). |
| `yafps/` (um jogo inteiro) | 552 linhas + 160 KB de mídia | **Sim.** E mostra que um *jogo* aqui custa 5 mods e meio dia — um dado importante para o §7. |
| O renderizador de portais do fork | `portal.{cpp,h}` + `portal_manager.{cpp,h}` ≈ **1660 linhas**, + ~250 de hooks em `ClientMap`, ~240 de `Sky`, 12 de shader | **A reescrever**, mas é a única peça gráfica realmente específica: 2.000 linhas, não 200.000. |
| Resto do fork (cliente duplo, protocolo, mapnode, API Lua) | ~4.500 linhas | **Não como código, sim como conhecimento.** As invariantes do `AGENTS.MD` §461 valem mais do que o código: são bugs já pagos. |
| `docs/PORTAL_SYSTEM.md` | 1366 linhas + 9 figuras SVG | **Sim**: é a especificação do motor novo, já escrita, até as fórmulas. |
| Motor de partida | ~214 mil linhas de C++ (+ Irrlicht) | Substituído. Um motor focado precisa de ~40-60 mil para fazer o que precisamos. |
| Jogos existentes (conteúdo) | enorme | **Perdido.** É o custo escondido da reescrita. |

**Dois limites da implementação atual que um motor novo eliminaria por construção** (medidos, não
supostos):

- **Sem recursão.** O portal dentro do portal não existe hoje: o hook que desenha os quads é pulado
  durante qualquer passe de RTT (`clientmap.cpp:1281`). Profundidade máxima = 1.
- **Cada portal custa uma cena inteira em resolução cheia.** Cada slot ativo faz um `drawAll()`
  completo com o frustum culling desligado, no tamanho de renderização da tela, sem LOD, sem
  resolução reduzida, sem um grafo de visibilidade que decida quais portais importam de verdade.

**Um número para guardar:** o `docs/PORTAL_SYSTEM.md` já existe e é detalhado até as fórmulas (a
matriz de Lengyel invertida em forma fechada, afastamento do ápice `0.05·BS`, plano 4 na face
*externa* do nó, limite `n·bn < 0`). Uma reescrita não recomeça da teoria: recomeça da
implementação, com a teoria em mãos. Isso corta talvez 40% do risco gráfico.

---

## 2. Viabilidade gráfica sobre raylib

### 2.1 O que a raylib dá, o que não dá

Verificado na **raylib 5.5** (headers da instalação local em `~/dev/raylib`, binding CPython).

**Ela dá (e economiza meses):** janela/entrada/gamepad via GLFW ou SDL2, `rlgl` como abstração de GL
3.3 core / GLES 2-3, carregamento de imagens e fontes, áudio (miniaudio), matemática (`raymath`),
meshes + `UploadMesh`/`DrawMesh`/`DrawMeshInstanced`, `RenderTexture2D`, shaders próprios completos
e — o principal — **acesso de baixo nível**: `rlLoadFramebuffer`, `rlFramebufferAttach`,
`rlLoadTextureDepth`, `rlLoadVertexArray`, `rlUpdateVertexBuffer`, `rlDrawVertexArrayElements`,
`rlEnableShader`, `rlSetUniform`, `rlSetUniformMatrix`, `rlSetMatrixProjection`/`Modelview`,
`rlTextureParameters`, `rlSetCullFace`, `rlColorMask`, `rlDisableDepthMask`.

Traduzindo: **a raylib não atrapalha**. Quando é preciso GL "de verdade", a `rlgl` entrega os ids
dos objetos GL e deixa você cuidar deles.

**Ela não dá (e você precisa escrever):**

| Falta | Impacto | Remédio |
|---|---|---|
| Sem scene graph / frustum culling / oclusão | Alto — é *o* trabalho de um motor de voxels | Você escreve, e é melhor assim (§2.6) |
| Sem texture array (`GL_TEXTURE_2D_ARRAY`) | Médio | ~60 linhas de shim GL, ou um atlas com margem + mips manuais |
| GL em thread única | Médio | Meshing em workers → buffers na CPU → upload na thread do GL (como faz o motor de partida) |
| Sem shadow maps / sombras | Médio | Você escreve, ou começa sem sombras (o clássico jogo de blocos não tem). Escritas em 2026-10-11: ver ARCHITECTURE.md, "Como a luz é desenhada" |
| Sem áudio espacial 3D (só pan/volume) | Baixo | Atenuação+pan à mão, ou o miniaudio direto (já linkado) |
| Sem `BeginTextureMode` aninhado | Baixo | Não precisa: a recursão de portais é feita **do mais fundo para o mais raso** num pool de RTs (§2.5) |
| Sem UI declarativa | Médio | `microui` ou `Clay` (C, MIT) + backend raylib; algo como formspec, guiado por Lua |
| Sem rede, sem física, sem armazenamento | Alto, mas esperado | ENet / AABB próprio / SQLite (§3) |

**Conclusão honesta:** a raylib cobre *a casca*, não o motor. Ela dá talvez 15% do trabalho do
cliente — mas são os 15% mais chatos e mais cheios de bugs (plataformas, entrada, decodificação de
imagens, áudio), e são os 15% que o fork do motor de partida hoje paga com o Irrlicht, que é mantido
vivo pelo próprio projeto.

### 2.2 Pipeline de portais proposto

Um frame, mundo ativo `A`, N portais visíveis:

```
1. cull:      for the player's camera  → drawlist A
2. portal:    for every visible, linked portal, ordered by depth:
                 vcam = transform(src → dst, cam)
                 cull for vcam                         → drawlist P[i]   (bypassing the player's frustum)
                 BeginTextureMode(RT[i])
                    rlSetMatrixProjection(oblique(P_base, dst_plane))   ← Lengyel
                    upload uniform portalClipPlanes[5]; glEnable(CLIP_DISTANCE0..4)
                    render opaque(P[i]) ; render transparent(P[i], sorted for vcam)
                    glDisable(CLIP_DISTANCE0..4)
                 EndTextureMode()
3. main:      opaque(A)
              portal quads: 4 inset walls + bottom with RT[i] (screen-space UV)    ← INSIDE the opaque pass
              transparent(A) sorted for the real camera
4. post:      fog/tonemap/UI
```

Duas decisões já foram pagas com sangue no fork e precisam ser trazidas idênticas:

- **Os quads dos portais são desenhados dentro do passe opaco**, não depois. A geometria
  transparente escreve profundidade: um quad desenhado depois é cortado pelo z de qualquer vidro na
  frente do portal.
- **O passe de RTT antes do passe principal**, para o driver voltar ao FBO 0 e o passe principal
  religar seu alvo limpo.

Na raylib as duas são triviais porque **o render graph é nosso**: não existe um `Draw3D` de outra
pessoa onde entrar com um hook. Este é o primeiro ponto em que a reescrita é mais simples que o
fork.

### 2.3 Oblique near clipping na rlgl — viabilidade confirmada

A raylib usa uma projeção **RH, profundidade [-1,1]** (`MatrixPerspective` → `m11 = -1`). Lengyel
padrão, índices no layout column-major do GL (idênticos aos campos `Matrix.m2/m6/m10/m14` da
raylib):

```c
// C = clip plane in eye space (RH camera space, -Z forward), normalised,
//     anchored just inside the destination portal's outer face.
static Matrix ObliqueNear(Matrix P, Vector4 C)
{
    Vector4 Q = {
        (sgnf(C.x) + P.m8)  / P.m0,
        (sgnf(C.y) + P.m9)  / P.m5,
        -1.0f,
        (1.0f + P.m10) / P.m14
    };
    float s = 2.0f / (C.x*Q.x + C.y*Q.y + C.z*Q.z + C.w*Q.w);
    P.m2  = C.x * s;
    P.m6  = C.y * s;
    P.m10 = C.z * s + 1.0f;
    P.m14 = C.w * s;
    return P;
}
// use:
BeginTextureMode(rt); BeginMode3D(vcam);
    rlDrawRenderBatchActive();                      // flush what was there
    rlSetMatrixProjection(ObliqueNear(rlGetMatrixProjection(), plane_eye));
    draw_world(drawlist_i);                          // DrawMesh uses the current matProjection
EndMode3D(); EndTextureMode();
```

A `rlgl` recalcula `mvp = matProjection * matModelView` a cada `rlDrawMesh`/flush do batch, então a
matriz oblíqua chega tanto às meshes quanto ao batch imediato.

> **Verificado no M0 na raylib 6.0** (não 5.5: o projeto começou da 6.0). `rlGetMatrixProjection`,
> `rlSetMatrixProjection` e `rlDrawRenderBatchActive` existem, e a convenção de projeção está
> **inalterada** em relação à 5.5 — `MatrixPerspective` continua RH com profundidade `[-1,1]` e
> `m11 = -1`, então a fórmula acima vale do mesmo jeito. Duas correções à primeira versão deste
> parágrafo: **`rlGetRenderBatch()` não existe** (e nunca existiu); para esvaziar o batch interno
> usa-se `rlDrawRenderBatchActive()`. E `rlLoadShaderCode` se chama **`rlLoadShader`** (fonte única)
> ou **`rlLoadShaderProgram`** (vertex+fragment a partir de strings).
>
> A fórmula foi verificada numericamente, não só de olho: `tests/portal_math_test.cpp` confere que
> um ponto sobre o plano de corte cai exatamente em `ndc.z == -1`, no eixo e fora dele, e que a
> geometria entre a câmera e o portal cai além do near plane.
>
> Cuidado com um detalhe da raylib que não está no código acima: a struct `Matrix` **declara** seus
> campos na ordem `m0, m4, m8, m12, m1, …`, então a ordem na memória não é a ordem dos nomes. O
> índice `mN` significa `row + 4·column` (column-major) e `MatrixToFloat` reordena antes de carregar.
> Um `float[16]` column-major padrão bate com `mN` elemento por elemento, mas **converter com
> `memcpy` produz uma matriz transposta**, sem aviso.

**O bug que some sozinho:** no fork, definir a projeção real fazia o SceneManager cortar entidades
contra um frustum oblíquo (manchas pretas), e é por isso que `setRenderProjectionMatrix` existe em
`ICameraSceneNode`. Num motor próprio, o culling da CPU usa o frustum **não modificado** porque o
código é nosso e sabemos por quê: sem hack, sem patch numa classe de terceiros.

**Proteções a trazer** (as três já estão no fork, `portal.cpp:153-206`):
1. Oblíqua **só** se a vcam estiver do lado do destino (`side < -0.001·BS`). Com o jogador *dentro*
   do portal, `c_e_w` muda de sinal, o near plane vai parar atrás da câmera e o portal fica preto →
   projeção normal.
2. Pular se `c_e_z ≤ 1e-4` (ângulo rasante) ou `|dot(c_c,q)| ≤ 1e-6` — casos degenerados.
3. O plano de corte é ancorado em `dst_surface + n·0.49·BS`, *dentro* da face externa, para não dar
   z-fighting com os blocos vizinhos.

> **Esclarecimento do M0.** `dst_surface` aqui é o **centro do nó**, não a superfície visível: a
> face externa está em `+0.5·BS`, então `0.49·BS` é "logo dentro" dessa face. É a mesma convenção do
> quinto plano de corte do §2.4 (ancorado, ao contrário, exatamente em `pos + n·0.5·BS`). Confundir
> centro e superfície é o que, no fork, deixava a metade da frente do nó de saída cobrindo a imagem
> em ângulos rasantes.
>
> O sinal da proteção 2 precisa ser relido na passagem para a raylib: o fork é **Irrlicht LH** com
> `+z_eye` para a frente e usa `c_e_z > 1e-4`; a raylib é **RH** com `-z_eye` para a frente, então a
> condição equivalente é `c_e_z < -1e-4`. O plano em eye space é melhor obtido transformando o do
> espaço do mundo pela inversa transposta da view, em vez de derivar de novo os componentes à mão:
> isso tira qualquer ambiguidade de sinal.

**Precisão:** a matriz de view da vcam precisa ser montada à mão a partir do `vdir` já normalizado,
não via `lookAt(pos, target)`. A subtração `target − pos` em float32 perde precisão com portais em
posições distantes (o caso entre mundos) — no fork foi preciso uma função dedicada
(`setRenderViewMatrix`); num motor próprio, é simplesmente o jeito como a função é escrita.

### 2.4 Planos de corte laterais — dois caminhos, ambos abertos

O near plane oblíquo sozinho deixa entrar a moldura do portal de destino nas bordas, em ângulos
rasantes. São necessários os 5 planos do fork (4 laterais + a face).

**Caminho A — `gl_ClipDistance` (GL 3.3 de desktop, preferido).** Os shaders são nossos:

```glsl
// VS
uniform vec4 uClipPlanes[5];
uniform int  uClipCount;              // 0 = off
out float gl_ClipDistance[5];
...
for (int k = 0; k < uClipCount; ++k)
    gl_ClipDistance[k] = dot(vec4(worldPos, 1.0), uClipPlanes[k]);
```

A raylib não expõe `glEnable`, mas **não é preciso um loader**: `glEnable` é GL core, exportado
pela `libGL.so`. Bastam

```c
extern void glEnable(unsigned int);  extern void glDisable(unsigned int);
#define GL_CLIP_DISTANCE0 0x3000
```

e cinco linhas em volta do draw. (Alternativa limpa: `glfwGetProcAddress`, o GLFW já está linkado.)

**Caminho B — `discard` no fragment shader (GLES 2/3, Web, drivers hostis).** Os mesmos 5 planos, um
teste sobre o `worldPos` interpolado, `discard` se estiver fora. Custa o early-z e um pouco de fill,
mas é portável para qualquer lugar e fica a um `#ifdef` de distância. No GLES3 existe
`EXT_clip_cull_distance`, sem garantia.

**A trazer idênticos do fork** (são refinamentos que custaram bugs, não enfeites): ápice **fora** da
face (`vpos -= n·(apex_side + 0.05·BS)`), senão o frustum tem meio-ângulo `atan(hw/depth)` e corta
metade do destino; plano 4 ancorado na face **externa** do nó (no centro ele cobre a imagem
inteira); desvio total quando o jogador está dentro da abertura (`|lx|<hw && |ly|<hh`); limite da
inclinação para trás (`n·bn < 0` → tira-se o componente `bn` e ancora-se no centro da borda).

### 2.5 Recursão (portal dentro de portal) — uma capacidade nova

Ela não existe hoje (profundidade 1, ver §1). A raylib não aninha `BeginTextureMode`, mas isso não é
um limite: a recursão é feita **de forma iterativa**, que é também o único jeito sensato de limitar
seu custo.

```
stack = [(player_cam, depth 0)]
for depth = MAXD..1:  render the deepest levels first into pool RTs
```

Pool sugerido: `MAX_PORTALS=16` slots × `MAXD=2` níveis, RTs com **resolução reduzida conforme a
profundidade** (100% / 50% / 25%) e reuso agressivo de slots para portais fora da tela. No nível
máximo, o portal desenha uma cor chapada ou o último frame (fantasma aceitável).

### 2.6 O trabalho de verdade: o renderizador de voxels

Isto é 70% do esforço gráfico, e a raylib **não** entrega:

- **Meshing**: greedy meshing por face + oclusão entre chunks vizinhos; formato de vértice compacto
  (posição quantizada em 8 bits no espaço do chunk, normal em 3 bits, AO em 2 bits, índice de
  textura em 12 bits → 8-12 bytes/vértice). Numa thread worker; upload na thread do GL com orçamento
  (ex.: no máximo 4 chunks/frame) para não engasgar.
- **Texturas**: `GL_TEXTURE_2D_ARRAY` (uma camada por textura de bloco) acaba com o vazamento entre
  tiles do atlas e deixa o mipmapping de graça. É a falta mais chata da raylib: ~60 linhas de shim
  (`glTexImage3D`/`glTexSubImage3D` via `glfwGetProcAddress`).
  *Construído nesse meio-tempo:* o atlas continua sendo um atlas, mas o tamanho do tile é o do
  **pacote**, e não uma constante — 32 px para a arte deste repositório, 128 px para um pacote CC0
  baixado, sem mexer em nenhum arquivo. A passagem para texture arrays herda a mesma regra: um
  tamanho por pacote, decidido lendo os arquivos antes de alocar
  ([`TEXTURE_PACKS.md`](TEXTURE_PACKS.pt.md)).
- **Luz**: propagação por flood fill de sol+bloco por chunk (o modelo dos jogos de blocos clássicos,
  4 bits cada), empacotada no vértice; recálculo incremental quando um bloco é colocado.
- **Culling**: frustum por chunk + **cave culling** (visibilidade de travessia por chunk,
  pré-calculada no meshing: qual face enxerga qual face). É o que deixa um motor de voxels rápido, e
  é por isso que hoje *o gargalo medido é desenhar o mundo A*.
- **Transparência**: um balde separado, ordenado de trás para a frente **por câmera** — ou seja,
  **por sessão e por vcam**, como `setPortalCamera(true, …)` já faz no fork.
- **Draw list por vcam**: a vcam olha para um terreno que a câmera real nunca viu. Ela precisa de
  uma drawlist própria com culling próprio. **Uma invariante que não se pode quebrar** (já está na
  memória do projeto): esse caminho *nunca* pode enfileirar uma atualização de mesh — chunks de ar
  ficam permanentemente sem mesh, e enfileirá-los de novo a cada frame inunda a fila e trava o jogo.

### 2.7 Riscos gráficos

| Risco | Gravidade | Mitigação |
|---|---|---|
| Recursão + N sessões = N×M passes | Média | Orçamento por frame, RTs reduzidos, MAXD=2, portais fora da tela pulados |
| `gl_ClipDistance` em drivers antigos/GLES | Baixa | Caminho B (`discard`) já projetado |
| Precisão de profundidade em RTs distantes | Média | Reverse-Z + profundidade float32 (a projeção é nossa: aqui é mais fácil que no Irrlicht) |
| Sem sombras → visual "chapado" | Baixa | AO por vértice + neblina direcional; sombras depois |
| Engasgos no meshing | Média | Workers + orçamento de upload + reconstrução amortizada |

---

## 3. Viabilidade do servidor

### 3.1 Forma do processo

Um binário só, três modos: `--server` (sem janela, sem raylib linkada no núcleo da simulação),
cliente com servidor no mesmo processo (um jogador), cliente puro. A regra já aprendida no fork
continua valendo: **o servidor no mesmo processo morre com o cliente que o iniciou** — sobreviver ao
redirecionamento não é tentado de novo; para um mundo que precisa ficar no ar, inicia-se um processo
dedicado.

Uma separação limpa e inegociável: `core/` (voxel, ECS, mapgen, protocolo) nunca inclui `raylib.h`.
Só `client/` inclui. É o que permite compilar o servidor sem GPU e testá-lo sem janela no CI.

### 3.2 Rede

- **Transporte: ENet** (C, MIT, ~10 mil linhas, zero dependências). UDP com canais confiáveis e não
  confiáveis, fragmentação, controle de congestionamento, sequenciamento por canal. Respeita a
  restrição "só linguagens nativas" e não adiciona toolchain.
- **Canais**: 0 handshake/autenticação · 1 chunks (confiável, ordenado, comprimido) · 2 entidades
  (não confiável, sequenciado) · 3 eventos de jogo (confiável) · 4+ **canais dos mods** (§6.5).
- **Compressão: zstd** nível 3 nos chunks (ou LZ4, se for preciso latência mínima).
- **Criptografia**: o ENet não cifra. `libsodium` para o handshake X25519 + chave de sessão, e para
  as **chaves de identidade do jogador** de que o §4.4 precisa. Não é opcional se os portais levam a
  servidores de terceiros.
  > **Desvio, aplicado depois do M3.** A biblioteca é a **Monocypher 4.0.3**, não a libsodium. O
  > motivo é mais a restrição por trás desta linha do que a linha em si: a libsodium compila com
  > autotools, e colocá-la como submódulo adicionaria autoconf e libtool à lista do que é preciso
  > para compilar o jogo — exatamente o crescimento de toolchain que o §3.1 exclui. A Monocypher dá as
  > mesmas primitivas (X25519, XChaCha20-Poly1305, BLAKE2b, ed25519 para os tickets do §4.4) em dois
  > arquivos C99. Custo: bem menos olhos sobre ela. Voltar para a libsodium é uma mudança de build, e
  > não de protocolo, porque nada acima de `core/net/crypto.h` sabe o que está embaixo. O que existe
  > hoje: criptografia de sessão com forward secrecy e uma chave de servidor que pode ser fixada
  > (`--server-key`). O que não existe: as chaves de identidade **do jogador**, que ficam para o M6.
- **Protocolo**: cabeçalho versionado + uma tabela de recursos negociados no handshake, para que um
  cliente novo converse com um servidor velho. O fork hoje coloca um número mágico `0x59415054` no
  início dos seus pacotes justamente para não colidir: do zero, isso é bem projetado de saída.

### 3.3 Simulação, armazenamento, mapgen

- **Tick com autoridade** a 20 Hz, desacoplado do frame rate do cliente. Interpolação no cliente,
  previsão só do jogador local, reconciliação para trás.
- **Interest management**: por jogador, um raio em chunks + prioridade pela direção do olhar. Um
  portal adiciona **um segundo centro de interesse** (o destino) — uma regra nativa aqui, um remendo
  no fork.
- **Armazenamento**: SQLite com WAL (Write-Ahead Logging) + compressão de chunks com **zstd** (nível
  3) — transacional, compacto em disco e resistente a corrupção numa queda/falta de energia
  repentina. Se virar gargalo sob cargas extremas, arquivos de região próprios no estilo Anvil.
- **Mapgen**: FastNoiseLite (C, um header só) + um pipeline de etapas registráveis a partir de Lua
  (§6). Determinístico pela seed: obrigatório se você quer testar sem janela.
- **Threads**: um job system com pool (N-1 núcleos): mapgen, meshing (cliente), iluminação, E/S. O
  tick do jogo continua em thread única (determinismo e sanidade); os mods rodam ali.

### 3.4 Autoridade e trapaça

Movimento validado no servidor (limite de velocidade, `noclip` desligado), colocar/quebrar validado
por distância e permissões, inventários só no servidor. Os portais trazem um caso próprio: o
teletransporte é *decidido pelo servidor* (como hoje em Lua), o cliente o desenha. O "moved too
fast" depois da troca, hoje aberto como BUG-10 no fork, é evitado na raiz: o pacote da troca leva um
**grace token** que suspende a verificação de velocidade por N ticks no servidor de destino.

---

## 4. Cliente multi-instância — o portal entre servidores diferentes

### 4.1 Requisito número um: zero estado global

Toda a dificuldade do cliente duplo do fork vem daí: `Sky`, `Clouds`, a distância de visão, "quem
desenha o próprio avatar", a entrada, a identidade do cliente — tudo coisas que existiam uma vez só.
As invariantes do `AGENTS.MD` são, todas elas, cicatrizes de singleton.

A regra do motor novo, aplicada em tempo de compilação:

```c
typedef struct Session {           // ALL of a world's state lives in here
    NetPeer     *peer;             // connection to that server
    ChunkStore   map;              // voxels + light + meshes
    EntityStore  entities;
    ContentReg   content;          // block/item registry OF THAT server (namespaced)
    Sky          sky;  Clouds clouds;  float view_range;
    Camera3D     cam;   bool has_camera;   // false = passive
    LuaState    *csm;              // any client-side scripts, isolated
} Session;

Session *sessions[MAX_SESSIONS];   // 0 = active; the others passive
int      active_session;
```

Nenhuma função de `core/` ou `client/world/` recebe dados implícitos: a sessão é **sempre** um
parâmetro. A troca vira `active_session = i` mais um punhado de transições explícitas, em vez das
promoções/rebaixamentos cirúrgicos de hoje.

### 4.2 Custos — já existe uma medida

Do trabalho com o cliente duplo no fork: **desenhar o mundo B custa 3-7% do frame**; o gargalo é
desenhar o mundo A. Duas consequências diretas:

1. A multissessão **não é o problema de desempenho**. 3-4 mundos vivos ao mesmo tempo são plausíveis,
   com orçamentos e distâncias de visão reduzidos para os passivos.
2. O maior ganho de um motor novo não está nos portais: está no **renderizador do mundo principal**
   (cave culling, vértices compactos, menos draw calls), onde o motor de partida deixa muito na mesa.

Orçamento por sessão passiva: distância de visão ~1/3, sem áudio, sem partículas, sem previsão,
ticks de mesh com prioridade baixa, RTs na resolução do portal (não na da tela).

### 4.3 Protocolo de link e troca

O modelo atual está certo, só precisa de uma limpeza. O **cliente** faz todo o trabalho; os dois
servidores não conversam para uma travessia (fundamental: não há confiança mútua a estabelecer para
passar de um mundo a outro).

**Exceção desde 2026-10-27: servidores amigos** (`/friend`, `src/server/admin.h`). Dois servidores
que se colocam um na lista do outro trocam, numa porta TCP própria (`--federation`), linhas
assinadas com a chave de assinatura de cada um e cifradas para quem as recebe: banimentos
permanentes por chave, quem está online (`/where`), um chat (`/fed`) e a mochila. A confiança existe
porque foi dada à mão, dos dois lados, e pode ser revogada (`/friend remove` também remove os
banimentos dele). Nada do que é preciso para atravessar passa por ali: um portal para um amigo
funciona como para qualquer outro. Já a mochila é uma só para todos os amigos: ela fica em um
servidor de cada vez, e o servidor aonde o jogador chega a pede ao servidor de onde ele vem (ou a
quem tem a cópia mais recente), que a entrega; itens que um mundo não conhece ficam escondidos e
viajam com a mochila (docs/ADMIN.md).

```
1. Server A → client:  PORTAL_LINK { portal_id, host, port, world, dst_pos, dst_yaw, ticket }
2. Client:             opens a passive session on B (normal handshake + ticket)
3. Client:             when the passive one is Ready → declares the state to A, with HEARTBEAT
4. Crossing:           A and B validate; swap in place; A parks the ghost, B promotes
5. Fallback:           no passive ready → classic redirect with loading screen
```

Invariantes já pagas, a levar para a especificação do protocolo (e não "a lembrar"):

- O estado da sessão passiva é **repetido por timer** (2 s), nunca enviado e esquecido: o servidor
  principal descarta pacotes de jogo até o cliente estar ativo ali, e o mundo A pode levar dezenas
  de segundos carregando depois que a passiva já está pronta.
- Uma sessão passiva se declara **no handshake**, não pelo chat: o servidor precisa saber *antes*
  de fazer o avatar fantasma aparecer para qualquer um.
- Uma sessão é identificada pelo **nome do mundo (+porta)**, nunca pela string do endereço:
  `127.0.0.1` e o IP da LAN são o mesmo mundo, e comparar endereços transforma toda volta para casa
  num redirecionamento.
- A troca **não zera as teclas seguradas**: só as bordas (`pressed/released/wheel`). Entrar numa
  porta com W segurado precisa continuar andando.
- A troca **decide de novo quem desenha o próprio avatar**: a sessão promovida sim, a rebaixada não.
- O céu da sessão promovida é tirado do `Sky` **vivo** da passiva, nunca da repetição dos eventos (um
  servidor manda `set_sky` uma vez só: na segunda volta a repetição está vazia).
- `init_secondary()` é **idempotente**: duas conexões com o mesmo nome são recusadas pelo servidor e
  matam a passiva que estava funcionando.

### 4.4 Identidade e autenticação (o verdadeiro salto de qualidade)

Hoje a sessão passiva entra como um usuário fantasma `<name>_pv` com senha vazia. Isso não é
aceitável num jogo de verdade. Projeto:

- Cada jogador tem um **par de chaves** (ed25519, `libsodium`) gerado na primeira execução. A
  identidade é a chave pública; o nome é só um rótulo.
- O servidor A emite um **ticket** assinado `{pubkey, portal_id, exp, scope=OBSERVE|ENTER}`. O
  cliente o apresenta a B. B o aceita se: a assinatura é válida, A está na sua lista de pares
  confiáveis **ou** B aceita observadores anônimos (uma configuração por mundo).
- Escopo `OBSERVE` = sem avatar, sem inventário, sem interação: só streaming de chunks numa esfera em
  volta do ponto de saída. Isso também reduz a superfície de abuso (nada de "entro passivo em 50
  servidores para minerar dados").
- Limite de taxa: N sessões passivas por IP, tempo esgotado se o jogador nunca atravessa.

### 4.5 Conteúdo e colisões de assets

Dois servidores = dois registros de blocos e dois conjuntos de mídia, e hoje isso já produz avisos
de colisão de mídia no caso entre jogos. Projeto:

- Mídia **endereçada por conteúdo** (BLAKE3 do arquivo), um cache em disco compartilhado entre todas
  as sessões. Dois servidores que mandam a mesma textura fazem o download dela uma vez só.
- Ids de conteúdo **por sessão**: `session->content` mapeia `numeric id → string name`. Nenhum id
  global, nunca. As texturas vão para camadas de texture array alocadas por sessão, com um orçamento.
- Handshake: o cliente manda a lista de hashes que já tem; o servidor manda só a diferença.

---

## 5. Scripting

### 5.1 Os critérios, antes dos nomes

Uma linguagem de scripting para este jogo precisa aguentar cinco coisas ao mesmo tempo. Estão em
ordem de peso, e são o motivo de a resposta não ser automaticamente "Lua, como o motor de partida":

1. **Uma sandbox para código não confiável.** Os portais levam a servidores de outras pessoas; cedo
   ou tarde alguém vai querer mandar scripts para o cliente. Um runtime em que "sandbox" significa
   "apague o `io` e torça" não basta.
2. **Custo dos bindings.** O motor é C/C++: cada API nova não pode custar 50 linhas de cola.
3. **Desempenho previsível.** Nenhuma pausa de GC visível a 60 fps com milhares de entidades.
4. **A restrição do projeto**: só Lua e C/C++, nenhuma toolchain nova para instalar — nem para você,
   nem para quem for escrever mods.
5. **Continuidade com o motor de partida**, porque o §7.5 (camada de compatibilidade) e ~9.700
   linhas de Lua já escritas pesam.

### 5.2 O campo, avaliado a sério

| Runtime | Ano / estado | Sandbox | Desempenho | Binding | Toolchain para quem escreve mods | Veredito |
|---|---|---|---|---|---|---|
| **Luau** | ativo, C++, MIT | **Projetada para código hostil**: globais congelados, sem `getfenv/setfenv/loadstring`, sem `os/io`, interrupções com orçamento, limite de memória por VM | Interpretador ~2-3× o PUC Lua; **geração de código nativo** x86-64/arm64 para o código quente | API no estilo `lua_State` (sem FFI) | Nenhuma (opcional: verificador de tipos + LSP) | **Escolhido (principal para gameplay/nós/UI)** |
| **WASM** (WAMR / Wasmtime) | ativo, C/Rust, Apache/MIT | **Isolamento total (ISA/memória virtual)**: zero acessos não concedidos pelo host | **Quase nativo (JIT/AOT)** | Marshalling / C-ABI / Component Model | **Sim: precisa de um compilador (Rust/Zig/C++/C#)** | **Escolhido como extensão para plugins pesados (§5.8, §6.6)** |
| **TypeScript / JS** (Hermes / QuickJS-ng) | ativo, C++, MIT | Excelente (realms isolados; Hermes com bytecode AOT compacto) | Bom; GC determinístico no Hermes | Verboso no QuickJS, estruturado no Hermes | TypeScript opcional | Alternativa para quem faz mods em JS/TS; possível via um transpilador para Luau (`roblox-ts`) ou o runtime Hermes |
| **LuaJIT 2.1** | rolling, C, MIT | Fraca se houver FFI (= acesso total à máquina) | O mais rápido do grupo | **FFI: zero cola** | Nenhuma | ~~Segundo backend~~ — o motivo (compatibilidade legada) caiu, §5.3 |
| Lua 5.4 (PUC) | oficial | Média | Lento | manual | Nenhuma | Alternativa de portabilidade |
| Wren | quase parado | Média | Médio | Simples | Nenhuma | Não: ecossistema parado |
| Umka | ativo, C, pequeno | Média | Bom (tipado) | Simples | Nenhuma | Não: comunidade minúscula, sem continuidade |
| Janet / Fennel | ativos | Média | Médio | Médio | Fennel: nenhuma | Não: sintaxe Lisp = uma barreira para quem faz mods |
| C# (.NET NativeAOT/Mono) | ativo | Fraca sem AppDomain | Excelente | Bom | **SDK do .NET obrigatório** | Não: runtime pesado (30-80 MB) |
| Python (CPython) | ativo | Péssima para isolar | GIL, inicialização lenta | Médio | Nenhuma, mas pesado | Não |

### 5.3 Escolha: Luau principal, LuaJIT como segundo backend

**Por que Luau e não LuaJIT como primeira escolha**, num jogo que abre portas para máquinas de
outras pessoas:

- É **o único Lua moderno projetado para rodar código de desconhecidos**: uma grande plataforma roda
  milhões de scripts de usuários nesta VM. Globais congelados por padrão,
  `getfenv`/`setfenv`/`loadstring` removidos, `io`/`os`/`debug` ausentes, orçamento de interrupção e
  limite de memória por VM são *primitivas do runtime*, não convenções suas.
- **Tipos graduais**: `--!strict`, anotações, verificador de tipos e LSP oficiais. Numa API de jogo
  com centenas de funções, a diferença de usabilidade para quem escreve mods é enorme em relação ao
  Lua 5.1 puro — e é o que o usuário pediu com "mais moderno".
- **Sintaxe moderna** sem quebrar o Lua: `continue`, interpolação de strings, `+=`, iteração
  generalizada, `type`/`export type`.
- **Geração de código nativo** (x86-64 e arm64) no código quente: não é o JIT do LuaJIT, mas cobre o
  caso "sistema ECS em Lua chamado 20 vezes por segundo sobre 5.000 entidades".
- C++ sob licença MIT, compilável com CMake: cabe na restrição "só Lua e C++".

**Por que o LuaJIT ficou como backend opcional** — e por que não é mais trabalho planejado:

1. ~~**Os mods legados do motor de partida não rodam no Luau sem mudanças.**~~ Era o primeiro
   motivo, e ele cai junto com a camada de compatibilidade (M11 cancelado, §7.5): não existe mais um
   mod de Lua 5.1 para rodar.
2. **FFI para código confiável**: sistemas ECS pesados e código do motor escrito em Lua ganhariam
   structs C nativas sem cola. É o único motivo que sobra, e nenhum número pediu isso ainda — o
   profiler do M9 diz para onde vai um frame, e não é para os bindings.

```
script_* (interface, ~25 functions)
   ├── backend "luau"    → all mods: new, downloaded, anything untrusted        [the only one]
   └── backend "luajit"  → never written; would stay for FFI and trusted hot path [not planned]
```

A mesma forma já escolhida para `gfx_*`: uma interface fina, para a decisão continuar reversível.
**Hoje há um runtime só**, e é o resultado que o critério de revisão escrito aqui procurava — por
outro caminho: não porque a compatibilidade rodou no Luau com pouco shim, mas porque a
compatibilidade não vai ser feita (§7.5).

### 5.4 Custo dos bindings sem FFI (o preço real do Luau)

É a única desvantagem real em relação ao LuaJIT, e precisa ser paga com ferramentas, não à mão:

- As APIs são declaradas **uma vez** numa tabela descritiva (nome, argumentos, tipos, flags); um
  **script Lua em tempo de build** gera a cola C++ (`lua_pushnumber`/`luaL_check*`), o arquivo de
  definições `.d.luau` para o verificador de tipos e a documentação em Markdown. Um gerador em Lua =
  nenhuma toolchain nova (restrição 4 respeitada).
- Estruturas quentes (posições, blocos, componentes ECS) não passam por tabelas: usam-se **userdata
  com metatabelas** e iteradores em lote (§6.1), para o custo por entidade ficar no C++.

### 5.5 Autoria moderna, opcional

Quem quiser pode escrever mods em **Luau tipado** (`--!strict`), ou em TypeScript transpilado para
Luau (com um transpilador TS→Luau) — mas **o jogo nunca exige isso**: um mod continua sendo um
arquivo `.luau` que abre num editor de texto e funciona. Essa é a linha que não se cruza, e é por
isso que C#, WASM e afins ficam de fora: no momento em que quem escreve um mod precisa instalar um
SDK, a biblioteca de mods morre.

### 5.6 Sandbox e segurança

O fork hoje tem `secure.trusted_mods` e mods que leem/escrevem um diretório compartilhado. No motor
novo, "sandbox" significa algo preciso, e o Luau entrega metade de graça:

- **Uma VM por mod** (não um ambiente compartilhado): isolamento de falhas e de globais.
- **Um manifesto com capacidades declaradas**: `fs:read <dir>`, `net:http <host>`, `native`, `exec`.
  Sem manifesto → sem `io`, `os`, `package`, e ponto. O carregador os remove *antes* de rodar uma
  linha do mod. No Luau, muitos nem existem para serem removidos.
- **O backend LuaJIT (e portanto a FFI) é uma capacidade de nível máximo**, equivalente a "este mod é
  código nativo": um aviso explícito ao usuário na instalação, nunca concedido a um mod vindo da
  rede.
- **Cota de memória** por VM e **orçamento de interrupção** por tick (`lua_callbacks().interrupt` no
  Luau, um hook por contagem de instruções no LuaJIT): um mod em laço infinito é interrompido, ele
  não congela o servidor.
- **Mods do lado do cliente** — ~~por padrão nenhum baixado do servidor é executado; se algum dia
  for, só em Luau, assinados e numa lista de permitidos~~. **Decidido em 2026-09-13 (M13): um mundo
  pode mandar código Luau ao cliente, e o cliente pode recusá-lo.** A frase "é a diferença entre um
  jogo e uma botnet" continua verdadeira, e é o motivo das condições:
  - só **Luau**, nunca código nativo; **uma VM por mod e por sessão**, que morre com a sessão;
  - **capacidades só de cliente**: desenhar, HUD, entrada, o canal do próprio mod, ler o mundo da
    própria sessão. **Sem arquivos, sem rede, sem processos**, e nenhuma capacidade de manifesto que
    um mod instalado à mão poderia pedir;
  - **cota de memória e orçamento de interrupção**, como no servidor: um mod que entra em laço para,
    o cliente não;
  - **a autoridade fica com o servidor**: o código que chega prevê e desenha, não decide. Um cliente
    que o recusa não ganha nada — o servidor verifica do mesmo jeito — e perde o que o mundo queria
    mostrar a ele;
  - **a recusa é do jogador**: para todos os mundos ou para um mundo, e um mundo recusado é jogado do
    mesmo jeito, sem o seu código.

  **Feito em 2026-09-13** (TODO.md OPEN-49). Um mod diz onde roda com `side = server | client |
  both`; o servidor empacota os de cliente e os oferece como mídia (`worldcode.<mod>.cnpmod`, no
  máximo 1 MB como toda mídia); o cliente os carrega no `ModHost` do mundo em que está, com uma
  tabela de funções da qual foi tirado tudo o que escreve no mundo ou registra conteúdo, nenhuma
  capacidade, e cota e interrupção não acima dos padrões. A recusa é `--world-code refuse`, ou a
  chave de assinatura do mundo em `world_code_refused.txt` no cache de mídia.

### 5.7 Hot reload e determinismo

- Cada mod declara seu estado serializável; a recarga recria o estado a partir do serializado e
  registra de novo os handlers. Um registro com gerações: na recarga, só as entradas do mod são
  descartadas.
- O tick do jogo é determinístico por seed+entrada: fundamental para testes sem janela (já existe
  hoje uma prática de testes sem janela com mods descartáveis e grep no log — ela precisa ser
  promovida a framework de primeira classe, não a gambiarra).

### 5.8 Estratégia híbrida de modding e scripting (recomendação de arquitetura)

Para juntar o máximo de acessibilidade para quem faz mods casualmente e potência nativa para
sistemas complexos, a arquitetura adota uma **estratégia em 3 níveis**:

1. **Nível 1 — Gameplay e blocos (Luau)**:
   - Padrão para 90% dos mods (blocos, itens, receitas, UI, lógica de interação).
   - Zero toolchain para quem escreve o mod: arquivos `.luau` editáveis a quente.
   - Tipos estáticos opcionais (`--!strict`), um LSP que se liga ao VS Code, uma sandbox nativa à
     prova de falhas.
2. **Nível 2 — Sistemas de alto desempenho e modding poliglota (WASM via WAMR)**:
   - Para geradores de mundo procedurais complexos, simuladores de fluidos ou física própria.
   - Quem faz mods avançados pode compilar para `.wasm` a partir de **Rust, Zig, C++, C# ou
     AssemblyScript**.
   - Uma sandbox com isolamento total da memória (nível ISA): sem acesso a syscalls nem a memória não
     autorizada.
3. ~~**Nível 3 — Compatibilidade legada e hot path confiável (LuaJIT 2.1)**~~:
   - Era para a camada de compatibilidade com o motor de partida. Sem ela (§7.5, 2026-09-12), sobra
     um nível sem usuário: nenhum mod para rodar, e nenhum número pediu FFI ainda.

---

## 6. Estender as *mecânicas*, não só os blocos

Registrar um bloco é fácil e todo mundo faz. O problema de verdade é: **como um mod adiciona um
sistema que o motor não previu** (um tipo de fluido, um circuito, um portal não euclidiano, um
veículo)? Seis alavancas, em ordem de poder crescente.

### 6.1 ECS moderno: adotar o Flecs (C/C++)

Em vez de escrever à mão um motor ECS do zero, com a inevitável dívida técnica, a recomendação é
integrar o **Flecs** (C99/C++20, licença MIT, zero dependências):
- **Armazenamento nativo por arquétipos**: consultas vetorizadas e amigáveis ao cache, com
  desempenho altíssimo (milhões de entidades/s).
- **Hierarquias e relações nativas**: suporte de primeira classe a pares/relações (ex.:
  `(PortalChildOf, ParentPortal)`, `(InWorld, SessionId)`), fundamental para o multimundo.
- **Bindings prontos**: o Flecs já expõe bindings para C/C++ e Lua/Luau e integração com WASM.
- **Painel de depuração REST / web**: o Flecs inclui um módulo explorador para inspecionar
  visualmente o estado de todas as entidades em tempo real no navegador durante o desenvolvimento.

Entidade = id + conjunto de componentes; o mod declara componentes (esquema tipado → layout compacto
do lado C) e registra sistemas em fases ordenadas:

```luau
--!strict
core.ecs.component("mymod:magnet", { radius = "f32", strength = "f32" })
core.ecs.system("mymod:attract", {
    phase   = "pre_physics",             -- input | pre_physics | physics | post_physics | net
    after   = { "core:gravity" },
    query   = { "mymod:magnet", "core:transform" },
    run = function(dt: number, it: core.Iter)   -- batch iteration, not per entity
        for i = 1, it.n do
            continue                      -- Luau syntax; the type of `it` is checked
        end
    end,
})
```

A consulta é um *iterador de arquétipos* que o Flecs executa em C: o custo por entidade continua
nativo, o Lua vê um lote compacto. É isso que permite a um mod adicionar uma *física* nova, e não só
comportamento.

### 6.2 Traits nos nós, em vez de comportamentos fixos no código

No motor novo, "sólido", "líquido", "escalável", "superfície de portal", "conduz sinal" não são flags
de um enum fechado, mas **traits** que um mod pode definir e que o motor consulta por tabelas de
despacho compiladas no momento do registro:

```lua
core.trait.define("mymod:conductive", { on_signal = "function", delay = "u8" })
core.node.register("mymod:wire", { traits = { ["mymod:conductive"] = { delay = 1 } } })
```

As faces sólidas direcionais (que no fork são bits em `param2` e patches em `mapnode.cpp`) viram um
trait padrão: `solidity = { [FACE_NX] = true, ... }`. **O portal_block do tipo 2 deixa de ser um
patch no motor.**

### 6.3 Barramento de eventos tipado

Eventos com esquema declarado, handlers com prioridade e a possibilidade de cancelar/modificar; o
próprio núcleo só emite eventos (nenhum caminho "especial" reservado ao motor). Se
`portal:pre_teleport` pode ser cancelado, um mod pode fazer "os portais não funcionam no Inferno" sem
tocar no `yaportal`.

### 6.4 Render graph extensível

A alavanca que falta totalmente ao motor de partida hoje. O cliente expõe o grafo de passes como
dados:

```lua
core.render.add_pass("mymod:heat_haze", {
    after   = "core:transparent",
    shader  = "mymod:haze",
    inputs  = { color = "core:scene", depth = "core:depth" },
    enabled = function() return player_in_desert end,
})
```

Com isso, "adicionar um novo tipo de portal com efeito visual próprio", hoje impossível sem fazer um
fork, vira um mod. É também o seguro contra repetir esta mesma análise daqui a dois anos.

### 6.5 Canais de protocolo por mod

Um mod declara mensagens com esquema e ganha um canal ENet dedicado:

```lua
local ch = core.net.channel("yaportal:link", { reliable = true })
ch:define("state", { portal = "u16", ready = "bool", world = "string" })
ch:send_to(player, "state", { portal = 3, ready = true, world = "backportal" })
```

Nenhum opcode novo no motor para cada recurso — que é exatamente o que o fork precisa fazer hoje
(`0x65`-`0x68`, `0x54`).

### 6.6 Plugins nativos (C ABI)

Para o que o scripting não aguenta (mapgen próprio pesado, física de veículos, compressão): mods que
trazem uma `.so`/`.dll` com uma tabela versionada de ponteiros de função. Sem sandbox, ativação
explícita pelo usuário, versão da ABI verificada no carregamento.

**Uma alternativa moderna, a preferir quando o desempenho permite: WASM** (WAMR, C, Apache-2) no
lugar de `dlopen`. O mesmo caso de uso — código compilado de outra linguagem — mas **isolado por
construção**: um plugin WASM que trava ou tenta uma syscall não derruba o servidor, e pode vir da
rede sem que o usuário assine um cheque em branco. O preço é o marshalling (nenhum valor dinâmico
atravessa a fronteira) e o fato de que o autor precisa compilar. Ordem recomendada: `dlopen` para os
plugins locais do usuário, WASM para tudo o que é distribuído.

---

### 6.7 Superfície mínima de API para portar o `yaportal` (um critério de aceitação concreto)

Um inventário real das chamadas que os três mods usam hoje. É a lista de compras do motor novo: se
isto estiver coberto, o mod é portado; se faltar uma peça, sabemos exatamente qual.

**Motor de partida padrão (o grosso, ~120 chamadas distintas):**

| Grupo | O que é preciso de verdade |
|---|---|
| Registro | `register_node` (~150 nós gerados em laços), `register_tool`, `register_craftitem`, `register_entity`, `register_craft`, `register_alias`, `register_lbm` (8: registram de novo botões/ventiladores/portas/pedestais/mostradores quando o mapblock carrega), `register_abm`, `register_biome`, `override_item` |
| Callbacks | `register_globalstep` (13 steppers independentes), entrar/sair/morrer/renascer, `on_dignode`, `on_player_hpchange`, `on_mods_loaded`, `on_shutdown`, `on_player_receive_fields`, `after` |
| Definições de nó | drawtype `nodebox` (×14) e `mesh`, `node_box`/`selection_box`, `paramtype2` facedir+wallmounted, `special_tiles`, `groups`, `drop`, `light_source`, `glow`, `pointable`, `walkable`, `use_texture_alpha`, `sunlight_propagates` |
| Modificadores de textura | `[combine` (o mostrador de 7 segmentos do relógio é montado em tempo de execução!), `[colorize` |
| Mapa | `get/set_node`, `swap_node`, `get_meta` (23 lugares) com tipos mistos, `hash_node_position`, `emerge_area`, `raycast`, `get_voxel_manip`, `is_protected`, `item_place` |
| Jogador/Objeto | pos/vel/accel, direção do olhar/yaw/pitch, `get_player_control`, `get/set_properties`, `set_physics_override`, `get_inventory`, `get_player_information`, `get_sky` |
| UI | `show_formspec` (9 lugares), `formspec_version[4]`, `textlist`, `dropdown`, `checkbox`, `field_close_on_enter`, `explode_textlist_event` |
| Armazenamento | `get_mod_storage`, `serialize`/`deserialize`, `write_json`/`parse_json`, `get_worldpath`, `get_modpath` (32 lugares — detectar o jogo hospedeiro) |
| Partículas/som | `add_particle(spawner)`, `sound_play` |

**Só do fork (~20 chamadas) — no motor novo precisam ser APIs de primeira classe, não patches:**
`set_portals`, `set/clear_portal_cam_hint`, `register/update_portal_sky_type`,
`set/clear_portal_sky`, `set/clear_portal_xworld_dest`, `set/clear_xworld_target`,
`redirect_player`, `xworld_swap_player`, `player_dual_ready`, `xworld_exchange(_posts)`,
`get_bind_port/address`, `player:portal_teleport(...)` (atômico: pos+yaw+pitch+roll+velocidade+céu),
`player:set/get_look_roll`, `set_look_pitch_animate`, `set_pos_look`, mais a chave
`physics_override.floor_portal_cb_shift` e os **três grupos interpretados pelo C++**
(`portal_block`, `portal_frame`, `portal_slab` + a codificação do recorte nos bits 3-6 de `param2`).
No modelo do §6.2 esses três grupos viram **traits declarados em Lua** e deixam de ser código do
motor.

**Duas dependências "feias" a trocar por recursos de verdade** (hoje são brechas):
- `request_insecure_environment()` → `io`/`os` reais e `io.popen` para **iniciar outros servidores**.
  No motor novo: um **supervisor de mundos** no núcleo (API `core.world.spawn{game, port}`), com
  permissões explícitas — não um buraco na sandbox.
- `request_http_api()` + um serviço `PortalExchange` em `port+5000` → um **serviço de registro**
  nativo, com registros assinados (§4.4), não HTTP em texto puro com uma chave compartilhada.
- Monkey-patching de `register_abm` e `rawset(_G, …)` para contornar o limite de 200 locais por
  chunk: sintomas de uma API sem namespaces próprios. Na nova: módulos por mod, sem `_G`.

**Uma observação que vale ouro:** hoje o cliente duplo compartilha o cache de texturas *pelo nome*
entre os dois jogos, e a única defesa é o prefixo `yafps_` em cada arquivo. É exatamente o bug que o
§4.5 (mídia endereçada por conteúdo + ids por sessão) elimina por construção.

---

## 7. Prós e contras — veredito honesto

### Prós da reescrita sobre raylib

1. **A arquitetura já nasce com os portais dentro.** Multissessão, render graph, drawlists por
   câmera virtual, culling separado: tudo de primeira classe, em vez de enxertos.
2. **A dívida do Irrlicht some.** O Irrlicht é mantido pelo próprio motor de partida; hoje você paga
   pela pilha de função fixa dele, pelo scene manager dele, pelos shaders dele.
3. **Chega de correr atrás do upstream.** Um fork que nunca pode ser mesclado é um imposto vitalício:
   cada rebase é um risco de regressão nas invariantes do `AGENTS.MD`.
4. **Desempenho potencialmente muito maior.** No mundo principal (que *é* o gargalo medido): cave
   culling, vértices compactos, menos draw calls. Nos portais: hoje cada portal ativo custa uma cena
   inteira em resolução cheia sem culling; com RTs reduzidos e um grafo de visibilidade de portais, o
   custo cai por um fator, não por uma porcentagem.
5. **Melhor programabilidade desde o início**: FFI, ECS, render graph, canais de rede por mod.
6. **Uma base de código que uma pessoa consegue entender**: 40-60 mil linhas focadas contra 214
   mil + Irrlicht.
7. **Distribuição mais simples**: a raylib é estática e minúscula; nada de AppImage de 85 MB.
8. **A especificação já está escrita**: `PORTAL_SYSTEM.md` + as invariantes são metade do trabalho
   de projeto.

### Contras (e são pesados)

1. **Conteúdo.** Um jogo completo para o motor de partida são anos-pessoa de trabalho. Sem um jogo,
   o motor é uma demo. É **o** risco do projeto, não os gráficos. Este contra foi escrito quando a
   resposta esperada era rodar esse conteúdo (§7.5); sem a compatibilidade, ele continua exatamente
   como está, e a única resposta é escolher um jogo que uma pessoa consiga terminar.
2. **Tudo o que é chato e invisível**: inventários, crafting, GUI, i18n, salvamentos, migrações, mobs
   e IA, clima, sons, acessibilidade, controles, multiplataforma.
3. **Nenhuma comunidade no dia 1.** O motor de partida traz mods, servidores, jogadores, wiki.
4. **O tempo de agora vai para recursos que já funcionam hoje.** Você está na build 8, com crafting,
   entre jogos e entre servidores funcionando: uma reescrita volta tudo para zero.
5. **A raylib é feita para jogos pequenos.** Ninguém lançou um MMO de voxels com raylib; todo sistema
   sério (culling, texture arrays, threads) você adiciona sozinho — a raylib não ajuda, só não
   atrapalha.
6. **Bus factor 1.** Um motor próprio sem comunidade é um motor que morre com o autor.

### Alternativas à reescrita completa

| Opção | Custo | Prós | Contras |
|---|---|---|---|
| **Ficar no fork do motor de partida** | 0 | Funciona hoje, conteúdo de graça | Imposto vitalício de rebase, teto de desempenho, arquitetura hostil à multissessão |
| **Reescrita em raylib (este documento)** | 6-10 meses até ficar jogável | Controle total, portais nativos | Zero conteúdo, zero comunidade |
| **Motor de uso geral + extensões em C++** | 3-5 meses | Editor, UI, pipeline de assets, comunidade | Os portais ainda precisam de viewports+matrizes próprias; multissessão = duas `SceneTree`s (difícil); GDScript ≠ restrição de Lua; um motor grande para entender |
| **bgfx / sokol_gfx em vez da raylib** | +1 mês em relação à raylib | Vários backends (Vulkan/D3D/Metal), threads de renderização de verdade | Mais cerimônia, menos "pilhas incluídas"; sokol é C (ok), bgfx é C++ |
| **SDL3 + GL puro** | +1-2 meses em relação à raylib | Zero abstrações entre você e a GPU | Você reescreve o que a raylib dá de graça (imagens, fontes, áudio, matemática) |

**Uma nota sobre bgfx/sokol:** se o objetivo real é *um motor sério*, o `sokol_gfx` (C, um header
só, GL/Metal/D3D11/WebGPU) é tecnicamente uma escolha mais sólida que a raylib para o renderizador, e
a raylib pode ficar só para janela/entrada/áudio... ou `sokol_app`. **Escolha adotada: raylib atrás
de `gfx_*`** — velocidade de protótipo agora (verificado que ela expõe tudo o que é preciso), com uma
interface fina de ~30 funções que torna a migração para o sokol uma questão de dias.

A interface `gfx_*` a respeitar (o resto do cliente passa por aqui, nunca por `raylib.h`):

```c
gfx_init/shutdown/begin_frame/end_frame
gfx_buffer_create/update/destroy          gfx_vertexlayout_create
gfx_texture_create/update/destroy         gfx_texture_array_create   // GL shim with raylib
gfx_shader_create/destroy                 gfx_uniform_set{f,v4,mat4,int}
gfx_rt_create/destroy/begin/end           gfx_rt_texture
gfx_draw(buffer, count, offset)           gfx_draw_instanced
gfx_state_set(depth, cull, blend, colormask)
gfx_clip_planes_set(vec4 *planes, int n)  // gl_ClipDistance or discard, behind the scenes
gfx_projection_set(mat4)  gfx_view_set(mat4)   // Lengyel's entry point
```

### 7.5 Estratégia de conteúdo — ~~a camada de compatibilidade com a API do motor de partida~~ (cancelada em 2026-09-12)

> **Esta seção é história, não plano.** A camada `compat_layer/` não vai ser feita: o motor de
> partida foi a inspiração, não o objetivo, e este motor não precisa rodar os mods de outra pessoa
> (M11 no §8). Ela continua escrita porque a análise abaixo é o motivo de a API nativa ter a forma
> que tem — traits em vez de enums, UI declarativa, chunks de 32³ — e porque o custo que ela apontava
> é real e agora está sendo pago: **o conteúdo é escrito aqui**.
>
> O que o motor concretamente precisava fazer para a camada foi feito e vale por si só: `param2` como
> trait, drawtypes nodebox+mesh, agendador de ABM/LBM. O resto — parser de formspec,
> `[combine`/`[colorize`, VoxelManip, aliases de mapgen, backend LuaJIT — não vai ser feito.

Com o objetivo "clone completo do clássico jogo de blocos" decidido, a paridade de conteúdo é **o**
problema. Dois caminhos, e só um se sustenta para uma pessoa sozinha:

| Caminho | Custo | Resultado |
|---|---|---|
| Reescrever o conteúdo do zero | anos-pessoa | O projeto morre antes |
| ~~Rodar os mods do motor de partida no motor novo~~ | 8-12 semanas | Os jogos existentes, toda a biblioteca de mods **e as 9.700 linhas de Lua já escritas** rodam com mudanças mínimas |

**Como**, sem sujar a arquitetura nova: a camada de compatibilidade é **um mod em Lua**, não código
do motor. O motor expõe sua API nativa limpa (§6); `compat_layer/` é um pacote Lua que monta a
tabela legada/`core` em cima dela. Ele roda no **backend LuaJIT** (§5.3): os mods existentes do
motor de partida são Lua 5.1 e usam `setfenv`/`getfenv`/`loadstring`, que não existem no Luau.

```lua
-- compat_layer/init.lua (sketch)
local M = {}
function M.register_node(name, def)
    core.node.register(name, translate_def(def))   -- drawtype/nodebox/param2 → trait
end
function M.register_abm(def)  core.sched.abm(def)  end
function M.show_formspec(pl, fid, spec)  core.ui.show(pl, formspec_parse(spec))  end
rawset(_G, "legacy", M); rawset(_G, "core", M)
```

O que o **motor** precisa fornecer para a camada ser fina (e não uma reimplementação disfarçada do
motor de partida):

- `param2` de 8 bits por nó, com a semântica `facedir`/`wallmounted` como **trait padrão**;
- drawtypes `nodebox` e `mesh` com `node_box`/`selection_box` (14 nodeboxes só no `yaportal`);
- agendador de ABM/LBM (ticks por área com probabilidade/intervalo, e um hook "ao carregar o
  chunk");
- metadados tipados por nó e por item;
- um equivalente do VoxelManip (acesso bruto aos blocos para o mapgen dos mods);
- **um parser de formspec** — a parte mais ingrata: uma linguagem de strings, mais de 20 elementos.
  Cobre-se o subconjunto real (button/field/label/box/dropdown/textlist/checkbox/list/item_image +
  `formspec_version[4]`), não a gramática inteira;
- mídia por nome com os modificadores `[combine` e `[colorize` (o mostrador do relógio os usa em
  tempo de execução).

**Custo honesto:** ~60-70% da API padrão do motor de partida. Não é de graça, e é preciso dizer com
clareza: **seguir este caminho significa herdar algumas restrições de projeto do motor de partida**
(semântica de `param2`, formspec, mapblocks de 16³). A mitigação é mantê-las confinadas na camada: o
núcleo novo continua livre (chunks de 32³ por dentro, UI declarativa nativa, traits em vez de enums),
e a camada de compatibilidade traduz.

**Ordem recomendada:** a camada NÃO é feita cedo. Ela é feita no M11, depois que a API nativa se
assentar num jogo de verdade (M7): uma camada de compatibilidade escrita sobre uma API instável é
reescrita duas vezes.

> Essa ordem se manteve até o fim: a API se assentou no M7, M8 e M9, e a escolha de não fazer a
> camada veio antes de escrever uma linha de compatibilidade. O que se perdeu foi zero trabalho; o que
> se ganhou foram três recursos do motor que a camada teria pedido e que o jogo usa de qualquer jeito.

---

## 8. Plano de implementação

Esforço: um único desenvolvedor em tempo integral com assistência de IA. "sem." = semanas.

### M-1 — Fechar o fork direito antes de congelá-lo (1 sem.) · **obrigatório**

O fork vai ser congelado: se não for fechado direito, não sobra nada funcionando para onde voltar.
Antes do congelamento:

- Uma release com tag e reproduzível (AppImage + Flatpak, `BUILD_NUMBER` incrementado) do último
  estado bom, com as builds já no repositório.
- Cada bug aberto escrito no `TODO.md` com sintoma, reprodução e hipótese — não na memória de quem
  trabalha nele.

  > **Corrigido no M0:** este ponto citava o `BUG-10` ("moved too fast" depois da troca) como aberto,
  > mas o `TODO.md` do fork o dá como **resolvido desde 2026-07-28**. No momento do congelamento
  > estavam abertos `BUG-1..BUG-6` e `BUG-9`; resolvidos `BUG-7`, `BUG-8`, `BUG-10`, `BUG-11`. A
  > mitigação com *grace token* descrita no §3.4 continua válida como projeto para o motor novo.
- Commit e **push para os dois remotos** de todo o trabalho ainda não enviado (motor `portal-fork` e
  o repositório externo), submódulo alinhado. Hoje há commits locais não enviados: congelar com eles
  fora de sincronia é o mesmo que perdê-los.
- `README.md` com uma linha: estado = congelado, onde o trabalho continua.

**Saída:** `git status` limpo nos dois repositórios, tag no remoto, artefatos para download.

### M0 — Spike de validação (3 sem.) · **KILL SWITCH**

O único objetivo é derrubar as três hipóteses arriscadas, com código descartável.

- Uma janela raylib **já atrás de `gfx_*`** (§7 — a interface nasce aqui, não depois), um chunk de
  32³ com mesh feita à mão, movimento FPS com colisão AABB.
- Um portal: RT, câmera virtual com a transformação do `PORTAL_SYSTEM.md` §1.3, a matriz de Lengyel
  via `rlSetMatrixProjection`, 5 planos de corte via `gl_ClipDistance` + `glEnable` declarado à mão,
  um quad de túnel com UVs em espaço de tela.
- **Luau** integrado com um punhado de bindings gerados, um mod que coloca blocos e move um portal
  em tempo de execução.

**Critérios de saída (os três, ou para aqui):**
1. Travessia visualmente contínua — nenhum frame mostrando a geometria entre a vcam e o portal.
2. O portal continua correto com a câmera *dentro* da moldura (o caso que quebra Lengyel).
3. ≥ 200 FPS com 2 portais em 1080p no hardware de desenvolvimento, com o custo do binding Luau
   medido e abaixo de 1% do frame.

### M1 — Núcleo voxel do cliente (6 sem.)
Armazenamento paginado de chunks de 32³, greedy meshing em workers, formato de vértice compacto,
texture array (shim GL), luz de sol+bloco por flood fill, AO por vértice, frustum + cave culling,
colisão AABB, raycast para seleção, colocar/quebrar.
**Teste:** um mundo estático de 512³ a 60 FPS, memória estável em 10 minutos de voo.

### M2 — Scripting e registro (5 sem.)
Interface `script_*` + **backend Luau** (uma VM por mod, orçamento de interrupção, cota de memória,
capacidades vindas do manifesto), um gerador de bindings a partir de uma tabela descritiva (cola C++
+ `.d.luau` + documentação), um registro orientado a dados para blocos/itens/entidades com hot
reload, barramento de eventos tipado, hooks de tick. O backend LuaJIT estava previsto para o M11:
esse marco foi cancelado (§8) e o runtime continua sendo um só.
**Teste:** um mod `--!strict` que adiciona 20 blocos e um item, recarregado a quente sem reiniciar;
um mod em laço infinito é interrompido sem parar o servidor.

### M3 — Servidor e rede (5 sem.)
Núcleo sem janela, ENet + canais, handshake versionado com negociação de recursos, streaming de
chunks comprimidos com interest management, replicação de entidades, autoridade sobre o movimento,
SQLite, mapgen determinístico com etapas registráveis.
Dimensionado para **2-8 jogadores por mundo** (decidido): sem sharding, sem arquivos de região,
anti-cheat leve (limite de velocidade + distância de interação + inventários no servidor).
**Teste:** 4 clientes na LAN, 30 minutos, sem dessincronia; um servidor sem janela no CI rodando
1000 ticks e conferindo um hash do mundo.

### M4 — Portais completos (5 sem.)
Port da lógica do `yaportal` (geometria, transformação, gatilhos, máquina de estados por jogador,
passagem pelo chão, roll, teletransporte atômico), pool de RTs com recursão, ordenação de
transparentes por vcam, cam hint, céu/neblina através do portal.
**Teste:** o conjunto de casos do `PORTAL_SYSTEM.md` — horizontal↔vertical, meio deslocamento,
jogador dentro da moldura, portal atrás de vidro, teletransporte dentro do frame (câmera saltando
≥ 4 nós entre o passe de RTT e o passe dos quads) e **dois portais olhando um para o outro em
profundidade 2** — o que o fork não consegue fazer hoje.

> **Fechado.** Cada cláusula tem um gate no `ctest`: `portal_cases` para meio deslocamento, câmera
> dentro da moldura, vidro e salto dentro do frame; `portal_orientation` para horizontal↔vertical;
> `portal_recursion` para a profundidade 2 — a linha que o fork não conseguia fazer. Tabela cláusula
> por cláusula no `README.md`, medições em `docs/archive/M4_FINDINGS.md`.

### M5 — Multissessão local (3 sem.)
Duas `Session`s no mesmo processo (dois mundos locais), uma ativa e uma passiva desenhada no portal,
troca no lugar com todas as invariantes do §4.3.
**Teste:** entrar e sair dez vezes seguidas; céu, teclas seguradas, avatar próprio e distância de
visão corretos em cada volta (os quatro bugs históricos).

> **Fechado.** `dual_session` faz as dez voltas e confere os quatro bugs em cada uma. Medições em
> `docs/archive/M5_FINDINGS.md`.

### M6 — Cross-server (4 sem.)
Chaves de identidade, tickets assinados, escopos OBSERVE/ENTER, heartbeat de estado, alternativa por
redirecionamento, mídia endereçada por conteúdo com cache compartilhado, ids de conteúdo por sessão.
**Teste:** dois servidores em máquinas diferentes, com *jogos* diferentes; travessia sem tela de
carregamento; uma passiva apontando para um servidor morto não enche o log (a regra do backoff).

> **Fechado, com um limite declarado.** `bad_link` faz o cenário inteiro com 400 ms e 20% de perda
> entre dois servidores com *jogos* diferentes, e insiste que a travessia aconteceu de verdade;
> `dead_world` mede a regra do backoff (2 linhas em 26 s contra 28108 sem ela); `two_clocks` faz os
> dois relógios de parede divergirem. "Máquinas diferentes" está coberto para MTU, fragmentação, NAT,
> um servidor com dois nomes e dois relógios — `tools/two_machines.sh` em contêineres para os
> primeiros, um `LD_PRELOAD` para o último. **Não** coberto: um kernel separado e disputa real de
> CPU, que ficam para uma VM. Medições em `docs/archive/M6_FINDINGS.md`.

### M7 — Jogo mínimo (8 sem.)
Inventário, crafting, UI (microui/Clay guiada por Lua), HUD, vida, drops, contêineres, mapgen com
biomas, ciclo dia/noite, áudio com atenuação, mobs básicos com ECS.
**Teste:** uma partida de 30 minutos desde o começo, sem travar e sem tocar no console.

> **Fechado, com quatro limites declarados.** `tests/long_game.sh` faz os trinta minutos — 7,9
> milhões de frames, **zero correções**, RSS estável em +1,1 MB, 224 blocos quebrados e 224 postos de
> volta, oito criaturas mortas, um dia e meio de céu. A primeira execução deu vermelho e encontrou um
> ponto de spawn que só existia com `--portals`, então um jogador que saía do mundo caía para sempre.
> **Não** coberto: é um *script* andando, não uma pessoa (ninguém toca no console, e ninguém joga de
> verdade); as criaturas acabam depois de um minuto e meio porque no M7 não há surgimento de mobs; o
> mundo não tem aberturas, então a travessia continua sendo a do `bad_link`; e ele precisa de uma
> janela, então numa máquina sem tela este teste não roda. Medições em
> `docs/archive/M7_FINDINGS.md` §33.

### M8 — ECS e API de mecânicas (4 sem.)
Componentes/sistemas a partir de Lua com iteração em lote, fases ordenadas, traits nos nós, passes
de renderização registráveis, canais de rede por mod.
**Teste:** reescrever os blocos clássicos dos quebra-cabeças com portais (botão, dispenser, pedestal,
relógio) **só** como mods, sem tocar no motor — é a prova de que a extensibilidade é real.

> **Fechado quanto ao seu teste de saída, com o resto do marco declarado aberto.**
> `tests/portal1.sh` faz os quatro blocos clássicos dos quebra-cabeças com portais como mods e nada
> mais: o botão afunda sob quem fica em cima e volta a subir, o sinal passa de um mod para outro e o
> ventilador solta um cubo, o pedestal pega o que lhe oferecem e devolve a quem chega perto, o
> relógio conta e, depois de fechar e reabrir o mundo, recomeça do número em que tinha parado.
> Catorze verificações, sem janela, e a última é um `grep`: **nenhum arquivo sob `src/` cita um
> bloco, um item ou um evento dos quatro**.
>
> **E depois fechado por inteiro.** O que o teste de saída não tocava — os quatro blocos queriam
> estado por célula, não entidades próprias — foi escrito depois, com a mesma forma: o gate antes do
> código, `DISABLED` até ficar verde. `tests/sentry.sh` (sem janela, 16 verificações) faz
> **componentes e sistemas a partir de Lua com iteração em lote** (512 entidades, uma chamada),
> **fases ordenadas** por um número e não pela ordem de carregamento, um **canal de rede por mod** (o
> campo existia no handshake desde o M3 e ninguém o usava; protocolo 20 → 21) e `find_nodes`, o
> equivalente ao LBM do motor de partida. `cnp_client --pass-check` (6 verificações, gpu) faz os
> **passes de renderização registráveis**: um passe roda dentro da renderização e uma vez por câmera,
> então o que um mod desenha também é visto através de um portal.
>
> **Continua aberto e declarado**, no `TODO.md`: um sistema cita um componente só (sem junção entre
> dois), as entidades de um mod não são visíveis para os outros jogadores, e elas vivem no mundo da
> tabela de sessões — então uma execução offline não tem nenhuma. Medições em
> `docs/archive/M8_FINDINGS.md`.

### M9 — Desempenho e robustez (4 sem.)
Meshing multithread com orçamento, occlusion culling, LOD distante, reverse-Z, profiler no jogo,
tratador de falhas, migração de salvamentos.
**Teste:** três gates, escritos antes do código como os do M8. `frame_budget` (gpu) mede quanto
custa um frame enquanto o mundo se mexe, num mundo de 512 nós — 512 chunks, não os 32 dos outros
gates — em três fases: parado, um bloco por frame e tudo sujo de uma vez. Os números são os do
jogador, e não os que o motor produz: 16,7 ms é um sexagésimo de segundo, 50 ms é um engasgo
visível, e uma câmera enterrada na rocha precisa desenhar o que está em volta dela, e não o mundo
atrás da rocha. `crash_evidence` exige que um processo que morre diga qual sinal, deixe uma pilha e
**morra do mesmo jeito** — um tratador que engole o SIGSEGV é pior do que nenhum tratador.
`migration` exige que um arquivo de mundo escrito por uma build anterior abra e mantenha o que
havia dentro, e que um de uma build posterior seja recusado pelo nome.

> Esta linha não existia: o M9 era o único marco do plano com uma lista de recursos e nenhum número.
> Ela foi escrita antes de começá-lo, porque o M7 fechou com o teste de saída escrito por último, e a
> primeira execução encontrou vinte e dois minutos de um jogador caindo para fora do mundo.
>
> **Fechado pelos seus três gates.** `migration` (14 verificações): a versão é lida **antes** de o
> esquema tocar no arquivo, porque uma abertura recusada não pode alterar o arquivo que recusou.
> `crash_evidence` (5): o sinal é nomeado, a pilha é escrita e o processo **morre do mesmo jeito** —
> em `sigaltstack`, porque o jeito mais comum de levar um SIGSEGV é ficar sem pilha. `frame_budget`
> (21): um mundo de 512 chunks se reconstruindo inteiro custa **4,23 ms** no pior frame, contra os
> 16,7 do orçamento, e se recupera no frame 130 em vez de nunca.
>
> **O profiler decidiu o resto do marco.** Sua propriedade é aritmética — tudo o que tem nome mais
> tudo o que não tem **é** o frame — e o primeiro número que ele produziu diz para onde vai um chunk:
> `snapshot` 0,32 ms, `build` 3,12, `upload` 0,00. 90% é a parte que só lê um snapshot e o registro,
> a única que pode sair da thread. Sem essa linha, a escolha entre threads e LOD teria sido uma
> opinião.
>
> **O occlusion culling é uma caminhada, não um teste**: quinze bits por chunk dizendo quais faces
> se alcançam, e o renderizador só entra num vizinho se pudesse atravessar aquele de onde vem. Câmera
> fechada na rocha: **4 chunks em vez de 129**. Diante de uma parede: **2 em vez de 154, e nem um
> pixel diferente** — que é a única verificação capaz de pegar a falha que importa, um buraco no
> mundo.
>
> **Continua aberto e declarado**, em `docs/archive/M9_FINDINGS.md`: **o LOD distante e o reverse-Z
> não foram escritos**, porque eram meios e o orçamento não pediu por eles — a hora de fazê-los é
> quando um número pedir, não antes. E a caminhada tem a granularidade de um chunk: num mundo de dois
> chunks de altura, a partir da superfície, ela descarta zero. **57 testes no `ctest`, todos verdes,
> e pela primeira vez nenhum `DISABLED`.**

### M10 — Distribuição (2 sem.)
Builds estáticas para Linux/Windows, AppImage/Flatpak (reaproveitando os scripts existentes),
formato de pacote de mod + versionamento, documentação da API.

### M11 — Camada de compatibilidade com a API do motor de partida · **CANCELADO em 2026-09-12**

Era a camada `compat_layer/` com o segundo backend LuaJIT: dez semanas para rodar os mods e jogos do
motor de partida em cima deste motor. **Não vai ser feito.** O motor de partida foi a inspiração, e
o fork anterior é de onde vem a experiência: ele não é o objetivo, e este motor não precisa rodar os
mods de outra pessoa.

**O que sobra desse marco, e já está feito.** O M11 pedia ao motor três coisas que valem por si só e
que, de fato, foram construídas antes de a escolha cair:

- `param2` facedir/wallmounted como **trait** — TODO.md CLOSED-27, um byte na célula;
- **agendador de ABM/LBM** — CLOSED/OPEN-42, `core.register_abm` e `core.register_lbm`;
- **drawtypes nodebox + mesh** — CLOSED-43, as caixas que param um corpo e um bloco desenhado como
  modelo.

Nenhuma das três é uma peça de compatibilidade: são um bloco com uma frente, um mundo onde algo
acontece sem ninguém tocar numa célula, e um bloco que não é um cubo. Elas ficam.

**O que cai junto:** o parser de formspec, os modificadores de textura `[combine`/`[colorize`, o
VoxelManip, os aliases de mapgen, a tradução de `register_node`/`register_entity` e o segundo
backend LuaJIT (§5.3), cujo primeiro motivo era rodar Lua 5.1 legado.

**E o que isso custa, dito e não escondido:** o §7.5 dizia que reescrever o conteúdo do zero vale
anos-pessoa e é o jeito como este projeto morre. Essa frase continua verdadeira. A diferença é que
agora o conteúdo é uma escolha de *escopo*, e não uma dívida com o catálogo de outra pessoa — ver o
M12.

### M12 — Conteúdo próprio (contínuo)

O conteúdo é escrito aqui, como mods deste motor e com a API dele. O que já está em `mods/` é a prova
de que o caminho existe: blocos com forma e dureza, ferramentas que contam, baús, crafting, ABMs, um
mapgen com etapas, entidades com componentes — tudo sem tocar no motor.

**O escopo está decidido, 2026-09-12.** O jogo é um clone do clássico jogo de blocos de onde portais
levam a outros mundos, e esses mundos podem ter mods diferentes — até mundos que são outro jogo
(M13). A frase do §7.5 continua verdadeira e precisa ficar à vista: a *paridade* de conteúdo com o
clássico jogo de blocos vale anos-pessoa, e sem o conteúdo de outra pessoa não há atalho. O que este
escopo tem e o "clone completo" não tinha é a forma: nem tudo precisa estar no sandbox, porque o
resto pode ficar atrás de um portal, em outro mundo, com seus próprios mods. O conselho deste
documento é medir o M12 por partida e não por lista — um sandbox que aguenta uma partida, como o M7
mediu em trinta minutos — e não correr atrás da lista de recursos do clássico jogo de blocos.

### M13 — Mundos com um modo de jogo próprio (estimativa aberta)

Decidido junto com o escopo (§0). **Um modo de jogo é um mundo**: um servidor com seus próprios mods,
alcançado por um portal a partir do sandbox, e não um modo dentro do sandbox. É a leitura que usa o
que este motor já tem — sessões por servidor, mídia endereçada por conteúdo, formas e modelos
viajando no welcome — e o fork já fez metade desse caminho: o `yafps/` dele é uma arena FPS mínima
"built as a cross-world portal destination", **552 linhas em cinco mods** (armas 178, alvos 114,
núcleo 114, arena 83, HUD 63). É o critério natural, como o `yaportal` foi no §6.7.

**Teste:** a partir do sandbox, um portal para um servidor rodando o `yafps` **reescrito como mod
deste motor**, e outro para um servidor com **uma pista cronometrada** — largada, checkpoints,
chegada, tempo na tela. O cliente que atravessa **não instalou nenhum dos dois mods**. Joga-se uma
partida e uma volta, e volta-se pelo mesmo portal. Mais um `grep` como o do M8: nenhum arquivo sob
`src/` cita uma arma, um veículo ou um checkpoint.

**O que falta hoje, lido no código e não estimado:**

1. **De um mundo chegam ao cliente a aparência e a forma, não o comportamento.** O cliente carrega
   mods do seu próprio diretório (`src/client/main.cpp`, `--mods` e `load_dir`). Pelo fio viajam os
   nomes dos blocos, as formas e a orientação (protocolo 27), os modelos e a mídia. Não viajam se um
   bloco é sólido, sua dureza, seu nível de escavação (TODO.md OPEN-43), nem uma linha de Luau. Um
   modo de jogo vive em boa parte no cliente — o tempo ou a munição na tela, a resposta de um tiro, a
   previsão de um veículo, um `core.render.pass` — e o §5.6 diz que **nenhum mod baixado do servidor
   é executado no cliente**. É a decisão aberta deste marco, logo abaixo.
2. **O movimento é um número do motor.** `core/physics/movement.h`: andar 4.8, correr 8, voar 14,
   pular 8, gravidade −25. O validador do servidor aceita na horizontal no máximo
   `kFlySpeed × tolerance` = 14 × 1.6 = **22.4 m/s**, fora do segundo de tolerância de um portal.
   Nenhum mod pode mudar esses números, nem por mundo nem por corpo, então um carro mais rápido que
   um jogador voando é trapaça para este servidor. O `yafps` usava `set_physics_override` e
   `set_velocity`. São necessários limites declarados pelo mundo e pelo corpo, enviados no welcome
   como as formas e lidos pelo validador, e um corpo que não seja um jogador a pé. O servidor verifica
   o movimento e não o simula (o `movement.h` diz isso), e para uma corrida é preciso medir se isso
   basta antes de decidir que não basta.
   **Primeira peça feita, 2026-09-13** (TODO.md OPEN-44): `core.movement` declara andar, correr,
   voar, pular e gravidade **por mundo**, os números viajam no welcome (protocolo 28) e o validador
   tira seus limites dali — a 60 m/s o gate recusa 0 claims de 98 no mundo que os declarou e 14 de 99
   num mundo sem mods. Faltam os limites **por corpo** e um corpo que não seja um jogador a pé.
3. **O cliente escolhe o alvo.** TODO.md OPEN-15: o cliente lança o raio, o servidor só confere se o
   corpo nomeado está dentro de `place_reach` = **7 nós**, e um golpe a cada `kHitCooldown` =
   **0.4 s**. O fuzil do `yafps` atira a **60 nós a cada 180 ms**, com um raycast no servidor, e o
   comentário dele diz por que não é preciso compensar a latência: *todos os mundos rodam nesta
   máquina*. Entre máquinas diferentes — que é o que o M6 mediu — essa frase não vale mais.
4. **Ninguém vê as entidades de um mod** (TODO.md OPEN-20), e os alvos e projéteis de um FPS são
   exatamente entidades de um mod.
5. ~~**O mundo é uma caixa** de 128 por 64, gerada inteira (OPEN-35) e nunca descarregada
   (OPEN-38).~~ **Feito em 2026-09-15:** o mundo do servidor não tem borda, as colunas são geradas ou
   relidas quando alguém chega perto e liberadas quando não há ninguém (protocolo 37). Uma arena cabe
   — o `yafps` pedia o mapgen `flat` —; uma pista talvez não, e é preciso dizer isso quando uma for
   desenhada.
6. **Dos defeitos da travessia, um continua aberto**, OPEN-2 (ficar preso no bloco de saída), nunca
   reproduzido. Na entrada de uma arena ou de uma corrida, é a primeira coisa que se sente.

**Decisão tomada, 2026-09-13: o código de um mundo do lado do cliente.** O primeiro dos dois caminhos
abaixo, com uma condição que a recomendação não dizia: **o cliente pode recusar o código de um
mundo**, e esse mundo é jogado do mesmo jeito, sem ele (§5.6). Os dois caminhos, como tinham sido
postos:

- **Luau enviado pelo servidor**, numa VM por mod e por sessão, com capacidades só de cliente —
  desenhar, HUD, entrada, o canal do próprio mod — e sem arquivos nem rede. Exige reescrever a linha
  do §5.6. É para isso que o Luau foi escolhido (§5.3: uma sandbox *projetada* para código não
  confiável), e é o único caminho em que um mundo novo não pede um motor novo.
- **Primitivas do motor guiadas por dados** — HUD declarativo, parâmetros de movimento, veículo, arma
  hitscan. Nenhum código de fora no cliente, mas cada modo que não cabe ali pede uma nova release do
  motor a todos os jogadores: o contrário de "mundos com mods diferentes".

A recomendação é o primeiro, com a autoridade ficando onde está: o cliente prevê e desenha, o
servidor decide — limites de movimento e o raio de um golpe são números que o servidor verifica de
qualquer forma, seja qual for o código que roda do outro lado.

**Totais:**
- Portais entre servidores funcionando (M-1..M6): **32 semanas (~7 meses)**
- Motor + jogo mínimo independente (…M10): **49 semanas (~11 meses)**
- Conteúdo próprio (M12): **contínuo**. A paridade com o clássico jogo de blocos continua em 2-3
  anos, e é por isso que o critério é uma partida e não uma lista.
- Mundos com um modo próprio (M13): **ainda sem estimativa**. A decisão sobre o código do lado do
  cliente está tomada (2026-09-13); a maior peça que vem dela é a sandbox do cliente, e nada foi
  medido que diga quanto ela custa.

### Organização durante o trabalho

- O fork do motor de partida fica **congelado** depois do M-1: nenhum trabalho novo, só a tag e a
  documentação dos bugs abertos. (Ciente do risco: por ~14 meses não há produto vivo.)
- O motor novo vive em **`/home/giacomo/dev/Cubes and portals/`**, com repositório git próprio, não
  dentro do fork anterior. Estrutura prevista:

```
Cubes and portals/
  docs/DESIGN.md          ← this document
  src/core/               ← voxel, ECS, mapgen, protocol (never #include <raylib.h>)
  src/client/gfx/         ← gfx_* + raylib backend (then possibly sokol)
  src/client/             ← sessions, render graph, portals, UI
  src/server/             ← headless
  src/script/             ← script_* + Luau backend (then luajit)
  mods/                   ← base mods in .luau
  third_party/            ← raylib, luau, enet, sqlite, zstd, fastnoiselite, microui
```
- O `docs/PORTAL_SYSTEM.md` e as invariantes do `AGENTS.MD` §461 viram a **especificação de testes**
  do motor novo: cada invariante = um teste automático. É o jeito de não comprar os mesmos bugs de
  novo (céu branco na segunda volta, conexão passiva dupla, teclas perdidas na troca, avatar
  invisível para si mesmo, identidade pelo endereço em vez de pelo mundo).

---

## 9. Riscos e decisões abertas

| Risco | Prob. | Impacto | Mitigação |
|---|---|---|---|
| O projeto encalha no conteúdo | **Alta** | Fatal | A mitigação era o M11 (rodar os jogos existentes), **cancelado em 2026-09-12**. O escopo fixado no mesmo dia não é menor: um clone do clássico jogo de blocos mais mundos com modos de jogo próprios. A mitigação que sobra é a forma — cada modo é um mundo pequeno atrás de um portal, e o M12 é medido por partida e não por lista de recursos. Ela é mais fraca do que a que existia, e é preciso dizer isso |
| Um modo rápido contra um validador pensado para quem anda | **Certa** | Alto | Hoje não é um risco, é um fato: 22.4 m/s na horizontal, um golpe dentro de 7 nós a cada 0.4 s. M13, pontos 2-3: limites declarados pelo mundo e verificados pelo servidor, raio lançado pelo servidor |
| Código de cliente enviado pelo servidor | Média | Alto | **Escolhido em 2026-09-13.** Luau, uma VM por mod e por sessão, capacidades só de cliente, sem arquivos, rede ou processos, cota e interrupção como no servidor, autoridade com o servidor; e o jogador pode recusar, para todos os mundos ou para um (§5.6) |
| Fork congelado = nenhum produto vivo por ~14 meses | **Alta** | Alto | M-1 obrigatório: release com tag, bugs documentados, tudo enviado. O fork continua *descongelável* numa tarde |
| Cansaço / bus factor 1 | Alta | Fatal | Marcos que produzem algo jogável toda vez; nada de big bang |
| raylib insuficiente no M9 | Média | Médio | Interface `gfx_*` fina → migração para o sokol em dias |
| Sandbox da FFI furada | Média | Alto | Capacidades no manifesto, FFI desligada por padrão, nenhum mod de cliente vindo do servidor |
| Cross-server abusado (scanners de mundos) | Média | Médio | Escopo OBSERVE, tickets que expiram, limite de taxa |
| Reescrita mais lenta do que o planejado (sempre) | **Alta** | Alto | Kill switch no M0; revisão do escopo no M4, no M6 e no fim do M10 |
| ~~O M11 traz de volta restrições de projeto do motor de partida~~ | — | — | Não se aplica mais: a camada não vai ser feita, e as restrições que ela herdaria (semântica de `param2`, formspec, mapblocks de 16³) ficam de fora |

**Decisões tomadas** (§0): fork congelado depois do M-1 · raylib atrás de `gfx_*` · 2-8 jogadores por
mundo · **sem compatibilidade com o motor de partida** (2026-09-12) · **M10 suspenso** até o jogo ser
declarado terminado e jogável · **escopo: um clone do clássico jogo de blocos com portais para mundos
com mods e modos de jogo próprios** (2026-09-12).

· **um mundo pode enviar código Luau ao cliente, e o cliente pode recusá-lo** (2026-09-13, §5.6).

**Nenhuma grande decisão de projeto continua aberta.** Fica o trabalho: o que o M13 lista, e o risco
nº 1 acima, que o escopo mitiga de forma mais fraca do que o M11 fazia.

---

## 10. Estado e próximos passos

**Estado, 2026-09-12.** M-1 feito (fork congelado e com tag); M0..M3 aprovados e M4..M9 fechados,
cada um com seus próprios limites declarados; M10 suspenso; M11 cancelado; M12 em andamento; M13
aberto e sem estimativa. Os defeitos abertos estão no `TODO.md`, as medições e o histórico em
`docs/archive/` (desde 2026-09-13, quando os testes foram reduzidos a 13 e a documentação reescrita
sobre o estado atual: [`ARCHITECTURE.md`](ARCHITECTURE.pt.md)).

**Próximos passos, em ordem:**

1. ~~Decidir o código de um mundo do lado do cliente~~ — **decidido em 2026-09-13**: Luau vindo do
   servidor, que o cliente pode recusar (§5.6).
2. **Limites de movimento declarados pelo mundo** e lidos pelo validador (M13, ponto 2). **Por mundo,
   feito em 2026-09-13** (TODO.md OPEN-44). A partir daí faltam, do mais barato para o mais caro:
   - ~~o streaming de chunks em velocidade de corrida, medido antes de mudá-lo~~ — **medido em
     2026-09-13** (TODO.md OPEN-45): aguenta até 120 m/s, onde o limite é o raio (128 nós, um
     segundo de estrada); a 256 m/s não aguenta, 64 nós de terreno à frente no mínimo. Não foi
     mudado: espera uma pista que diga a que distância é preciso enxergar;
   - ~~o trajeto de um claim verificado pelo servidor~~ — **feito em 2026-09-13** (TODO.md OPEN-46):
     o servidor refaz o claim com o solucionador do cliente, de cinco maneiras, e o aceita se uma
     delas chega a menos de meio nó; o primeiro claim depois de uma colocação pelo servidor é julgado
     por onde termina. Um trajeto que volta atrás num eixo entre dois claims continua recusado, o que
     a pé não acontece e a 160 m/s acontece;
   - ~~velocidade por corpo~~ — **feito em 2026-09-13** (TODO.md OPEN-47): `core.body_movement` dá a
     um corpo números próprios a qualquer momento, com uma mensagem para esse corpo e um segundo em
     que o servidor aceita o mais rápido entre os antigos e os novos. É a primeira peça de um veículo,
     e não um veículo: sem direção, sem inércia.
3. ~~O golpe verificado pelo servidor e as entidades de um mod visíveis~~ — **feito em 2026-09-13**
   (TODO.md OPEN-15, OPEN-20): o servidor lança o raio na direção do corpo que um golpe nomeia e o
   recusa se houver um bloco sólido no meio; `core.ecs.visible` faz uma entidade de um mod ser
   enviada a todos, como uma *coisa* e não como uma pessoa. No caminho: os ids de corpos no fio eram
   de 32 bits e perdiam a geração (OPEN-48), agora vão inteiros.
4. ~~O código Luau que um mundo envia ao cliente~~ — **feito em 2026-09-13** (TODO.md OPEN-49), com a
   recusa do jogador. O que falta nele está naquela entrada: HUD e entrada a partir do código de um
   mundo, e um gate que o veja rodando numa janela.
5. ~~Um sandbox com um portal para a arena e outro para a pista~~ — **feito em 2026-09-14**
   (protocolo 34): cada abertura tem seu próprio destino. Com o `grep` limpo no mesmo dia, **o teste
   do M13 passa**, à mão, em três servidores locais (TODO.md, "one sandbox, two worlds").

**Estado, 2026-09-14.** O M13 passou no seu teste; o que sobra são os buracos que o teste não olha.
Do mais barato para o mais caro:

6. ~~O kart dirigido à mão~~ — testado à mão (2026-09-14).
7. **A pista como jogo.** ~~Nada de voar por cima dos muros~~ e ~~um corpo da largura do kart~~ —
   **feitos em 2026-09-14** (protocolo 35, `core.body_width`; TODO.md, "the timed track"), e ~~contagem
   regressiva, tempos guardados por nome e comparados~~ no mod da pista, com `core.player_name`. ~~Um
   piloto roteirizado que não tira o kart da área dos boxes~~ — área dos boxes com 13 de largura,
   duas voltas e um retorno testados no jogo (2026-09-14). ~~Tempos perdidos quando o servidor
   reinicia~~ — **guardados desde 2026-09-19** com `core.storage_set`, no arquivo do mundo. **Desde
   2026-09-20 você chega a pé**: o mundo é de andar, os karts ficam estacionados na área dos boxes
   como blocos com o modelo do kart, e um clique direito num deles faz você entrar (`core.body_drive`,
   `core.body_model`, `core.model_id`; F faz você sair e o kart fica onde está -- desde 2026-10-16;
   antes era outro clique direito, que fazia sair sem querer).
8. **Inércia e direção verificadas pelo servidor** (TODO.md OPEN-47). **Decidido em 2026-09-19:** o
   kart é um corpo rígido sobre rodas com Jolt Physics (5.6.0, build determinística), o jogador a pé
   continua no nosso próprio solucionador AABB, com peso no chão e o controle no ar dos
   quebra-cabeças com portais; o movimento fica em `core/physics` (`player_update`,
   `vehicle_update`). O próximo passo: o cliente manda as teclas e o dt, o servidor as repete com o
   mesmo código e corrige onde não bate, no lugar das verificações de velocidade, trajeto e altura. A
   aderência é decidida pelos pneus do Jolt, não por um número de `core.drive`. **Enquanto esse passo
   não existir**, três regras mantêm o kart dentro do que o servidor consegue verificar (TODO.md,
   2026-09-20): o chassi fica a no máximo 26 graus da vertical, as rodas não aceitam como chão nada
   acima de 50 graus — as duas coisas que o faziam deitar numa parede e subir por ela — e "dentro de
   um bloco", para um corpo que dirige, é só o centro, porque a caixa do servidor não é a forma que o
   cliente simula, e a recusa colocava o chassi de volta dentro da parede.
9. Depois, a decisão sobre se o jogo é jogável, que reabre o M10.

---

## Verificação dos fatos citados

Fatos verificados durante a análise, não lembrados:

- raylib 5.5 instalada localmente (`~/dev/raylib/prova FPS/venv`, `raylib-5.5.0.4`): confirmada a
  presença de `rlSetMatrixProjection`, `rlGetMatrixProjection`, `rlLoadFramebuffer`,
  `rlFramebufferAttach`, `rlLoadTextureDepth`, `rlLoadVertexArray`, `rlUpdateVertexBuffer`,
  `rlDrawVertexArrayElements`, `rlSetUniform(Matrix)`, `rlTextureParameters`, `DrawMeshInstanced`,
  filtros anisotrópicos. Confirmada **a ausência** de APIs de texture array.

  > **Atualizado no M0:** o projeto começou da **raylib 6.0** (submódulo `third_party/raylib`, tag
  > `6.0`), não da 5.5. Verificado de novo no header da 6.0: todas as funções acima continuam
  > existindo, as texture arrays continuam ausentes, como esperado, e a convenção de projeção não
  > mudou (RH, profundidade `[-1,1]`, `m11 = -1`). Ausentes, por outro lado, `rlGetRenderBatch` e
  > `rlLoadShaderCode`, que o §2.3 citou por engano. Build verificada: `PLATFORM_DESKTOP`,
  > `GRAPHICS_API_OPENGL_33`, GLFW em X11, raylib estática. Scripting: **Luau 0.733**
  > (`third_party/luau`).
- Fork do motor: 77 arquivos, +6.369/−309 em relação ao upstream v5.16.1 (`5ebd9b579`); medido
  também 90 arquivos / +6.587 contra a base de merge `750ae1956` do `upstream/master`. A base `src/`
  do motor de partida: 214.491 linhas (cliente 40.962, servidor 16.574, rede 7.100, gui 18.692,
  mapgen 11.669).
- O renderizador de portais do fork: `portal.cpp` 1050 linhas (100% novas) + `portal.h` 84 +
  `portal_manager.{h,cpp}` 525 ≈ **1.660 linhas**; hooks em `clientmap.cpp` +182/−17; `sky.cpp`
  +187/−78; shader +12. `game.cpp` +1247/−105, das quais ~880 são as dez funções do cliente duplo e
  `performSessionSwap` (282 linhas sozinha).
- Recursão de portais: **ausente** hoje — o hook dos quads é pulado quando `m_bypass_frustum_culling`
  está ligado (`clientmap.cpp:1281-1283`).
- Cada RTT é feito no tamanho de renderização cheio (`ctx.target_size`, `ECF_A8R8G8B8`) e faz um
  `drawAll()` completo com o culling desligado (`portal.cpp:660-919`); nova renderização dentro do
  frame se a câmera se move ≥ 4 nós (`portal.cpp:1033-1047`).
- Mods: `yaportal/init.lua` 6911 linhas / 289 KB, `yaportal_link/init.lua` 2221 / 94 KB, `yafps/`
  552 linhas + 160 KB de mídia; total de Lua ≈ 9.684 linhas.
- O mod **não usa mod channels**: toda a comunicação extra passa por HTTP + o ambiente inseguro.
- Já existe no sistema um experimento com raylib em Python (`~/dev/raylib/prova FPS`, `pyray` 5.5,
  368 linhas) — útil como referência de ergonomia, não como base.

**Não verificado na máquina (a confirmar no spike M0):** as propriedades do Luau (geração de código
nativo, interrupção/cota de memória, ausência de `setfenv`/`loadstring`), do QuickJS-ng e do WAMR
vêm de conhecimento prévio, não de uma build local. O M0 precisa incluir: compilar o Luau com CMake
ao lado da raylib, e uma medida do custo de uma chamada de script por frame.
- APIs Lua adicionadas pelo fork: `set_portals`, `set_portal_cam_hint`, `clear_portal_cam_hint`,
  `set_portal_xworld_dest`, `set_xworld_target`, `register/update_portal_sky_type`, `set_portal_sky`,
  `redirect_player`, `xworld_swap_player`, `xworld_exchange(_posts)`. Opcodes novos:
  `TOCLIENT_PORTAL_TELEPORT 0x65`, `TOCLIENT_REDIRECT 0x66`, `TOCLIENT_XWORLD_SWAP 0x67`,
  `TOCLIENT_PORTAL_STATE 0x68`, `TOSERVER_XWORLD_STATUS 0x54`.
- Medida de desempenho do cliente duplo (do trabalho do M7 no fork): desenhar o mundo B custa
  **3-7%** do frame; o gargalo é o mundo A.
