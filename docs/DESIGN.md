# Cubes and Portals — a from-scratch voxel sandbox on raylib with native portals

*[Leggi in italiano](DESIGN.it.md)*

> Design document. Feasibility analysis (graphics, server, multi-instance client, scripting,
> extensibility) and a milestone plan. It is not yet a commitment to write code: §8 contains an
> explicit *kill switch* after 3 weeks of spike.
>
> Written on 2026-08-14, starting from the experience of an earlier fork
> (a fork of the starting engine with portals + cross-server dual client, working).

---

## Context

The earlier fork today is: ~9,700 lines of Lua (`yaportal/init.lua` 6911, `yaportal_link/init.lua`
2221, `yafps/` 552) on top of a **fork of the starting engine** (77 files, +6,369/−309 lines on a base
of ~214k lines of C++). Seven of the fork's capabilities are *invisible* to a stock mod and therefore
not upstreamable as a block: RTT from a second camera, oblique near plane, per-vertex
`gl_ClipDistance`, atomic teleport in one packet, camera roll, collision box shift, **a second live
connection** to another server rendered inside the portal.

The cost of this setup is not writing the features: it is **chasing upstream** and fighting
architectures that were not designed for two live worlds in the same process (singletons, global
sky, a CPU-side `ViewArea` that gets dirty if you touch the projection, client identity by address
instead of by world). Since the probability of an upstream merge is ~0, the question is: **is it
worth starting again from an engine of our own, with raylib as the platform layer, designed from day
0 around portals and multi-session?**

This document answers on four axes (graphics, server, multi-instance client, scripting), with
measured pros and cons and a milestone plan.

---

## 0. TL;DR — verdict

| Question | Short answer |
|---|---|
| Are portals with oblique near clipping feasible on raylib? | **Yes, and cleaner than now.** `rlSetMatrixProjection` accepts Lengyel's matrix without touching CPU culling: the fork's `setRenderProjectionMatrix` hack disappears by construction. |
| Are side clip planes feasible? | **Yes.** Our own shaders → `gl_ClipDistance[0..4]`; `glEnable(GL_CLIP_DISTANCE0+i)` is core GL, it only needs declaring (raylib does not expose it but does not block it). `discard` fallback for GLES/Web. |
| A server feasible in pure C/C++? | **Yes.** ENet + zstd + SQLite + FastNoiseLite. No new toolchain, all C libraries. |
| A multi-instance client for cross-server portals? | **Yes, and it is the strongest argument for the rewrite.** Today it works but it was wrenched out of the singletons by force; from scratch it is "a `struct Session`, no globals". A measure already in hand: **rendering world B costs only 3-7% of the frame** — multi-session is not the bottleneck. |
| Scripting language? | **Luau** (a modern Lua: gradual types, native codegen, a sandbox *designed* for untrusted code) behind a `script_*` interface. **LuaJIT** was planned as a second backend for the starting engine's legacy mods and for trusted code with FFI: with compatibility dropped (2026-09-12), a single runtime remains and FFI is not planned. Also evaluated QuickJS-ng, WASM, Wren, Umka, C#: §5. |
| Extend the *mechanics*, not just the blocks? | Server-side ECS with components/systems defined from Lua + traits on nodes + a render graph with registrable passes + per-mod protocol channels. §6. |
| **Real cost** | Validation spike ~3 weeks. A playable client+server with cross-server portals: **~7 months**. Engine + minimal game: **~11 months**. Content parity in the style of the classic block game: **years**, unless §7.5. |
| **Recommendation** | Spike M0 as a gate. On content the recommendation was the **compatibility layer with the starting engine's API** (§7.5), **cancelled on 2026-09-12**: the starting engine was the inspiration, not the goal. The uncomfortable half of the sentence stays true — a project like this dies on content and not on graphics. **Scope fixed the same day:** a clone of the classic block game from which portals lead to other worlds, each a server with its own mods, up to worlds that are another game — a stunt race, an arena-style FPS. It is no smaller than the scope §7.5 called fatal; what keeps it standing is that every mode is a small world behind a portal and not a piece of the sandbox (M12, M13). |

### Decisions taken (they fix the scope of this plan)

| Decision | Choice | Consequence in the plan |
|---|---|---|
| Goal | **A clone of the classic block game, with portals to other worlds with their own mods — up to different game modes (stunt race, arena-style FPS)** (2026-09-12) | §7.5 is cancelled, so the sandbox's content is written here (M12) and parity stays years away. Every mode is a server with its own mods, and asks the engine for things a sandbox does not: a world's behaviour reaching the client, fast movement, ranged hits (M13) |
| Fork of the starting engine | **Frozen** | No live product during the rewrite → M-1 mandatory (tagged release + documented bugs before the freeze) |
| Graphics layer | **raylib behind `gfx_*`** (~30 functions) | Migration to `sokol_gfx` possible in days if needed in M9 |
| Multiplayer | **2-8 players per world** | SQLite is enough, light anti-cheat, simple interest management (−1 week on M3) |
| Scripting | **Luau**, single runtime | Sandbox and types from the runtime; binding cost paid with a generator (§5.4). The second LuaJIT backend fell with the starting engine's compatibility (§5.3) |
| Location | **`/home/giacomo/dev/Cubes and portals/`** | New, independent repo; the earlier fork is not touched except for the freeze |

---

## 1. Starting point — what exists and how much it weighs

| Component | Size | Reusable in a new engine? |
|---|---|---|
| `yaportal/init.lua` | 6911 lines of Lua | **Yes, ~70%.** Portal geometry, transform, triggers, state machine, the classic blocks of portal puzzles. Only the `core.*` calls change. |
| `yaportal_link/init.lua` | 2221 lines of Lua | **Yes, ~50%.** The JSON file registry and pairing logic survive; the handshake is redesigned (see §4.4). |
| `yafps/` (a whole game) | 552 lines + 160 KB of media | **Yes.** And it shows that a *game* here costs 5 mods and half a day — an important data point for §7. |
| The fork's portal renderer | `portal.{cpp,h}` + `portal_manager.{cpp,h}` ≈ **1660 lines**, + ~250 of hooks in `ClientMap`, ~240 of `Sky`, 12 of shader | **To rewrite**, but it is the only truly specific graphics piece: 2,000 lines, not 200,000. |
| Rest of the fork (dual client, protocol, mapnode, Lua API) | ~4,500 lines | **Not as code, yes as knowledge.** The invariants of `AGENTS.MD` §461 are worth more than the code: they are bugs already paid for. |
| `docs/PORTAL_SYSTEM.md` | 1366 lines + 9 SVG figures | **Yes**: it is the new engine's specification, already written, down to the formulas. |
| Starting engine | ~214k lines of C++ (+ Irrlicht) | Replaced. A focused engine needs ~40-60k to do what we need. |
| Existing games (content) | huge | **Lost.** It is the hidden cost of the rewrite. |

**Two limits of the current implementation a new engine would remove by construction** (measured,
not assumed):

- **No recursion.** The portal inside the portal does not exist today: the hook drawing the quads
  is skipped during any RTT pass (`clientmap.cpp:1281`). Maximum depth = 1.
- **Every portal costs a whole scene at full resolution.** Every active slot does a full
  `drawAll()` with frustum culling disabled, at the screen's rendering size, with no LOD, no reduced
  resolution, no visibility graph deciding which portals really matter.

**A number to keep in mind:** `docs/PORTAL_SYSTEM.md` already exists and is detailed down to the
formulas (Lengyel's matrix inverted in closed form, apex standoff `0.05·BS`, plane 4 on the node's
*outer* face, clamp `n·bn < 0`). A rewrite does not start again from theory: it starts again from
implementation, theory in hand. This cuts maybe 40% of the graphics risk.

---

## 2. Graphics feasibility on raylib

### 2.1 What raylib gives, what it does not

Checked on **raylib 5.5** (headers of the local install in `~/dev/raylib`, CPython binding).

**It gives (and saves you months):** window/input/gamepad via GLFW or SDL2, `rlgl` as a GL 3.3 core /
GLES 2-3 abstraction, image and font loading, audio (miniaudio), maths (`raymath`), meshes +
`UploadMesh`/`DrawMesh`/`DrawMeshInstanced`, `RenderTexture2D`, full custom shaders, and — crucially
— **low-level access**: `rlLoadFramebuffer`, `rlFramebufferAttach`, `rlLoadTextureDepth`,
`rlLoadVertexArray`, `rlUpdateVertexBuffer`, `rlDrawVertexArrayElements`, `rlEnableShader`,
`rlSetUniform`, `rlSetUniformMatrix`, `rlSetMatrixProjection`/`Modelview`, `rlTextureParameters`,
`rlSetCullFace`, `rlColorMask`, `rlDisableDepthMask`.

Translated: **raylib does not get in your way**. When "real" GL is needed, `rlgl` hands you the GL
object ids and lets you handle them.

**It does not give (and you must write it):**

| Missing | Impact | Remedy |
|---|---|---|
| No scene graph / frustum culling / occlusion | High — it is *the* work of a voxel engine | You write it, and better so (§2.6) |
| No texture array (`GL_TEXTURE_2D_ARRAY`) | Medium | ~60 lines of GL shim, or an atlas with padding + manual mips |
| Single-thread GL | Medium | Meshing on workers → CPU buffers → upload on the GL thread (as the starting engine does) |
| No shadow maps / shadows | Medium | You write them, or start without shadows (the classic block game has none). Written on 2026-10-11: see ARCHITECTURE.md, "How light is drawn" |
| No spatial 3D audio (only pan/volume) | Low | Attenuation+pan by hand, or miniaudio directly (already linked) |
| No nested `BeginTextureMode` | Low | Not needed: portal recursion is done **from deepest to shallowest** on a pool of RTs (§2.5) |
| No declarative UI | Medium | `microui` or `Clay` (C, MIT) + raylib backend; formspec-like driven by Lua |
| No network, no physics, no storage | High but expected | ENet / own AABB / SQLite (§3) |

**Honest conclusion:** raylib covers *the shell*, not the engine. It gives you maybe 15% of the
client work — but it is the most tedious and bug-prone 15% (platforms, input, image decoding, audio),
and it is the 15% the fork of the starting engine pays for today with Irrlicht, which is kept alive by
the project itself.

### 2.2 Proposed portal pipeline

One frame, active world `A`, N visible portals:

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

Two decisions have already been paid for in blood in the fork and must be carried over identical:

- **Portal quads are drawn inside the opaque pass**, not after. Transparent geometry writes depth: a
  quad drawn after is z-culled by any pane of glass in front of the portal.
- **The RTT pass before the main pass**, so the driver goes back to FBO 0 and the main pass rebinds
  its clean target.

On raylib both are trivial because **the render graph is ours**: there is no one else's `Draw3D` to
slip into with a hook. This is the first point where the rewrite is simpler than the fork.

### 2.3 Oblique near clipping on rlgl — feasibility confirmed

raylib uses an **RH, depth [-1,1]** projection (`MatrixPerspective` → `m11 = -1`). Standard Lengyel,
indices in GL column-major layout (identical to raylib's `Matrix.m2/m6/m10/m14` fields):

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

`rlgl` recomputes `mvp = matProjection * matModelView` at every `rlDrawMesh`/batch flush, so the
oblique matrix reaches both meshes and the immediate batch.

> **Checked in M0 on raylib 6.0** (not 5.5: the project started from 6.0). `rlGetMatrixProjection`,
> `rlSetMatrixProjection` and `rlDrawRenderBatchActive` exist, and the projection convention is
> **unchanged** from 5.5 — `MatrixPerspective` stays RH with depth `[-1,1]` and `m11 = -1`, so the
> formula above holds identically. Two corrections to the first draft of this paragraph:
> **`rlGetRenderBatch()` does not exist** (and never did); to empty the internal batch use
> `rlDrawRenderBatchActive()`. And `rlLoadShaderCode` is called **`rlLoadShader`** (single source)
> or **`rlLoadShaderProgram`** (vertex+fragment from strings).
>
> The formula was checked numerically, not only by inspection: `tests/portal_math_test.cpp` checks
> that a point lying on the clip plane lands exactly at `ndc.z == -1`, on axis and off axis, and that
> the geometry between camera and portal lands beyond the near plane.
>
> Beware a raylib detail not in the code above: the `Matrix` struct **declares** its fields in the
> order `m0, m4, m8, m12, m1, …`, so the order in memory is not the order of the names. Index `mN`
> means `row + 4·column` (column-major) and `MatrixToFloat` reorders before loading. A standard
> column-major `float[16]` matches `mN` element by element, but **converting with `memcpy` produces a
> transposed matrix**, silently.

**The bug that disappears by itself:** in the fork, setting the real projection made the SceneManager
cull entities against an oblique frustum (black blotches), and that is why `setRenderProjectionMatrix`
exists in `ICameraSceneNode`. In an engine of our own CPU culling uses the **unmodified** frustum
because it is our code and we know why: no hack, no patch to a third-party class.

**Guards to carry over** (all three already in the fork, `portal.cpp:153-206`):
1. Oblique **only** if the vcam is on the destination side (`side < -0.001·BS`). With the player
   *inside* the portal `c_e_w` changes sign, the near plane ends up behind the camera and the portal
   goes black → normal projection.
2. Skip if `c_e_z ≤ 1e-4` (grazing angle) or `|dot(c_c,q)| ≤ 1e-6` — degenerate.
3. The clip plane is anchored at `dst_surface + n·0.49·BS`, *inside* the outer face, so as not to
   z-fight with the adjacent blocks.

> **Clarification from M0.** `dst_surface` here is the **node's centre**, not the visible surface:
> the outer face is at `+0.5·BS`, so `0.49·BS` is "just inside" that face. It is the same convention
> as §2.4's fifth clip plane (anchored instead at exactly `pos + n·0.5·BS`). Confusing centre and
> surface is what, in the fork, left the front half of the exit node covering the image at grazing
> angles.
>
> The sign of guard 2 must be reread when moving to raylib: the fork is **Irrlicht LH** with `+z_eye`
> forward and uses `c_e_z > 1e-4`; raylib is **RH** with `-z_eye` forward, so the equivalent
> condition is `c_e_z < -1e-4`. The plane in eye space is best obtained by transforming the world
> space one with the inverse transpose of the view, instead of re-deriving the components by hand: it
> removes every sign ambiguity.

**Precision:** the vcam's view matrix must be built by hand from the already normalised `vdir`, not
via `lookAt(pos, target)`. The subtraction `target − pos` in float32 loses precision with far-off
portal positions (the cross-world case) — in the fork it took a dedicated function
(`setRenderViewMatrix`); in an engine of our own it is simply how the function is written.

### 2.4 Side clip planes — two roads, both open

The oblique near plane alone lets the destination portal's frame in at the edges, at grazing angles.
The fork's 5 planes (4 sides + face) are needed.

**Road A — `gl_ClipDistance` (desktop GL 3.3, preferred).** The shaders are ours:

```glsl
// VS
uniform vec4 uClipPlanes[5];
uniform int  uClipCount;              // 0 = off
out float gl_ClipDistance[5];
...
for (int k = 0; k < uClipCount; ++k)
    gl_ClipDistance[k] = dot(vec4(worldPos, 1.0), uClipPlanes[k]);
```

raylib does not expose `glEnable`, but **no loader is needed**: `glEnable` is core GL, exported by
`libGL.so`. Enough are

```c
extern void glEnable(unsigned int);  extern void glDisable(unsigned int);
#define GL_CLIP_DISTANCE0 0x3000
```

and five lines round the draw. (Clean alternative: `glfwGetProcAddress`, GLFW is already linked.)

**Road B — `discard` in the fragment shader (GLES 2/3, Web, hostile drivers).** Same 5 planes, a test
on the interpolated `worldPos`, `discard` if outside. It costs early-z and some fill, but it is
portable everywhere and within reach of an `#ifdef`. On GLES3 there is `EXT_clip_cull_distance`, not
guaranteed.

**To carry over identical from the fork** (they are refinements that cost bugs, not embellishments):
apex **outside** the face (`vpos -= n·(apex_side + 0.05·BS)`), otherwise the frustum has half-angle
`atan(hw/depth)` and cuts away half the destination; plane 4 anchored to the node's **outer** face
(at the centre it covers the whole image); full bypass when the player is inside the opening
(`|lx|<hw && |ly|<hh`); clamp of backward tilt (`n·bn < 0` → remove the `bn` component and anchor to
the centre of the edge).

### 2.5 Recursion (portal inside portal) — a new capability

It does not exist today (depth 1, see §1). raylib does not nest `BeginTextureMode`, but it is not a
limit: recursion is done **iteratively**, which is also the only sane way to limit its cost.

```
stack = [(player_cam, depth 0)]
for depth = MAXD..1:  render the deepest levels first into pool RTs
```

Suggested pool: `MAX_PORTALS=16` slots × `MAXD=2` levels, RTs at **resolution reduced with depth**
(100% / 50% / 25%) and aggressive reuse of slots for off-screen portals. At the maximum level, the
portal draws a flat tint or the last frame (acceptable ghosting).

### 2.6 The real work: the voxel renderer

This is 70% of the graphics effort, and raylib does **not** give it to you:

- **Meshing**: greedy meshing per face + occlusion between neighbouring chunks; compact vertex format
  (8-bit quantised position in chunk space, normal in 3 bits, AO in 2 bits, texture index in 12 bits →
  8-12 bytes/vertex). On a worker thread; upload on the GL thread on a budget (e.g. max 4
  chunks/frame) so as not to stutter.
- **Textures**: `GL_TEXTURE_2D_ARRAY` (one layer per block texture) removes bleeding between atlas
  tiles and makes mipmapping free. It is raylib's most annoying gap: ~60 lines of shim
  (`glTexImage3D`/`glTexSubImage3D` via `glfwGetProcAddress`).
  *Built in the meantime:* the atlas stays an atlas, but the tile size is the **pack's** and not a
  constant — 32 px for this repo's art, 128 px for a downloaded CC0 pack, without touching a file. The
  move to texture arrays inherits the same rule: one size per pack, decided by reading the files before
  allocating ([`TEXTURE_PACKS.md`](TEXTURE_PACKS.md)).
- **Light**: sun+block flood-fill propagation per chunk (the classic block games' model, 4 bits
  each), packed into the vertex; incremental recomputation on block placement.
- **Culling**: frustum per chunk + **cave culling** (traversal visibility per chunk, precomputed at
  meshing: which face sees which face). It is what makes a voxel engine fast, and it is why today *the
  measured bottleneck is rendering world A*.
- **Transparency**: a separate bucket sorted back-to-front **per camera** — so **per session and per
  vcam**, as `setPortalCamera(true, …)` already does in the fork.
- **Draw list per vcam**: the vcam looks at terrain the real camera has never seen. It needs its own
  drawlist with its own culling. **An invariant not to break** (it is already in the project's
  memory): that path must *never* queue a mesh update — air chunks permanently have no mesh and
  queuing them again every frame floods the queue and nails the game.

### 2.7 Graphics risks

| Risk | Severity | Mitigation |
|---|---|---|
| Recursion + N sessions = N×M passes | Medium | Per-frame budget, scaled RTs, MAXD=2, off-screen portals skipped |
| `gl_ClipDistance` on old drivers/GLES | Low | Road B (`discard`) already designed |
| Depth precision in far RTs | Medium | Reverse-Z + float32 depth (we own the projection: here it is easier than on Irrlicht) |
| No shadows → "flat" look | Low | Per-vertex AO + directional fog; shadows later |
| Meshing stutter | Medium | Workers + upload budget + amortised rebuild |

---

## 3. Server feasibility

### 3.1 Shape of the process

A single binary, three modes: `--server` (headless, no raylib linked into the core sim), client with
an in-process server (singleplayer), pure client. The rule already learnt on the fork still holds:
**the in-process server dies with the client that launched it** — surviving the redirect is not
attempted again; for a world that must stay up a dedicated process is launched.

A clean, non-negotiable separation: `core/` (voxel, ECS, mapgen, protocol) never includes
`raylib.h`. Only `client/` does. It is what makes the server compilable without a GPU and testable
headless in CI.

### 3.2 Network

- **Transport: ENet** (C, MIT, ~10k lines, zero dependencies). UDP with reliable/unreliable
  channels, fragmentation, congestion control, per-channel sequencing. It respects the "native
  languages only" constraint and adds no toolchain.
- **Channels**: 0 handshake/auth · 1 chunks (reliable, ordered, compressed) · 2 entities
  (unreliable, sequenced) · 3 game events (reliable) · 4+ **mod channels** (§6.5).
- **Compression: zstd** level 3 on chunks (or LZ4 if minimal latency is needed).
- **Crypto**: ENet does not encrypt. `libsodium` for the X25519 handshake + session key, and for the
  **player identity keys** §4.4 needs. It is not optional if portals lead to third-party servers.
  > **Deviation, applied after M3.** The library is **Monocypher 4.0.3**, not libsodium. The reason
  > is the constraint behind this line more than the line itself: libsodium builds with autotools, and
  > adding it as a submodule would add autoconf and libtool to the list of what is needed to build the
  > game — exactly the toolchain growth §3.1 rules out. Monocypher gives the same primitives (X25519,
  > XChaCha20-Poly1305, BLAKE2b, ed25519 for §4.4's tickets) in two C99 files. Cost: far fewer eyes on
  > it. Going back to libsodium is a build change and not a protocol change, because nothing above
  > `core/net/crypto.h` knows what is underneath. What exists today: session encryption with forward
  > secrecy and a pinnable server key (`--server-key`). What does not exist: the **player's**
  > identity keys, which stay M6.
- **Protocol**: versioned header + a table of features negotiated at handshake, so a new client talks
  to an old server. The fork today puts a magic `0x59415054` at the head of its packets precisely so
  as not to collide: from scratch, it is designed well at once.

### 3.3 Simulation, storage, mapgen

- **Authoritative tick** at 20 Hz, decoupled from the client's frame rate. Client interpolation,
  prediction of the local player only, backward reconciliation.
- **Interest management**: per player, a radius in chunks + priority by gaze direction. A portal adds
  **a second centre of interest** (the destination) — a native rule here, a bolt-on in the fork.
- **Storage**: SQLite with WAL (Write-Ahead Logging) + chunk compression with **zstd** (level 3) —
  transactional, compact on disk and resistant to corruption on a crash/sudden power loss. If it
  emerges as a bottleneck under extreme loads, custom Anvil-style region files.
- **Mapgen**: FastNoiseLite (single-header C) + a pipeline of stages registrable from Lua (§6).
  Deterministic by seed: mandatory if you want to test headless.
- **Threads**: a job system with a pool (N-1 cores): mapgen, meshing (client), lighting, I/O. The
  game tick stays single-thread (determinism and sanity); mods run there.

### 3.4 Authority and cheating

Movement validated server-side (speed cap, `noclip` off), placing/breaking validated by distance and
permissions, inventories server-side only. Portals introduce a case of their own: teleportation is
*decided by the server* (as today in Lua), the client draws it. The post-swap "moved too fast",
open today as BUG-10 in the fork, is prevented at the root: the swap packet carries a **grace token**
that suppresses the speed check for N ticks on the destination server.

---

## 4. Multi-instance client — the portal between different servers

### 4.1 Requirement number one: zero global state

All the difficulty of the fork's dual client comes from there: `Sky`, `Clouds`, the view range,
"who draws their own avatar", input, client identity — all things that existed once only. The
invariants in `AGENTS.MD` are, all of them, singleton scars.

The new engine's rule, applied at compile time:

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

No function of `core/` or `client/world/` takes implicit data: the session is **always** a
parameter. The swap becomes `active_session = i` plus a handful of explicit transitions, instead of
today's surgical promotions/demotions.

### 4.2 Costs — there is already a measure

From the dual-client work on the fork: **rendering world B costs 3-7% of the frame**; the bottleneck
is rendering world A. Two direct consequences:

1. Multi-session **is not the performance problem**. 3-4 live worlds at once are plausible with
   reduced budgets and view ranges for the passive ones.
2. The biggest gain of a new engine is not in portals: it is in the **main world's renderer** (cave
   culling, compact vertices, fewer draw calls), where the starting engine leaves a lot on the table.

Budget per passive session: view range ~1/3, no audio, no particles, no prediction, low-priority mesh
ticks, RTs at the portal's resolution (not the screen's).

### 4.3 Link and swap protocol

The current model is right, it only needs cleaning up. The **client** does all the work; the two
servers do not talk for a crossing (fundamental: no mutual trust to establish to pass from one world
to another).

**Exception since 2026-10-27: friend servers** (`/friend`, `src/server/admin.h`). Two servers that
put each other in their own list exchange, on a TCP port of their own (`--federation`), lines signed
with each one's signing key and encrypted for the receiver: permanent bans by key, who is online
(`/where`), a chat (`/fed`) and the backpack. The trust is there because it was given by hand, on
both sides, and it is revocable (`/friend remove` also removes its bans). Nothing needed to cross
passes there: a portal to a friend works as to anyone. The backpack instead is one for all friends:
it is on one server at a time, and the server the player arrives at asks the one they come from (or
whoever has the most recent copy), which hands it over; items a world does not know stay hidden and
travel with the backpack (docs/ADMIN.md).

```
1. Server A → client:  PORTAL_LINK { portal_id, host, port, world, dst_pos, dst_yaw, ticket }
2. Client:             opens a passive session on B (normal handshake + ticket)
3. Client:             when the passive one is Ready → declares the state to A, with HEARTBEAT
4. Crossing:           A and B validate; swap in place; A parks the ghost, B promotes
5. Fallback:           no passive ready → classic redirect with loading screen
```

Invariants already paid for, to carry into the protocol spec (not "to remember"):

- The passive one's state is **repeated on a timer** (2 s), never fire-and-forget: the main server
  drops in-game packets until the client is active there, and world A may load for tens of seconds
  after the passive one is already ready.
- A passive session declares itself **in the handshake**, not via chat: the server must know *before*
  making the ghost avatar appear to anyone.
- A session is identified by **world name (+port)**, never by address string: `127.0.0.1` and the
  LAN IP are the same world, and comparing addresses turns every homecoming into a redirect.
- The swap **does not reset held keys**: only the edges (`pressed/released/wheel`). Walking into a
  door with W held must keep walking.
- The swap **re-decides who draws their own avatar**: the promoted session yes, the demoted one no.
- The promoted session's sky is taken from the passive one's **live** `Sky`, never from the replay
  of events (a server sends `set_sky` once only: on the second round the replay is empty).
- `init_secondary()` is **idempotent**: two connections with the same name are refused by the server
  and kill the passive one that worked.

### 4.4 Identity and authentication (the real leap in quality)

Today the passive session enters as a ghost user `<name>_pv` with an empty password. It is not
acceptable in a real game. Design:

- Every player has a **key pair** (ed25519, `libsodium`) generated at first start. The identity is
  the public key; the name is only a label.
- Server A issues a signed **ticket** `{pubkey, portal_id, exp, scope=OBSERVE|ENTER}`. The client
  presents it to B. B accepts it if: the signature is valid, A is in its list of trusted peers **or**
  B accepts anonymous observers (a per-world setting).
- Scope `OBSERVE` = no avatar, no inventory, no interaction: only chunk streaming in a sphere round the
  exit point. It also reduces the abuse surface (no "I enter passively into 50 servers to mine data").
- Rate limit: N passive sessions per IP, timeout if the player never crosses.

### 4.5 Content and asset collisions

Two servers = two block registries and two media sets, and today this already produces media
collision warnings in the cross-game case. Design:

- **Content-addressed** media (BLAKE3 of the file), a cache on disk shared between all sessions. Two
  servers shipping the same texture download it once.
- **Per-session** content ids: `session->content` maps `numeric id → string name`. No global id,
  ever. Textures end up in texture array layers allocated per session, with a budget.
- Handshake: the client sends the list of hashes it already has; the server sends only the delta.

---

## 5. Scripting

### 5.1 The criteria, before the names

A scripting language for this game must bear five things together. They are in order of weight, and
they are why the answer is not automatically "Lua like the starting engine":

1. **A sandbox for untrusted code.** Portals lead to other people's servers; sooner or later someone
   will want to send scripts to the client. A runtime where "sandbox" means "delete `io` and hope" is
   not enough.
2. **Binding cost.** The engine is C/C++: every new API must not cost 50 lines of glue.
3. **Predictable performance.** No GC pauses visible at 60 fps with thousands of entities.
4. **The project's constraint**: only Lua and C/C++, no new toolchain to install — neither for you
   nor for whoever will write mods.
5. **Continuity with the starting engine**, because §7.5 (compat layer) and ~9,700 lines of Lua
   already written weigh.

### 5.2 The field, evaluated seriously

| Runtime | Year / state | Sandbox | Perf | Binding | Toolchain for the mod author | Verdict |
|---|---|---|---|---|---|---|
| **Luau** | active, C++, MIT | **Designed for hostile code**: frozen globals, no `getfenv/setfenv/loadstring`, no `os/io`, budgeted interrupts, per-VM memory cap | Interpreter ~2-3× PUC Lua; **native codegen** x86-64/arm64 for hot code | `lua_State`-style API (no FFI) | None (optional: type checker + LSP) | **Chosen (primary for gameplay/nodes/UI)** |
| **WASM** (WAMR / Wasmtime) | active, C/Rust, Apache/MIT | **Total isolation (ISA/virtual memory)**: zero accesses not granted by the host | **Near native (JIT/AOT)** | Marshalling / C-ABI / Component Model | **Yes: needs a compiler (Rust/Zig/C++/C#)** | **Chosen as an extension for heavy plugins (§5.8, §6.6)** |
| **TypeScript / JS** (Hermes / QuickJS-ng) | active, C++, MIT | Excellent (isolated realms; Hermes with compact AOT bytecode) | Good; deterministic GC on Hermes | Verbose on QuickJS, structured on Hermes | Optional TypeScript | Alternative for JS/TS modders; supportable via a Luau transpiler (`roblox-ts`) or the Hermes runtime |
| **LuaJIT 2.1** | rolling, C, MIT | Weak if FFI is there (= full access to the machine) | The fastest of the group | **FFI: zero glue** | None | ~~Second backend~~ — the reason (legacy compat) fell, §5.3 |
| Lua 5.4 (PUC) | official | Medium | Slow | manual | None | Portability fallback |
| Wren | nearly stalled | Medium | Medium | Simple | None | No: stalled ecosystem |
| Umka | active, C, small | Medium | Good (typed) | Simple | None | No: tiny community, no continuity |
| Janet / Fennel | active | Medium | Medium | Medium | Fennel: none | No: Lisp syntax = a barrier for modders |
| C# (.NET NativeAOT/Mono) | active | Weak without AppDomain | Excellent | Good | **.NET SDK mandatory** | No: heavy runtime (30-80 MB) |
| Python (CPython) | active | Terrible to sandbox | GIL, slow startup | Medium | None but heavy | No |

### 5.3 Choice: Luau primary, LuaJIT as second backend

**Why Luau and not LuaJIT as first choice**, with a game that opens doors onto other people's
machines:

- It is **the only modern Lua designed to run strangers' code**: a big platform runs millions of user
  scripts on this VM. Globals frozen by default, `getfenv`/`setfenv`/`loadstring` removed,
  `io`/`os`/`debug` absent, interrupt budget and memory limit per VM are *runtime primitives*, not
  your conventions.
- **Gradual types**: `--!strict`, annotations, official type checker and LSP. On a game API with
  hundreds of functions, the difference in usability for mod authors is huge compared with bare Lua
  5.1 — and it is what the user asked for with "more modern".
- **Modern syntax** without breaking Lua: `continue`, string interpolation, `+=`, generalised
  iteration, `type`/`export type`.
- **Native codegen** (x86-64 and arm64) on hot code: it is not LuaJIT's JIT, but it covers the case
  "ECS system in Lua called 20 times a second on 5,000 entities".
- C++ under the MIT licence, buildable with CMake: it fits the "only Lua and C++" constraint.

**Why LuaJIT stayed as an optional backend** — and why it is no longer planned work:

1. ~~**The starting engine's legacy mods do not run on Luau unmodified.**~~ It was the first reason,
   and it falls with the compatibility layer (M11 cancelled, §7.5): there is no longer a Lua 5.1 mod
   to run.
2. **FFI for trusted code**: heavy ECS systems and engine code written in Lua would get native C
   structs without glue. It remains the only reason, and no number has asked for it yet — M9's profiler
   says where a frame goes, and it is not in the bindings.

```
script_* (interface, ~25 functions)
   ├── backend "luau"    → all mods: new, downloaded, anything untrusted        [the only one]
   └── backend "luajit"  → never written; would stay for FFI and trusted hot path [not planned]
```

The same shape already chosen for `gfx_*`: a thin interface, so the decision stays reversible.
**Today there is a single runtime**, and it is the result the revision criterion written here was
looking for — by another road: not because compat ran on Luau with little shim, but because compat is
not being done (§7.5).

### 5.4 Binding cost without FFI (Luau's real price)

It is the only real disadvantage compared with LuaJIT, and it must be paid with tooling, not by hand:

- APIs are declared **once** in a descriptive table (name, arguments, types, flags); a **Lua script at
  build time** generates the C++ glue (`lua_pushnumber`/`luaL_check*`), the `.d.luau` definitions file
  for the type checker, and the Markdown documentation. A generator in Lua = no new toolchain
  (constraint 4 respected).
- Hot structures (positions, blocks, ECS components) do not go through tables: **userdata with
  metatables** and batch iterators are used (§6.1), so the per-entity cost stays in C++.

### 5.5 Modern authoring, optional

Whoever wants can write mods in **typed Luau** (`--!strict`), or in TypeScript transpiled to Luau
(with a TS→Luau transpiler) — but **the game never requires it**: a mod stays a `.luau` file that
opens in a text editor and works. This is the line not to cross, and it is why C#, WASM and the like
are out: as soon as a mod's author has to install an SDK, the mod library dies.

### 5.6 Sandbox and security

The fork today has `secure.trusted_mods` and mods reading/writing a shared directory. In the new
engine "sandbox" means something precise, and Luau gives half of it away:

- **One VM per mod** (not a shared env): crash and globals isolation.
- **A manifest with declared capabilities**: `fs:read <dir>`, `net:http <host>`, `native`, `exec`. No
  manifest → no `io`, `os`, `package`, full stop. The loader removes them *before* running a line of
  the mod. On Luau many do not even exist to be removed.
- **The LuaJIT backend (and so FFI) is a top-level capability**, equivalent to "this mod is native
  code": an explicit warning to the user at install, never granted to a mod that came from the network.
- **Memory quota** per VM and **interrupt budget** per tick (`lua_callbacks().interrupt` on Luau, an
  instruction-count hook on LuaJIT): a mod in an infinite loop is interrupted, it does not freeze the
  server.
- **Client-side mods** — ~~by default none downloaded from the server is run; if ever, only on Luau,
  signed and whitelisted~~. **Decided on 2026-09-13 (M13): a world can send the client Luau code, and
  the client can refuse it.** The sentence "it is the difference between a game and a botnet" stays
  true, and it is the reason for the conditions:
  - only **Luau**, never native code; **one VM per mod and per session**, which dies with the session;
  - **client-only capabilities**: drawing, HUD, input, its own mod's channel, reading its own
    session's world. **No files, no network, no processes**, and no manifest capability a mod installed
    by hand could ask for;
  - **memory quota and interrupt budget**, as on the server: a mod that loops stops, the client does
    not;
  - **authority stays with the server**: the code that arrives predicts and draws, it does not decide.
    A client refusing it gains nothing — the server checks all the same — and loses what the world
    wanted to show it;
  - **refusal is the player's**: for every world or for one world, and a refused world is played all
    the same, without its code.

  **Done on 2026-09-13** (TODO.md OPEN-49). A mod says where it runs with `side = server | client |
  both`; the server packs those for clients and offers them as media (`worldcode.<mod>.cnpmod`, at most
  1 MB like every media); the client loads them into the `ModHost` of the world it is in, with a
  function table stripped of everything that writes the world or registers content, no capability and
  quota and interrupt not above the defaults. Refusal is `--world-code refuse`, or the world's signing
  key in `world_code_refused.txt` in the media cache.

### 5.7 Hot reload and determinism

- Every mod declares its serialisable state; reload recreates the state from the serialised one and
  re-registers the handlers. A registry with generations: on reload only the mod's entries are dropped.
- The game tick is deterministic by seed+input: fundamental for headless tests (a practice of headless
  tests with throwaway mods and grep on the log already exists today — it must be promoted to a
  first-class framework, not a workaround).

### 5.8 Hybrid modding and scripting strategy (architectural recommendation)

To combine maximum accessibility for casual modders and native power for complex systems, the
architecture adopts a **3-tier strategy**:

1. **Tier 1 — Gameplay and blocks (Luau)**:
   - Default for 90% of mods (blocks, items, recipes, UI, interaction logic).
   - Zero toolchain for the mod author: `.luau` files editable hot.
   - Optional static types (`--!strict`), an LSP that plugs into VS Code, a native crash-proof sandbox.
2. **Tier 2 — High-performance systems and polyglot modding (WASM via WAMR)**:
   - For complex procedural world generators, fluid simulators, or custom physics.
   - Advanced modders can compile to `.wasm` from **Rust, Zig, C++, C# or AssemblyScript**.
   - A sandbox with total memory isolation (ISA level): no access to syscalls or unauthorised memory.
3. ~~**Tier 3 — Legacy compatibility & trusted hot path (LuaJIT 2.1)**~~:
   - It was for the starting engine's compatibility layer. With that gone (§7.5, 2026-09-12), a tier
     without a user remains: no mod to run, and no number has asked for FFI yet.

---

## 6. Extending the *mechanics*, not just the blocks

Registering a block is easy and everybody does it. The real problem is: **how does a mod add a system
the engine did not foresee** (a kind of fluid, a circuit, a non-Euclidean portal, a vehicle)? Six
levers, in order of increasing power.

### 6.1 Modern ECS: adopting Flecs (C/C++)

Instead of writing an ECS engine from scratch by hand with inevitable technical debt, the
recommendation is to integrate **Flecs** (C99/C++20, MIT licence, zero dependencies):
- **Native archetype storage**: vectorised, cache-friendly queries with very high performance
  (millions of entities/sec).
- **Native hierarchies and relationships**: first-class support for pairs/relationships (e.g.
  `(PortalChildOf, ParentPortal)`, `(InWorld, SessionId)`), fundamental for multi-world.
- **Ready bindings**: Flecs already exposes C/C++ and Lua/Luau bindings and WASM integration.
- **REST / web debug dashboard**: Flecs includes an explorer module to inspect visually the state of
  all entities in real time in the browser during development.

Entity = id + set of components; the mod declares components (typed schema → compact layout on the C
side) and registers systems in ordered phases:

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

The query is an *archetype iterator* run by Flecs in C: the per-entity cost stays native, Lua sees a
compact batch. This is what lets a mod add new *physics*, not only behaviour.

### 6.2 Traits on nodes, instead of hard-coded behaviours

In the new engine, "solid", "liquid", "climbable", "portal surface", "conducts signal" are not flags
of a closed enum but **traits** a mod can define and the engine consults through dispatch tables
compiled at registration time:

```lua
core.trait.define("mymod:conductive", { on_signal = "function", delay = "u8" })
core.node.register("mymod:wire", { traits = { ["mymod:conductive"] = { delay = 1 } } })
```

Directional solid faces (which in the fork are bits in `param2` and patches to `mapnode.cpp`) become a
standard trait: `solidity = { [FACE_NX] = true, ... }`. **The type-2 portal_block stops being a patch
to the engine.**

### 6.3 Typed event bus

Events with a declared schema, handlers with priority and the ability to cancel/modify; the core
itself only emits events (no "special" path reserved to the engine). If `portal:pre_teleport` is
cancellable, a mod can do "portals do not work in the Inferno" without touching `yaportal`.

### 6.4 Extensible render graph

The lever the starting engine completely lacks today. The client exposes the graph of passes as data:

```lua
core.render.add_pass("mymod:heat_haze", {
    after   = "core:transparent",
    shader  = "mymod:haze",
    inputs  = { color = "core:scene", depth = "core:depth" },
    enabled = function() return player_in_desert end,
})
```

With this, "adding a new kind of portal with its own visual effect", impossible today without
forking, becomes a mod. It is also the insurance against this same analysis being repeated in two
years.

### 6.5 Per-mod protocol channels

A mod declares messages with a schema and gets a dedicated ENet channel:

```lua
local ch = core.net.channel("yaportal:link", { reliable = true })
ch:define("state", { portal = "u16", ready = "bool", world = "string" })
ch:send_to(player, "state", { portal = 3, ready = true, world = "backportal" })
```

No new opcode in the engine for every feature — which is exactly what the fork has to do today
(`0x65`-`0x68`, `0x54`).

### 6.6 Native plugins (C ABI)

For what scripting cannot bear (heavy custom mapgen, vehicle physics, compression): mods shipping a
`.so`/`.dll` with a versioned table of function pointers. Not sandboxed, explicit user opt-in, ABI
version checked at load.

**A modern alternative, to prefer when performance allows: WASM** (WAMR, C, Apache-2) in place of
`dlopen`. Same use case — code compiled from another language — but **isolated by construction**: a
WASM plugin that crashes or attempts a syscall does not bring down the server, and it can come from
the network without the user signing blank cheques. The price is marshalling (no dynamic values
across the boundary) and the fact that the author must compile. Recommended order: `dlopen` for the
user's local plugins, WASM for everything distributed.

---

### 6.7 Minimal API surface to port `yaportal` (a concrete acceptance criterion)

A real inventory of the calls the three mods use today. It is the new engine's shopping list: if this
is covered, the mod ports; if a piece is missing, we know exactly which.

**Stock starting engine (the bulk, ~120 distinct calls):**

| Group | What is really needed |
|---|---|
| Registration | `register_node` (~150 nodes generated in loops), `register_tool`, `register_craftitem`, `register_entity`, `register_craft`, `register_alias`, `register_lbm` (8: re-register buttons/vents/doors/pedestals/displays when the mapblock loads), `register_abm`, `register_biome`, `override_item` |
| Callbacks | `register_globalstep` (13 independent steppers), join/leave/die/respawn, `on_dignode`, `on_player_hpchange`, `on_mods_loaded`, `on_shutdown`, `on_player_receive_fields`, `after` |
| Node definitions | drawtype `nodebox` (×14) and `mesh`, `node_box`/`selection_box`, `paramtype2` facedir+wallmounted, `special_tiles`, `groups`, `drop`, `light_source`, `glow`, `pointable`, `walkable`, `use_texture_alpha`, `sunlight_propagates` |
| Texture modifiers | `[combine` (the clock's 7-segment display is composed at runtime!), `[colorize` |
| Map | `get/set_node`, `swap_node`, `get_meta` (23 sites) with mixed types, `hash_node_position`, `emerge_area`, `raycast`, `get_voxel_manip`, `is_protected`, `item_place` |
| Player/Object | pos/vel/accel, look dir/yaw/pitch, `get_player_control`, `get/set_properties`, `set_physics_override`, `get_inventory`, `get_player_information`, `get_sky` |
| UI | `show_formspec` (9 sites), `formspec_version[4]`, `textlist`, `dropdown`, `checkbox`, `field_close_on_enter`, `explode_textlist_event` |
| Storage | `get_mod_storage`, `serialize`/`deserialize`, `write_json`/`parse_json`, `get_worldpath`, `get_modpath` (32 sites — detecting the host game) |
| Particles/sound | `add_particle(spawner)`, `sound_play` |

**Fork-only (~20 calls) — in the new engine they must be first-class APIs, not patches:**
`set_portals`, `set/clear_portal_cam_hint`, `register/update_portal_sky_type`,
`set/clear_portal_sky`, `set/clear_portal_xworld_dest`, `set/clear_xworld_target`,
`redirect_player`, `xworld_swap_player`, `player_dual_ready`, `xworld_exchange(_posts)`,
`get_bind_port/address`, `player:portal_teleport(...)` (atomic: pos+yaw+pitch+roll+velocity+sky),
`player:set/get_look_roll`, `set_look_pitch_animate`, `set_pos_look`, plus the
`physics_override.floor_portal_cb_shift` key and the **three groups interpreted by the C++**
(`portal_block`, `portal_frame`, `portal_slab` + carve encoding in bits 3-6 of `param2`). In the §6.2
model these three groups become **traits declared from Lua** and stop being engine code.

**Two "ugly" dependencies to replace with real features** (today they are loopholes):
- `request_insecure_environment()` → real `io`/`os` and `io.popen` to **launch other servers**. In the
  new engine: a **world supervisor** in the core (API `core.world.spawn{game, port}`), with explicit
  permissions — not a hole in the sandbox.
- `request_http_api()` + a `PortalExchange` service on `port+5000` → a native **registry service**,
  with signed records (§4.4), not plain HTTP with a shared key.
- Monkey-patching `register_abm` and `rawset(_G, …)` to get round the 200-locals-per-chunk limit:
  symptoms of an API without namespaces of its own. In the new one: modules per mod, no `_G`.

**A note worth gold:** today the dual client shares the texture cache *by name* between the two games,
and the only defence is the `yafps_` prefix on every file. It is exactly the bug §4.5
(content-addressed media + per-session ids) removes by construction.

---

## 7. Pros and cons — honest verdict

### Pros of the rewrite on raylib

1. **The architecture is born with portals inside.** Multi-session, render graph, drawlists per
   virtual camera, separate culling: all first-class instead of grafts.
2. **Irrlicht's debt disappears.** Irrlicht is maintained by the starting engine itself; today you
   pay for its fixed-function stack, its scene manager, its shaders.
3. **No more chasing upstream.** A fork that can never be merged is a lifelong tax: every rebase is a
   risk of regression on the invariants of `AGENTS.MD`.
4. **Potentially far higher performance.** On the main world (which *is* the measured bottleneck):
   cave culling, compact vertices, fewer draw calls. On portals: today every active portal costs a whole
   scene at full resolution without culling; with scaled RTs and a portal visibility graph the cost
   drops by a factor, not a percentage.
5. **Better scriptability from the start**: FFI, ECS, render graph, network channels per mod.
6. **A code base one person can understand**: 40-60k focused lines against 214k+Irrlicht.
7. **Simpler distribution**: raylib is static and tiny; no 85 MB AppImage.
8. **The specification is already written**: `PORTAL_SYSTEM.md` + the invariants are half the design
   work.

### Cons (and they are heavy)

1. **Content.** A complete game for the starting engine is man-years of work. Without a game, the
   engine is a demo. It is **the** risk of the project, not the graphics. This con was written when the
   expected answer was running that content (§7.5); with compatibility gone, it stands exactly as it is
   and the only answer is choosing a game one person can finish.
2. **Everything boring and invisible**: inventories, crafting, GUI, i18n, saves, migrations, mobs and
   AI, weather, sounds, accessibility, controllers, multi-platform.
3. **No community on day 1.** The starting engine brings mods, servers, players, wiki.
4. **Current time is spent on features that already work today.** You are at build 8, with crafting,
   cross-game and cross-server working: a rewrite puts them all back at zero.
5. **raylib is meant for small games.** Nobody has shipped a voxel MMO with raylib; every serious
   system (culling, texture arrays, threading) you add yourself — raylib does not help, it only does
   not get in the way.
6. **Bus factor 1.** An engine of one's own without a community is an engine that dies with its author.

### Alternatives to the full rewrite

| Option | Cost | Pros | Cons |
|---|---|---|---|
| **Stay on the fork of the starting engine** | 0 | Works today, free content | Lifelong rebase tax, performance ceiling, architecture hostile to multi-session |
| **raylib rewrite (this doc)** | 6-10 months to playable | Total control, native portals | Zero content, zero community |
| **General-purpose engine + C++ extensions** | 3-5 months | Editor, UI, asset pipeline, community | Portals still need custom viewports+matrices; multi-session = two `SceneTree`s (hard); GDScript ≠ Lua constraint; a big engine to understand |
| **bgfx / sokol_gfx instead of raylib** | +1 month vs raylib | Multi-backend (Vulkan/D3D/Metal), real render threading | More ceremony, fewer "batteries included"; sokol is C (ok), bgfx is C++ |
| **SDL3 + pure GL** | +1-2 months vs raylib | Zero abstractions between you and the GPU | You rewrite what raylib gives away (images, fonts, audio, maths) |

**A note on bgfx/sokol:** if the real goal is *a serious engine*, `sokol_gfx` (C, single header,
GL/Metal/D3D11/WebGPU) is technically a sounder choice than raylib for the renderer, and raylib can be
kept only for window/input/audio... or `sokol_app`. **Choice adopted: raylib behind `gfx_*`** —
prototype speed now (checked that it exposes everything needed), with a thin interface of ~30
functions that makes migrating to sokol a matter of days.

The `gfx_*` interface to respect (the rest of the client goes through here, never through `raylib.h`):

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

### 7.5 Content strategy — ~~the compatibility layer with the starting engine's API~~ (cancelled 2026-09-12)

> **This section is history, not plan.** The `compat_layer/` layer is not being done: the starting
> engine was the inspiration, not the goal, and this engine does not have to run someone else's mods
> (M11 in §8). It stays written because the analysis below is why the native API has the shape it has
> — traits instead of enums, declarative UI, 32³ chunks — and because the cost it named is real and is
> now being paid: **content is written here**.
>
> What the engine concretely had to do for the layer has been done and is worth it on its own:
> `param2` as a trait, nodebox+mesh drawtypes, ABM/LBM scheduler. The rest — formspec parser,
> `[combine`/`[colorize`, VoxelManip, mapgen aliases, LuaJIT backend — is not being done.

With the goal "complete clone of the classic block game" decided, content parity is **the** problem.
Two roads, and only one holds for a single person:

| Road | Cost | Outcome |
|---|---|---|
| Rewrite the content from scratch | man-years | The project dies first |
| ~~Run the starting engine's mods on the new engine~~ | 8-12 weeks | The existing games, the whole mod library, **and the 9,700 lines of Lua already written** run with minimal changes |

**How**, without dirtying the new architecture: the compat layer is **a Lua mod**, not engine code.
The engine exposes its clean native API (§6); `compat_layer/` is a Lua package that builds the
legacy/`core` table on top of it. It runs on the **LuaJIT backend** (§5.3): the starting engine's
existing mods are Lua 5.1 and use `setfenv`/`getfenv`/`loadstring`, which do not exist on Luau.

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

What the **engine** must provide for the layer to be thin (and not a reimplementation of the starting
engine in disguise):

- 8-bit `param2` per node with `facedir`/`wallmounted` semantics as a **standard trait**;
- `nodebox` and `mesh` drawtypes with `node_box`/`selection_box` (14 nodeboxes in `yaportal` alone);
- ABM/LBM scheduler (per-area ticks with probability/interval, and an "on chunk load" hook);
- typed per-node and per-item metadata;
- a VoxelManip equivalent (raw block access for mods' mapgen);
- **a formspec parser** — the nastiest part: a string language, 20+ elements. The real subset is
  covered (button/field/label/box/dropdown/textlist/checkbox/list/item_image + `formspec_version[4]`),
  not the whole grammar;
- media by name with the `[combine` and `[colorize` modifiers (the clock's display uses them at
  runtime).

**Honest cost:** ~60-70% of the stock starting engine's API. It is not free, and it must be said
clearly: **taking this road means inheriting some of the starting engine's design constraints**
(`param2` semantics, formspec, 16³ mapblocks). The mitigation is keeping them confined in the layer:
the new core stays free (32³ chunks internally, native declarative UI, traits instead of enums), and
the compat translates.

**Recommended order:** the layer is NOT done early. It is done in M11, after the native API has
settled on a real game (M7): a compat written on an unstable API is rewritten twice.

> This order held to the end: the API settled on M7, M8 and M9, and the choice not to do the layer
> came before writing a line of compat. What was lost is zero work; what was gained are three engine
> features the layer would have asked for and that the game uses anyway.

---

## 8. Implementation plan

Effort: a single full-time developer with AI assistance. "wk" = weeks.

### M-1 — Close the fork properly before freezing it (1 wk) · **mandatory**

The fork is frozen: if it is not closed properly, nothing working remains to go back to. Before the
freeze:

- A tagged, reproducible release (AppImage + Flatpak, `BUILD_NUMBER` bumped) of the last good state,
  with the builds already in the repo.
- Every open bug written in `TODO.md` with symptom, reproduction and hypothesis — not in the memory of
  whoever works on it.

  > **Corrected in M0:** this point cited `BUG-10` (post-swap "moved too fast") as open, but the
  > fork's `TODO.md` gives it **solved since 2026-07-28**. At the time of the freeze `BUG-1..BUG-6`
  > and `BUG-9` were open; `BUG-7`, `BUG-8`, `BUG-10`, `BUG-11` solved. The *grace token* mitigation
  > described in §3.4 stays valid as a design for the new engine.
- Commit and **push to both remotes** all unpushed work (engine `portal-fork` and outer), submodule
  aligned. Today there are local unpushed commits: freezing with those out of sync is the same as
  losing them.
- `README.md` with one line: state = frozen, where work continues.

**Exit:** clean `git status` on both repos, remote tag, downloadable artefacts.

### M0 — Validation spike (3 wk) · **KILL SWITCH**

The only goal is to falsify the three risky hypotheses, with throwaway code.

- A raylib window **already behind `gfx_*`** (§7 — the interface is born here, not later), a 32³ chunk
  meshed by hand, FPS movement with AABB collision.
- One portal: RT, virtual camera with the transform of `PORTAL_SYSTEM.md` §1.3, Lengyel's matrix via
  `rlSetMatrixProjection`, 5 clip planes via `gl_ClipDistance` + `glEnable` declared by hand, a tunnel
  quad with screen-space UVs.
- **Luau** integrated with a handful of generated bindings, a mod that places blocks and moves a
  portal at runtime.

**Exit criteria (all three, or it stops here):**
1. Visually continuous crossing — no frame showing the geometry between vcam and portal.
2. The portal stays correct with the camera *inside* the frame (the case that breaks Lengyel).
3. ≥ 200 FPS with 2 portals at 1080p on development hardware, with the cost of the Luau binding
   measured and under 1% of the frame.

### M1 — Client voxel core (6 wk)
Paged 32³ chunk store, greedy meshing on workers, compact vertex format, texture array (GL shim),
sun+block flood-fill light, per-vertex AO, frustum + cave culling, AABB collision, raycast for
selection, placing/breaking.
**Test:** a static 512³ world at 60 FPS, stable memory over 10 minutes of flight.

### M2 — Scripting and registry (5 wk)
`script_*` interface + **Luau backend** (one VM per mod, interrupt budget, memory quota, capabilities
from the manifest), a binding generator from a descriptive table (C++ glue + `.d.luau` + doc), a
data-driven registry for blocks/items/entities with hot reload, typed event bus, tick hooks. The
LuaJIT backend was planned for M11: that milestone is cancelled (§8) and the runtime stays one.
**Test:** a `--!strict` mod adding 20 blocks and an item, hot reloaded without restarting; a mod in an
infinite loop is interrupted without stopping the server.

### M3 — Server and network (5 wk)
Headless core, ENet + channels, versioned handshake with feature negotiation, compressed chunk
streaming with interest management, entity replication, movement authority, SQLite, deterministic
mapgen with registrable stages.
Sized for **2-8 players per world** (decided): no sharding, no region files, light anti-cheat (speed
cap + interaction distance + server-side inventories).
**Test:** 4 clients on LAN, 30 minutes, no desync; a headless server in CI running 1000 ticks and
checking a hash of the world.

### M4 — Complete portals (5 wk)
Port of `yaportal`'s logic (geometry, transform, triggers, per-player state machine, floor
passthrough, roll, atomic teleport), RT pool with recursion, transparent sorting per vcam, cam hint,
sky/fog through the portal.
**Test:** the suite of cases from `PORTAL_SYSTEM.md` — horizontal↔vertical, half offset, player inside
the frame, portal behind glass, intra-frame teleport (camera jumping ≥ 4 nodes between the RTT pass and
the quad pass), and **two portals looking at each other at depth 2** — which the fork cannot do today.

> **Closed.** Every clause has a gate in `ctest`: `portal_cases` for half offset, camera inside the
> frame, glass and intra-frame jump; `portal_orientation` for horizontal↔vertical; `portal_recursion`
> for depth 2 — the line the fork could not do. Clause-by-clause table in `README.md`, measurements in
> `docs/archive/M4_FINDINGS.md`.

### M5 — Local multi-session (3 wk)
Two `Session`s in the same process (two local worlds), one active and one passive rendered in the
portal, swap in place with all the invariants of §4.3.
**Test:** go in and out ten times in a row; sky, held keys, own avatar and view range correct on every
round (the four historical bugs).

> **Closed.** `dual_session` does the ten rounds and checks the four bugs on every lap. Measurements in
> `docs/archive/M5_FINDINGS.md`.

### M6 — Cross-server (4 wk)
Identity keys, signed tickets, OBSERVE/ENTER scopes, state heartbeat, fallback to redirect,
content-addressed media with a shared cache, per-session content ids.
**Test:** two servers on different machines, with different *games*; crossing without a loading
screen; a passive one towards a dead server does not spam the log (the backoff rule).

> **Closed, with a declared limit.** `bad_link` does the whole scenario at 400 ms and 20% loss
> between two servers with different *games*, and insists the crossing really happened; `dead_world`
> measures the backoff rule (2 lines in 26 s against 28108 without); `two_clocks` makes the two wall
> clocks diverge. "Different machines" is covered for MTU, fragmentation, NAT, a server with two names
> and two clocks — `tools/two_machines.sh` in containers for the first ones, an `LD_PRELOAD` for the
> last. **Not** covered: a separate kernel and real CPU contention, which remain for a VM. Measurements
> in `docs/archive/M6_FINDINGS.md`.

### M7 — Minimal game (8 wk)
Inventory, crafting, UI (microui/Clay driven by Lua), HUD, health, drops, containers, mapgen with
biomes, day/night cycle, audio with attenuation, basic mobs with ECS.
**Test:** a 30-minute game from the start, without crashes and without touching the console.

> **Closed, with four declared limits.** `tests/long_game.sh` does the thirty minutes — 7.9 million
> frames, **zero corrections**, flat RSS at +1.1 MB, 224 blocks broken and 224 put back, eight
> creatures killed, a day and a half of sky. The first run was red and found a spawn that existed only
> with `--portals`, so a player leaving the world fell for ever. **Not** covered: it is a *script*
> walking and not a person (nobody touches the console, and nobody really plays); creatures run out
> after a minute and a half because in M7 there is no mob spawning; the world has no openings, so
> crossing stays `bad_link`'s; and it needs a window, so on a headless machine this test does not run.
> Measurements in `docs/archive/M7_FINDINGS.md` §33.

### M8 — ECS and mechanics API (4 wk)
Components/systems from Lua with batch iteration, ordered phases, traits on nodes, registrable render
passes, network channels per mod.
**Test:** rewrite the classic portal-puzzle blocks (button, dispenser, pedestal, clock) **only** as
mods, without touching the engine — it is the proof that extensibility is real.

> **Closed for its exit test, with the rest of the milestone declared open.** `tests/portal1.sh` does
> the four classic portal-puzzle blocks as mods and nothing else: the button goes down under whoever
> stands on it and comes back up, the signal crosses from one mod to another and the vent drops a
> cube, the pedestal takes what is offered and gives it back to whoever comes near, the clock counts
> and after closing and reopening the world restarts from the number it had stopped at. Fourteen
> checks, headless, and the last is a `grep`: **no file under `src/` names a block, an item or an event
> of the four**.
>
> **And then closed in full.** What the exit test did not touch — the four blocks wanted per-cell
> state, not entities of their own — was written after, with the same shape: the gate before the code,
> `DISABLED` until it is green. `tests/sentry.sh` (headless, 16 checks) does **components and systems
> from Lua with batch iteration** (512 entities, one call), **phases ordered** by a number and not by
> load order, a **network channel per mod** (the field existed in the handshake since M3 and nobody used
> it; protocol 20 → 21) and `find_nodes`, the equivalent of the starting engine's LBM.
> `cnp_client --pass-check` (6 checks, gpu) does the **registrable render passes**: a pass runs inside
> the render and once per camera, so what a mod draws is seen through a portal too.
>
> **Remaining open and declared**, in `TODO.md`: a system names one component only (no join between
> two), a mod's entities are not visible to other players, and they live in the world of the session
> table — so an offline run has none. Measurements in `docs/archive/M8_FINDINGS.md`.

### M9 — Performance and robustness (4 wk)
Budgeted multi-threaded meshing, occlusion culling, distant LOD, reverse-Z, in-game profiler, crash
handler, save migration.
**Test:** three gates, written before the code like M8's. `frame_budget` (gpu) measures what a frame
costs while the world moves, on a 512-node world — 512 chunks, not the other gates' 32 — in three
phases: still, one block per frame, and everything dirty at once. The numbers are the player's and not
the ones the engine produces: 16.7 ms is a sixtieth of a second, 50 ms is a visible hitch, and a camera
buried in rock must draw what is round it and not the world behind the rock. `crash_evidence` asks
that a dying process say which signal, leave a stack, and **die all the same** — a handler that
swallows SIGSEGV is worse than no handler. `migration` asks that a world file written by an earlier
build open and keep what was inside, and that one from a later build be refused by name.

> This line was not there: M9 was the only milestone of the plan with a list of features and no
> number. It was written before starting it, because M7 closed with its exit test written last and the
> first run found twenty-two minutes of a player falling out of the world.
>
> **Closed by its three gates.** `migration` (14 checks): the version is read **before** the schema
> touches the file, because a refused opening must not change the file it refused. `crash_evidence`
> (5): the signal is named, the stack is written, and the process **dies all the same** — on
> `sigaltstack`, because the commonest way to get a SIGSEGV is running out of stack. `frame_budget`
> (21): a 512-chunk world rebuilding entirely costs **4.23 ms** in the worst frame against the budget's
> 16.7, and recovers at frame 130 instead of never.
>
> **The profiler decided the rest of the milestone.** Its property is arithmetic — everything with a
> name plus everything without **is** the frame — and the first number it produced says where a chunk
> goes: `snapshot` 0.32 ms, `build` 3.12, `upload` 0.00. 90% is the part that reads only a snapshot
> and the registry, the only one that can leave the thread. Without that line the choice between
> threads and LOD would have been an opinion.
>
> **Occlusion culling is a walk, not a test**: fifteen bits per chunk saying which faces reach each
> other, and the renderer enters a neighbour only if it could cross the one it comes from. Camera
> sealed in rock: **4 chunks instead of 129**. In front of a wall: **2 instead of 154, and not one
> pixel different** — which is the only check able to catch the failure that matters, a hole in the
> world.
>
> **Remaining open and declared**, in `docs/archive/M9_FINDINGS.md`: **distant LOD and reverse-Z were
> not written**, because they were means and the budget did not ask for them — the moment to do them is
> when a number asks, not before. And the walk is chunk-grained: in a world two chunks high, from the
> surface, it discards zero. **57 tests in `ctest`, all green, and for the first time no `DISABLED`.**

### M10 — Distribution (2 wk)
Static Linux/Windows builds, AppImage/Flatpak (reusing the existing scripts), mod package format +
versioning, API documentation.

### M11 — Starting engine API compatibility layer · **CANCELLED on 2026-09-12**

It was the `compat_layer/` layer with the second LuaJIT backend: ten weeks to run the starting
engine's mods and games on top of this engine. **It is not being done.** The starting engine was the
inspiration and the earlier fork is where the experience comes from: it is not the goal, and this
engine does not have to run someone else's mods.

**What remains of that milestone, and is already done.** M11 asked the engine for three things that
are worth it on their own and in fact were built before the choice fell:

- `param2` facedir/wallmounted as a **trait** — TODO.md CLOSED-27, a byte on the cell;
- **ABM/LBM scheduler** — CLOSED/OPEN-42, `core.register_abm` and `core.register_lbm`;
- **nodebox + mesh drawtypes** — CLOSED-43, the boxes that stop a body and a block drawn as a model.

None of the three is a compatibility piece: they are a block with a front, a world where something
happens without anyone touching a cell, and a block that is not a cube. They stay.

**What falls with it:** the formspec parser, the `[combine`/`[colorize` texture modifiers,
VoxelManip, mapgen aliases, the translation of `register_node`/`register_entity`, and the second
LuaJIT backend (§5.3), whose first reason was running legacy Lua 5.1.

**And what it costs, said and not hidden:** §7.5 said that rewriting content from scratch is worth
man-years and is the way this project dies. That sentence stays true. The difference is that now
content is a choice of *scope* and not a debt to someone else's catalogue — see M12.

### M12 — Content of our own (ongoing)

Content is written here, as mods of this engine and with its API. What is already in `mods/` is the
proof the road exists: blocks with shape and hardness, tools that count, chests, crafting, ABMs, a
mapgen with stages, entities with components — all without touching the engine.

**The scope is decided, 2026-09-12.** The game is a clone of the classic block game from which
portals lead to other worlds, and those worlds can have different mods — up to worlds that are
another game (M13). §7.5's sentence stays true and must be kept in view: content *parity* with the
classic block game is worth man-years, and with someone else's content gone there is no shortcut. What
this scope has that "complete clone" did not is the shape: not everything has to be in the sandbox,
because the rest can be behind a portal, in another world, with its own mods. This document's advice
is to measure M12 by game and not by list — a sandbox that holds a game, as M7 measured in thirty
minutes — and not to chase the classic block game's feature list.

### M13 — Worlds with a game mode of their own (open estimate)

Decided with the scope (§0). **A game mode is a world**: a server with its own mods, reached by a
portal from the sandbox, not a mode inside the sandbox. It is the reading that uses what this engine
already has — sessions per server, content-addressed media, shapes and models travelling in the
welcome — and the fork has already gone half this way: its `yafps/` is a minimal FPS arena "built as a
cross-world portal destination", **552 lines in five mods** (weapons 178, targets 114, core 114, arena
83, HUD 63). It is the natural criterion, as `yaportal` was in §6.7.

**Test:** from the sandbox, a portal to a server running `yafps` **rewritten as a mod of this
engine**, and one to a server with **a timed track** — start, checkpoints, finish, time on screen. The
client crossing **has installed neither mod**. A game and a lap are played, and you come back by the
same portal. Plus a `grep` like M8's: no file under `src/` names a weapon, a vehicle or a checkpoint.

**What is missing today, read from the code and not estimated:**

1. **From a world to the client arrive look and shape, not behaviour.** The client loads mods from its
   own directory (`src/client/main.cpp`, `--mods` and `load_dir`). On the wire travel block names,
   shapes and facing (protocol 27), models and media. Whether a block is solid, its hardness, its dig
   level (TODO.md OPEN-43) do not travel, nor a line of Luau. A game mode lives largely on the client —
   time or ammo on screen, the feedback of a shot, a vehicle's prediction, a `core.render.pass` — and
   §5.6 says **no mod downloaded from the server is run on the client**. It is this milestone's open
   decision, below.
2. **Movement is an engine number.** `core/physics/movement.h`: walk 4.8, sprint 8, fly 14, jump 8,
   gravity −25. The server's validator accepts horizontally at most `kFlySpeed × tolerance` = 14 × 1.6
   = **22.4 m/s**, outside a portal's grace second. No mod can change these numbers, per world or per
   body, so a car faster than a flying player is a cheat for this server. `yafps` used
   `set_physics_override` and `set_velocity`. Limits declared by the world and the body are needed,
   sent in the welcome like shapes and read by the validator, and a body that is not a player on foot.
   The server checks movement and does not simulate it (`movement.h` says so), and for a race it must
   be measured whether that is enough before deciding it is not.
   **First piece done, 2026-09-13** (TODO.md OPEN-44): `core.movement` declares walk, sprint, fly,
   jump and gravity **per world**, the numbers travel in the welcome (protocol 28) and the validator
   derives its limits from there — at 60 m/s the gate refuses 0 claims out of 98 in the world that
   declared them and 14 out of 99 in one without mods. Limits **per body** and a body that is not a
   player on foot remain.
3. **The client chooses the target.** TODO.md OPEN-15: the client casts the ray, the server only
   checks that the named body is within `place_reach` = **7 nodes**, and one blow every `kHitCooldown`
   = **0.4 s**. `yafps`'s rifle fires at **60 nodes every 180 ms**, with a server-side raycast, and its
   comment says why latency compensation is not needed: *all worlds run on this machine*. Between
   different machines — which is what M6 measured — that sentence no longer holds.
4. **Nobody sees a mod's entities** (TODO.md OPEN-20), and an FPS's targets and projectiles are
   exactly a mod's entities.
5. ~~**The world is a box** of 128 by 64, all generated (OPEN-35) and never unloaded (OPEN-38).~~
   **Done on 2026-09-15:** the server's world has no edge, columns are generated or reread when
   someone comes near and left when nobody is there (protocol 37). An arena fits — `yafps` asked for
   the `flat` mapgen —; a track maybe not, and it must be said when one is drawn.
6. **Of the crossing's defects one remains open**, OPEN-2 (stuck in the exit block), never
   reproduced. At the entrance of an arena or a race it is the first thing felt.

**Decision taken, 2026-09-13: a world's client-side code.** The first of the two roads below, with a
condition the recommendation did not state: **the client can refuse a world's code**, and that world
is played all the same, without it (§5.6). The two roads, as they had been put:

- **Luau shipped by the server**, in a VM per mod and per session, with client-only capabilities —
  drawing, HUD, input, its own mod's channel — and no files or network. It asks to rewrite §5.6's line.
  It is what Luau was chosen for (§5.3: a sandbox *designed* for untrusted code), and it is the only road
  where a new world does not ask for a new engine.
- **Data-driven engine primitives** — declarative HUD, movement parameters, vehicle, hitscan weapon.
  No foreign code on the client, but every mode that does not fit asks all players for an engine
  release: the opposite of "worlds with different mods".

The recommendation is the first, with authority staying where it is: the client predicts and draws,
the server decides — movement limits and a blow's ray are numbers the server checks anyway, whatever
code runs on the other side.

**Totals:**
- Working cross-server portals (M-1..M6): **32 weeks (~7 months)**
- Engine + standalone minimal game (…M10): **49 weeks (~11 months)**
- Content of our own (M12): **ongoing**. Parity with the classic block game stays 2-3 years, and it
  is the reason the criterion is a game and not a list.
- Worlds with a mode of their own (M13): **no estimate yet**. The decision on client-side code is
  taken (2026-09-13); the biggest piece that comes from it is the client sandbox, and nothing has been
  measured that says what it costs.

### Arrangement during the work

- The fork of the starting engine is **frozen** after M-1: no new work, only the tag and the
  documentation of open bugs. (Aware of the risk: for ~14 months there is no live product.)
- The new engine lives in **`/home/giacomo/dev/Cubes and portals/`**, its own git repo, not inside the
  earlier fork. Planned structure:

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
- `docs/PORTAL_SYSTEM.md` and the invariants of `AGENTS.MD` §461 become the new engine's **test
  specification**: every invariant = an automatic test. It is the way not to buy the same bugs again
  (white sky on the second round, double passive connection, keys lost at the swap, avatar invisible to
  itself, identity by address instead of by world).

---

## 9. Risks and open decisions

| Risk | Prob. | Impact | Mitigation |
|---|---|---|---|
| The project runs aground on content | **High** | Fatal | The mitigation was M11 (running the existing games), **cancelled on 2026-09-12**. The scope fixed the same day is no smaller: a clone of the classic block game plus worlds with game modes of their own. The mitigation that remains is the shape — every mode is a small world behind a portal, and M12 is measured by game and not by feature list. It is weaker than the one there was, and it must be said |
| A fast mode against a validator designed for walkers | **Certain** | High | Today it is not a risk but a fact: 22.4 m/s horizontally, a blow within 7 nodes every 0.4 s. M13 points 2-3: limits declared by the world and checked by the server, ray cast by the server |
| Client code shipped by the server | Medium | High | **Chosen on 2026-09-13.** Luau, a VM per mod and per session, client-only capabilities, no files, network or processes, quota and interrupt as on the server, authority to the server; and the player can refuse it, for every world or for one (§5.6) |
| Frozen fork = no live product for ~14 months | **High** | High | M-1 mandatory: tagged release, documented bugs, everything pushed. The fork stays *unfreezable* in an afternoon |
| Fatigue / bus factor 1 | High | Fatal | Milestones that produce something playable every time; no big bang |
| raylib insufficient in M9 | Medium | Medium | Thin `gfx_*` interface → migration to sokol in days |
| Leaky FFI sandbox | Medium | High | Capabilities in the manifest, FFI off by default, no client mods from the server |
| Cross-server abused (world scanners) | Medium | Medium | OBSERVE scope, expiring tickets, rate limit |
| Rewrite slower than planned (always) | **High** | High | Kill switch at M0; scope review at M4, M6 and at the end of M10 |
| ~~M11 reintroduces the starting engine's design constraints~~ | — | — | No longer applies: the layer is not being done, and the constraints it would have inherited (`param2` semantics, formspec, 16³ mapblocks) stay out |

**Decisions taken** (§0): fork frozen after M-1 · raylib behind `gfx_*` · 2-8 players per world · **no
compatibility with the starting engine** (2026-09-12) · **M10 suspended** until the game is declared
finished and playable · **scope: a clone of the classic block game with portals to worlds with their
own mods and game modes** (2026-09-12).

· **a world can ship Luau code to the client, and the client can refuse it** (2026-09-13, §5.6).

**No big design decision remains open.** The work remains: what M13 lists, and risk no. 1 above,
which the scope mitigates more weakly than M11 did.

---

## 10. State and next steps

**State, 2026-09-12.** M-1 done (fork frozen and tagged); M0..M3 passed and M4..M9 closed, each with
its own declared limits; M10 suspended; M11 cancelled; M12 ongoing; M13 open and without an estimate.
Open defects are in `TODO.md`, measurements and history in `docs/archive/` (since 2026-09-13, when the
tests were cut to 13 and the documentation rewritten on the current state:
[`ARCHITECTURE.md`](ARCHITECTURE.md)).

**Next steps, in order:**

1. ~~Decide a world's client-side code~~ — **decided on 2026-09-13**: Luau from the server, refusable
   by the client (§5.6).
2. **Movement limits declared by the world** and read by the validator (M13 point 2). **Per world done
   on 2026-09-13** (TODO.md OPEN-44). From there remain, cheapest first:
   - ~~chunk streaming at racing speed, measured before changing it~~ — **measured on 2026-09-13**
     (TODO.md OPEN-45): it holds up to 120 m/s, where the limit is the radius (128 nodes, one second
     of road); at 256 m/s it does not, 64 nodes of terrain ahead at the least. Not changed: it waits
     for a track that says how far one needs to see;
   - ~~a claim's path checked by the server~~ — **done on 2026-09-13** (TODO.md OPEN-46): the server
     retraces the claim with the client's solver, in five ways, and accepts it if one arrives within
     half a node; the first claim after a server placement is judged by where it ends. A path that
     turns back on an axis between two claims stays refused, which on foot does not happen and at 160
     m/s does;
   - ~~speed per body~~ — **done on 2026-09-13** (TODO.md OPEN-47): `core.body_movement` gives a body
     numbers of its own at any moment, with a message to that body and a second in which the server
     accepts the faster of old and new. It is the first piece of a vehicle and not a vehicle: no
     steering, no inertia.
3. ~~The blow checked by the server and a mod's visible entities~~ — **done on 2026-09-13** (TODO.md
   OPEN-15, OPEN-20): the server casts the ray towards the body a blow names and refuses it if there is
   a solid block in between; `core.ecs.visible` has a mod's entity sent to everyone, as a *thing* and
   not as a person. On the way: body ids on the wire were 32-bit and lost the generation (OPEN-48), now
   they are whole.
4. ~~The Luau code a world ships to the client~~ — **done on 2026-09-13** (TODO.md OPEN-49), with the
   player's refusal. What is missing from it is in that entry: HUD and input from a world's code, and a
   gate that watches it run in a window.
5. ~~A sandbox with a portal to the arena and one to the track~~ — **done on 2026-09-14** (protocol
   34): every opening has its own destination. With the `grep` clean the same day, **M13's test
   passes**, by hand on three local servers (TODO.md, "one sandbox, two worlds").

**State, 2026-09-14.** M13 has passed its test; what remains are the holes the test does not look at.
Cheapest first:

6. ~~The kart driven by hand~~ — tried by hand (2026-09-14).
7. **The track as a game.** ~~No flying over the walls~~ and ~~a body as wide as the kart~~ — **done
   on 2026-09-14** (protocol 35, `core.body_width`; TODO.md, "the timed track"), and ~~countdown, times
   kept by name and compared~~ in the track's mod, with `core.player_name`. ~~A scripted driver that
   does not take the kart out of the pit area~~ — pit area 13 wide, two laps and a return tried in game
   (2026-09-14). ~~Times lost at a server restart~~ — **kept since 2026-09-19** with
   `core.storage_set`, in the world file. **Since 2026-09-20 you arrive on foot**: the world walks,
   karts are parked in the pit area as blocks wearing the kart model, and a right click on one gets you
   in (`core.body_drive`, `core.body_model`, `core.model_id`; F gets you out and the kart stays where it
   is -- since 2026-10-16, before it was another right click, which got you out by mistake).
8. **Inertia and steering checked by the server** (TODO.md OPEN-47). **Decided on 2026-09-19:** the
   kart is a rigid body on wheels with Jolt Physics (5.6.0, deterministic build), the player on foot
   stays on our own AABB solver, with weight on the ground and the portal puzzles' air control;
   movement is in `core/physics` (`player_update`, `vehicle_update`). The next step: the client sends
   keys and dt, the server replays them with the same code and corrects where it does not match, in
   place of the speed, path and height checks. Grip is decided by Jolt's tyres, not by a `core.drive`
   number. **Until that step exists**, three rules keep the kart within what the server can check
   (TODO.md, 2026-09-20): the chassis stays within 26 degrees of vertical, the wheels take nothing over
   50 degrees as ground — the two things that made it lie down on a wall and climb it — and "inside a
   block" for a driving body is the centre only, because the server's box is not the shape the client
   simulates and the refusal put the chassis back inside the wall.
9. Then the decision whether the game is playable, which reopens M10.

---

## Checking the facts cited

Facts checked during the analysis, not remembered:

- raylib 5.5 installed locally (`~/dev/raylib/prova FPS/venv`, `raylib-5.5.0.4`): confirmed the
  presence of `rlSetMatrixProjection`, `rlGetMatrixProjection`, `rlLoadFramebuffer`,
  `rlFramebufferAttach`, `rlLoadTextureDepth`, `rlLoadVertexArray`, `rlUpdateVertexBuffer`,
  `rlDrawVertexArrayElements`, `rlSetUniform(Matrix)`, `rlTextureParameters`, `DrawMeshInstanced`,
  anisotropic filters. Confirmed **the absence** of texture array APIs.

  > **Updated in M0:** the project started from **raylib 6.0** (submodule `third_party/raylib`, tag
  > `6.0`), not 5.5. Rechecked on 6.0's header: all the functions above still exist, texture arrays
  > stay absent as expected, and the projection convention has not changed (RH, depth `[-1,1]`,
  > `m11 = -1`). Absent instead `rlGetRenderBatch` and `rlLoadShaderCode`, which §2.3 cited by mistake.
  > Build checked: `PLATFORM_DESKTOP`, `GRAPHICS_API_OPENGL_33`, GLFW on X11, static raylib.
  > Scripting: **Luau 0.733** (`third_party/luau`).
- Engine fork: 77 files, +6,369/−309 against upstream v5.16.1 (`5ebd9b579`); also measured 90 files /
  +6,587 against the merge base `750ae1956` of `upstream/master`. The starting engine's `src/` base:
  214,491 lines (client 40,962, server 16,574, network 7,100, gui 18,692, mapgen 11,669).
- The fork's portal renderer: `portal.cpp` 1050 lines (100% new) + `portal.h` 84 +
  `portal_manager.{h,cpp}` 525 ≈ **1,660 lines**; hooks in `clientmap.cpp` +182/−17; `sky.cpp`
  +187/−78; shader +12. `game.cpp` +1247/−105, of which ~880 are the ten dual-client functions and
  `performSessionSwap` (282 lines on its own).
- Portal recursion: **absent** today — the quad hook is skipped when `m_bypass_frustum_culling` is on
  (`clientmap.cpp:1281-1283`).
- Every RTT is at full rendering size (`ctx.target_size`, `ECF_A8R8G8B8`) and does a full `drawAll()`
  with culling disabled (`portal.cpp:660-919`); intra-frame re-render if the camera moves ≥ 4 nodes
  (`portal.cpp:1033-1047`).
- Mods: `yaportal/init.lua` 6911 lines / 289 KB, `yaportal_link/init.lua` 2221 / 94 KB, `yafps/` 552
  lines + 160 KB of media; total Lua ≈ 9,684 lines.
- The mod **does not use mod channels**: all extra communication goes through HTTP + the insecure
  environment.
- An experiment with raylib in Python already exists on the system (`~/dev/raylib/prova FPS`, `pyray`
  5.5, 368 lines) — useful as an ergonomics reference, not as a base.

**Not checked on the machine (to confirm in spike M0):** the properties of Luau (native codegen,
interrupt/memory quota, absence of `setfenv`/`loadstring`), QuickJS-ng and WAMR come from prior
knowledge, not from a local build. M0 must include: building Luau with CMake beside raylib, and a
measure of the cost of a scripted call per frame.
- Lua APIs added by the fork: `set_portals`, `set_portal_cam_hint`, `clear_portal_cam_hint`,
  `set_portal_xworld_dest`, `set_xworld_target`, `register/update_portal_sky_type`, `set_portal_sky`,
  `redirect_player`, `xworld_swap_player`, `xworld_exchange(_posts)`. New opcodes:
  `TOCLIENT_PORTAL_TELEPORT 0x65`, `TOCLIENT_REDIRECT 0x66`, `TOCLIENT_XWORLD_SWAP 0x67`,
  `TOCLIENT_PORTAL_STATE 0x68`, `TOSERVER_XWORLD_STATUS 0x54`.
- Dual-client performance measure (from the M7 work on the fork): rendering world B costs **3-7%** of
  the frame; the bottleneck is world A.
