#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The scripting boundary. Two backends are planned behind it: Luau for
// everything untrusted, LuaJIT later for the original engine's compatibility layer and for
// trusted hot paths (DESIGN.md 5.3). Nothing above this header includes a Lua
// header, so that second backend stays a link-time choice.
//
// The engine never calls into the world from here directly. A script_host of
// function pointers is handed in, which keeps src/script free of any dependency
// on core/ or client/ and makes the whole layer testable with a stub host.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct script_vm script_vm;

// ------------------------------------------------------------------ events
// What crosses the boundary when an event is emitted. Deliberately three types:
// a bus that can carry anything is a bus whose schema means nothing, and
// numbers, strings and booleans are what a declared schema can actually check
// (DESIGN.md 6.3).
typedef enum script_vtype {
    SCRIPT_V_NUMBER = 0,
    SCRIPT_V_STRING,
    SCRIPT_V_BOOL
} script_vtype;

typedef struct script_field {
    const char  *name;
    script_vtype type;
    double       num;
    const char  *str;   // meaningful only for SCRIPT_V_STRING
    int          b;
} script_field;

// The result of running one handler. `fields` is written back: a handler may
// change a value, and the next handler -- and the engine -- must see the change.
//
// Any string in `fields` after the call points into the VM that just ran and is
// only valid until the next call into it. The bus copies immediately.
typedef struct script_event_result {
    int cancelled;
    // The first key the handler wrote that the event does not have -- neither
    // a field nor `cancel` -- or empty. `ev.cancelled = true` is the typo this
    // is for: a new key nothing reads, and a handler that thinks it said no
    // (OPEN-18).
    char stray[64];
} script_event_result;

// ------------------------------------------------------------------ systems
// What a system is handed: one call for many entities.
//
// A component's fields are numbers, so a batch is columns of doubles and
// nothing else -- structure of arrays and not an array of structures, because
// the Lua side receives it as arrays it indexes and writes back into. One call
// for four thousand entities is the whole point of the exercise (DESIGN.md 8,
// "iterazione batch"); the alternative, a call per entity, spends more time
// crossing this boundary than inside it.
//
// Every column is READ BACK after the call. A system that changes a position
// has changed it.
typedef struct script_column {
    const char *name;
    double     *values;   // `count` of them, owned by the caller
} script_column;

typedef struct script_batch {
    int             count;       // entities in this batch
    double          dt;          // seconds since the last tick
    double          tick;        // the world's tick number, so a system can tell them apart
    const double   *ids;         // `count` entity ids, read-only
    script_column  *columns;
    int             column_count;
} script_batch;

// What a script is allowed to do to the world. The host owns the pointers and
// the userdata; the VM only calls them.
typedef struct script_host {
    void (*set_block)(void *ud, int x, int y, int z, int id);

    // Which way a cell is turned, 0..3 quarter turns about the vertical. A
    // second call and not a fifth argument on set_block, because the two are
    // written by different things at different times: placement turns a cell it
    // has just filled, and a mod usually only wants the block.
    void (*set_facing)(void *ud, int x, int y, int z, int facing);
    int  (*get_facing)(void *ud, int x, int y, int z);
    int  (*get_block)(void *ud, int x, int y, int z);

    // Moves a portal at runtime. M0 exercises this because "a mod that places
    // blocks and moves a portal" is what the spike is asked to demonstrate.
    void (*set_portal)(void *ud, int index, float x, float y, float z);
    float (*get_portal)(void *ud, int index, int axis);
    // The room of this server an opening leads to, or "" (core.portal_room).
    const char *(*portal_room)(void *ud, int index);

    // Opens and closes one at runtime (core.portal_open, core.portal_close): a
    // frame of blocks a player built. Which opening, or -1.
    int (*portal_open)(void *ud, float x, float y, float z, int axis, int sign, int rot,
                       float half_w, float half_h, int colour);
    int (*portal_close)(void *ud, float x, float y, float z);
    int (*portal_link)(void *ud, int a, int b);
    int (*portal_seal)(void *ud, int player, int index, bool sealed);

    void (*log)(void *ud, const char *message);

    // The registry, as seen from a mod: declare a block, or look one up by
    // name. Returning an id the mod does not have to remember is what keeps
    // block numbering an engine detail.
    int (*register_node)(void *ud, const char *name, int r, int g, int b, int solid,
                         int light_source, const char *turn, int dig_level, double hardness,
                         const char *material);
    // What a block is DRAWN as, when that is a model rather than a cube or a set
    // of boxes: M11's `drawtype = mesh`. The model is an id from register_model,
    // and 0 puts the block back to being drawn from its own table.
    int (*node_model)(void *ud, int id, int model);
    // What breaking a block gives, as another block's id: 0 is its own item.
    int (*node_drop)(void *ud, int id, int as);
    int (*node_drop_item)(void *ud, int id, const char *item);
    int (*node_alias)(void *ud, const char *old_name, const char *name);
    int (*node_solid)(void *ud, int id);
    int (*node_turns)(void *ud, int id);
    int (*loaded)(void *ud, int x, int y, int z);   // 1 if that cell's blocks are in memory
    int (*keep_loaded)(void *ud, int x, int z, int r);   // columns kept 30 s; 0 off a server
    // A seat: `player` carried on `host` (0: off) at x, y, z in its frame.
    int (*ride)(void *ud, int player, int host, double x, double y, double z);   // how many values core.set_facing takes for it
    // How lit a cell is now, 0..15: its block light, or the sky's times the hour.
    int (*light)(void *ud, int x, int y, int z);
    // The light blocks' light alone, 0..15, the sky left out.
    int (*block_light)(void *ud, int x, int y, int z);
    int (*watch_cell)(void *ud, int x, int y, int z, int on);
    int (*enchant_held)(void *ud, int player, int level, int kind);
    int (*held_enchant)(void *ud, int player);
    int (*wear_held)(void *ud, int player, int amount);
    int (*item_alias)(void *ud, const char *old_name, const char *name);
    int (*player_regen)(void *ud, int player, int on);
    int (*player_max_health)(void *ud, int player, int max);
    float (*time_of_day)(void *ud);
    int (*set_time)(void *ud, double t);
    int (*player_spawn)(void *ud, int player, double x, double y, double z, int on);
    // A block a body climbs; a pair of blocks that are one door, shut and open.
    int (*node_climbable)(void *ud, int id, int on);
    int (*node_door)(void *ud, int shut, int open);
    // A block that carries what stands on it (a conveyor belt): m/s, and its top.
    int (*node_conveyor)(void *ud, int id, float speed, float height, float rise, int rail);

    int (*node_box)(void *ud, int id, double x0, double y0, double z0, double x1, double y1,
                    double z1, const char *part);
    // A track piece (core/track/piece.h): what it is, and one put down.
    int (*node_piece)(void *ud, int id, int fill, int plan, int size, int climb, int grade0,
                      int grade1, int bank0, int bank1, int curbs, int wall, int width);
    int (*place_piece)(void *ud, int x, int y, int z, int id, int facing);
    int (*piece_entry)(void *ud, int x, int y, int z, int *ex, int *ey, int *ez, int *id, int *facing);
    int (*take_piece)(void *ud, int x, int y, int z);
    int (*node_id)(void *ud, const char *name);
    const char *(*node_name)(void *ud, int id);
    const char *(*node_yield)(void *ud, int id);

    // The climate the generator gave a column, by name, and "" with no
    // generator to ask. A function of the seed and the column, like
    // terrain_height, so a mapgen stage may ask it about a column nobody has
    // generated -- which a tree whose crown reaches into the next column has to.
    const char *(*biome)(void *ud, int x, int z);

    // Makes a registered block a liquid (core.node_liquid).
    int (*node_liquid)(void *ud, int id, int opacity, int fill);

    // A stage of world generation, written in Lua (DESIGN.md 8, M3: "mapgen
    // deterministico con stage registrabili" -- the C++ half of which has been
    // there since M3 and the Lua half of which had not).
    //
    // The handler is held by ref and called once per column of chunks, with the
    // seed and the column's bounds, for every column the world ever generates
    // (TODO.md OPEN-35). Registration order is the contract exactly as it is for
    // the C++ stages: a stage sees what the ones before it wrote.
    //
    // Refused once the mods have loaded, and that refusal matters: a stage
    // registered from a tick handler would run on some columns and not others, which is
    // a mod quietly doing nothing rather than a mod being told.
    int (*register_mapgen)(void *ud, const char *name, int handler_ref,
                           char *err, size_t err_size);

    // A modifier: something that happens TO cells, on its own
    // (core/world/modifiers.h). `nodes` is a list of block names and `count`
    // how many; `interval` and `chance` are the ABM's and are ignored for an
    // LBM, which runs once per matching cell of a chunk that has just loaded.
    //
    // Two entries and not one with a flag, because they are two different
    // things to a mod author and a flag would make a typo silently swap them.
    // Both refuse after the mods have loaded, for the reason register_mapgen
    // does: a modifier asked for from a tick handler is a mod quietly doing
    // nothing.
    int (*register_abm)(void *ud, const char *name, const char *const *nodes, int count,
                        double interval, int chance, int handler_ref,
                        char *err, size_t err_size);
    int (*register_lbm)(void *ud, const char *name, const char *const *nodes, int count,
                        int handler_ref, char *err, size_t err_size);

    // The generator's own ground height at a column. Here so that a stage does
    // not re-derive the terrain noise: a mod that computed its own would be
    // computing a second opinion about where the ground is, and the day the
    // noise changes only one of the two would follow it.
    int (*terrain_height)(void *ud, int x, int z);

    // Where the generator puts water over a column (core.water_level): the y
    // of the first cell above it, the terrain height where it is dry, and 0
    // with no generator to ask.
    int (*water_level)(void *ud, int x, int z);

    // Items, and the reason they are a second pair of calls rather than a flag
    // on register_node: every block gets an item form for free, so a mod only
    // reaches for these when it wants something that is NOT a block.
    int (*register_item)(void *ud, const char *name, int max_stack, const char *places,
                         int dig_level, double dig_speed, const char *digs, int damage,
                         const char *wear, int armor, int durability, int arms);
    int (*item_id)(void *ud, const char *name);

    // And the other direction: how many ids there are, what one is called, and
    // how many of it fit in a slot. item_id answers about a name a mod already
    // has; a mod that wants to know what EXISTS -- a creative inventory, a
    // search box -- has nothing to ask without these.
    int (*item_count)(void *ud);
    const char *(*item_name)(void *ud, int id);
    int (*item_stack)(void *ud, int id);

    // What the bodies in this world look like. A model is declared by name and
    // by the file it is in ON THIS MACHINE -- the name is what travels, the
    // file is not, because a path from somebody else's server is a path into
    // this machine's disk.
    //
    // And then who wears it. Two calls and not a field on the spawn, because a
    // world today says what a PERSON looks like and what a CREATURE looks like,
    // and there is nothing yet that would make one player differ from another.
    int (*register_model)(void *ud, const char *name, const char *file);
    // A model somebody else registered, by name, 0 for none (core.model_id).
    int (*model_id)(void *ud, const char *name);

    void (*player_model)(void *ud, int model);

    // And what ONE body wears (core.body_model), at any time: a world where a
    // player walks up to a kart and gets in draws the difference with this.
    int (*body_model)(void *ud, int player, int model);

    // Where this world's camera sits (core.view): nodes behind the player's
    // head, 0 for the eye. A player's machine only; a server has no camera.
    double (*view)(void *ud, double distance);   // the distance it was
    // The camera from above, the ray under the free cursor, and a walk a
    // world's code steers (core.iso_view, core.cursor_ray, core.walk).
    void (*iso_view)(void *ud, double distance, double yaw, double pitch, double fov, double seconds);
    // The same camera held on a point instead of the head (core.fixed_view).
    void (*fixed_view)(void *ud, double x, double y, double z, double distance, double yaw,
                       double pitch, double fov, double seconds);
    int (*cursor_ray)(void *ud, float *ox, float *oy, float *oz, float *dx, float *dy, float *dz);
    void (*walk)(void *ud, double dx, double dz, double speed);
    void (*engine_hud)(void *ud, int on);
    int (*engine_hud_on)(void *ud);
    void (*arms_clip)(void *ud, const char *clip);
    void (*zoom)(void *ud, double factor);
    void (*held_model)(void *ud, const char *name);
    int (*rope)(void *ud, int on);   // this machine's body on a rope (core.rope)
    void (*mob_model)(void *ud, int model);
    // A sky of one colour with no sun, and a world with no engine terrain.
    void (*world_sky)(void *ud, int r, int g, int b);
    // A tinted sky that keeps its sun, under whirling clouds of that colour.
    void (*world_storm)(void *ud, int r, int g, int b);
    void (*storm_funnel)(void *ud, int slot, double x, double y, double z, int on);   // a twister drawn
    void (*overcast)(void *ud, double amount);   // a grey sky over the hours, 0..1
    void (*night_sight)(void *ud, double amount);   // drawn no darker than this, 0..1
    int (*node_foliage)(void *ud, int id, int on);   // a block that takes the season's colour
    void (*foliage)(void *ud, int r, int g, int b, double amount);
    void (*mapgen_core)(void *ud, int on);
    // Only these mods' stages make the ground (core.mapgen_only), one per call.
    void (*mapgen_only)(void *ud, const char *mod);
    int (*portal_place)(void *ud, int index, float x, float y, float z, int axis, int sign,
                        float half_w, float half_h);
    // A kind of creature, and one of them put in the world (mobs.h).
    int (*register_mob)(void *ud, const char *name, int model, int health, double speed,
                        const char *fight, int damage, double reach, double every,
                        const char *drop, int drop_count, int shot_model, int opens_doors,
                        double hover, int fireball, const char *spawn_on);
    int (*spawn_mob)(void *ud, const char *name, double x, double y, double z);
    int (*mob_tame)(void *ud, const char *name, const char *item, int tamed_model);
    int (*mob_shear)(void *ud, const char *name, const char *item, const char *drop, int count,
                     int shorn_model, double regrow);
    int (*mob_lays)(void *ud, const char *name, const char *item, double min_s, double max_s);
    int (*mob_drop)(void *ud, const char *name, const char *item, int max);
    int (*mob_sight)(void *ud, const char *name, double nodes);
    int (*shoot)(void *ud, double x, double y, double z, double dx, double dy, double dz,
                 double max, int damage, float *distance, int *killed);
    int (*shoot_arrow)(void *ud, int player, double speed, int damage, int model, const char *item);
    int (*fall)(void *ud, int x, int y, int z);
    int (*replace_nodes)(void *ud, int x1, int y1, int z1, int x2, int y2, int z2, int from, int to,
                         int round);
    int (*repair_held)(void *ud, int player, int points);
    int (*node_falls)(void *ud, int id, int on);
    int (*node_glass)(void *ud, int id, int opacity);
    int (*blast)(void *ud, double x, double y, double z, double r, int damage);
    int (*push_mobs)(void *ud, int x, int y, int z, int dx, int dy, int dz);
    int (*monsters_near)(void *ud, double x, double y, double z, double r);
    int (*mobs_near)(void *ud, double x, double y, double z, double r, float *out, int max);
    int (*mob_ids)(void *ud, double x, double y, double z, double r, int *out, int max);
    int (*mob_pos)(void *ud, int id, float *x, float *y, float *z);
    int (*mob_goto)(void *ud, int id, int x, int y, int z);
    int (*mob_going)(void *ud, int id);
    int (*mob_act)(void *ud, int id, const char *clip, const char *item);
    int (*mob_face)(void *ud, int id, double x, double z);
    int (*mob_remove)(void *ud, int id);
    int (*mob_hurt)(void *ud, int id, int damage, double from_x, double from_z);
    int (*mob_place)(void *ud, int id, double x, double y, double z);
    // A worker carried on a mod's thing at x, y, z in its frame; thing 0 lets go.
    int (*mob_ride)(void *ud, int id, double thing, double x, double y, double z);
    int (*mob_blow_every)(void *ud, int id, double factor);
    int (*mob_velocity)(void *ud, int id, double vx, double vy, double vz);
    int (*mob_unseen)(void *ud, int id, int player);
    int (*player_launch)(void *ud, int player, double vx, double vy, double vz, double seconds);
    // A worker sent after one player (0: called off), as its kind fights.
    int (*mob_hunt)(void *ud, int id, int player);
    int (*mob_attack)(void *ud, int id, int mob);
    int (*mob_pierce)(void *ud, const char *name, double part);
    int (*slow_mobs)(void *ud, double x, double y, double z, double r, double factor, double seconds);
    int (*mob_breed)(void *ud, const char *name, const char *item, int baby_model, double grow,
                     int hearts_model);

    // What a cell remembers. Strings both ways, and core/world/node_meta.h
    // says why: a pedestal holding item id 41 is holding whatever 41 means the
    // next time the world is opened, and a name survives a mod being added.
    //
    // The returned pointer is owned by the host and is valid until the next
    // call into it -- the same contract script_field's strings have, and for
    // the same reason: the alternative is an allocation per read on a path a
    // mod runs every tick.
    // Who is in this world and where. Two calls and not one: "who is here" and
    // "where is that one" are different questions, and a single call returning
    // a list of positions would answer the button and leave the pedestal --
    // which has to hand something to a NAMED player -- with nothing.
    //
    // `players` fills `out` with at most `max` ids and returns how many it
    // wrote; `player_pos` writes through its three pointers and returns 1, or
    // returns 0 for somebody who is not in this world and writes nothing.
    // A player's bag, from a mod's side. By NAME and not by id, like everything
    // else a mod is handed: an id is assigned in registration order, so a
    // number a mod wrote down means a different item the day somebody adds one.
    //
    // Both return how many actually moved, which is the only honest answer: a
    // bag can be empty of what is asked for, and it can be full.
    int (*take_item)(void *ud, int player, const char *item, int count);
    // How many of an item a player's bag holds, worn squares included.
    int (*bag_count)(void *ud, int player, const char *item);
    // Ground somebody owns, and the things of theirs in a player's bag: the
    // stacks whose tag id is `tag` (core.owned_area marks them).
    int (*owned_area)(void *ud, int x1, int z1, int x2, int z2, int tag);
    int (*bag_tagged)(void *ud, int player, int tag, int *out, int max);
    int (*swap_item)(void *ud, int player, const char *from, const char *to, int n);
    // core.owned_cell, owned_at, owned_member and player_factors: a player's
    // territory, its owners and what being on it does to a body.
    int (*owned_cell)(void *ud, int x, int z, int tag);
    int (*owned_at)(void *ud, int x, int z);
    int (*owned_member)(void *ud, int player, int tag, int on);
    int (*player_factors)(void *ud, int player, double walk, double dig, double hurt, double regen,
                          const char *source);
    int (*take_tagged)(void *ud, int player, int tag);
    int (*give_item)(void *ud, int player, const char *item, int count);
    const char *(*held_item)(void *ud, int player);
    // What is worn in one worn square ("head" .. "back"), by name.
    const char *(*worn_item)(void *ud, int player, const char *square);
    // And which one it is: the low bits of its tag (ItemStack::tag), a mod's.
    int (*worn_id)(void *ud, int player, const char *square);
    int (*worn_set_id)(void *ud, int player, const char *square, int id);
    // Whether one player may fly (core.player_fly): their flying speed kept
    // aside and 0 sent while they may not.
    int (*player_fly)(void *ud, int player, int allowed);

    // Where the cells are that a mod did not put down. `out` is filled with
    // x, y, z triples and the return is how many NUMBERS were written, so a
    // caller's buffer of 3n holds n positions.
    //
    // The name and not an id, like everything else a mod is handed. A box of
    // more than a million cells is refused: the search runs inside the tick,
    // and the interrupt budget that stops a runaway mod counts safepoints in
    // the VM, not time spent in the engine on the mod's behalf.
    int (*find_nodes)(void *ud, int x1, int y1, int z1, int x2, int y2, int z2,
                      const char *name, int *out, int max);

    // A way through the blocks (core/voxel/pathfind.h), as x y z triples.
    int (*find_path)(void *ud, int x1, int y1, int z1, int x2, int y2, int z2, int height,
                     int step_up, int drop, int swim, int max_nodes, double hurry, int smooth,
                     int *out, int max);

    int (*players)(void *ud, int *out, int max);
    int (*player_pos)(void *ud, int player, float *x, float *y, float *z);

    // The name a player joined with, "" for nobody here. Owned by the host
    // until the next call, like meta_get's answer.
    const char *(*player_name)(void *ud, int player);
    // core.player_key: the fingerprint of the key a player proved, or "".
    const char *(*player_key)(void *ud, int player);
    int (*world_spawn)(void *ud, float *x, float *y, float *z);
    // A mod's line about a body, which goes along when the body drives into
    // another room of the server (core.player_carry, core.player_carried).
    int         (*player_carry)(void *ud, int player, const char *text);
    const char *(*player_carried)(void *ud, int player);
    int         (*body_taken)(void *ud, int player);

    // Where that player is looking, length 1, from the position the server
    // accepted; and the first block along a ray. Together they are a shot the
    // server decides without trusting a client's aim.
    int (*player_look)(void *ud, int player, float *x, float *y, float *z);
    // And moving one (core.player_place): feet there, looking along l, or
    // keeping the look when l is zero. 1, or 0 when there is no such body.
    int (*player_place)(void *ud, int player, double x, double y, double z, double lx,
                        double ly, double lz);
    // Lying down (core.player_lie): the middle of the body at x, y, z, the head
    // along (hx, hz); or up again when `on` is 0. 1, or 0 when there is no such
    // body.
    int (*player_lie)(void *ud, int player, int on, double x, double y, double z, double hx,
                      double hz);
    // A clip a body plays until told otherwise (core.player_clip), "" for none:
    // on a server everybody else sees it; on a player's machine the arms seen
    // in first person play it.
    int (*player_clip)(void *ud, int player, const char *clip);
    // A model of a mod's own a body wears in one of ten slots (core.player_wear),
    // 0 for none, and its tint; for everybody to see. 1, or 0 when refused.
    int (*player_wear)(void *ud, int player, int slot, int model, int tint);
    // A sound by name at a place (core.play_sound), for everybody in the world
    // to hear. 1, or 0 for a name that is not "mod:thing".
    int (*play_sound)(void *ud, const char *name, double x, double y, double z);
    int (*raycast)(void *ud, double x, double y, double z, double dx, double dy, double dz,
                   double max, int *hx, int *hy, int *hz, float *distance);

    int         (*meta_set)(void *ud, int x, int y, int z, const char *key, const char *value);
    const char *(*meta_get)(void *ud, int x, int y, int z, const char *key);

    // Recipes. Strings and not ids, because a mod registers these while it
    // loads and the item form of a block is minted after every mod has had its
    // say -- an id taken up there is kNoItem for everything a mod builds with.
    // See core/item/recipe.h.
    // `makes` and not `result`: the generated binding calls its own return
    // value `result`, and two of them in one function do not compile -- which
    // is how this was found rather than by reading the generator.
    int (*recipe_shaped)(void *ud, const char *makes, int count, const char *row1,
                         const char *row2, const char *row3);
    int (*recipe_shapeless)(void *ud, const char *makes, int count, const char *items);
    // Things that take colours at a table, and the dyes (RecipeSet::add_dyeable).
    int (*recipe_dye)(void *ud, const char *item);
    int (*item_dye)(void *ud, const char *item, int dye);

    // Panels, from a mod's side. DESIGN.md 8 asks for a UI driven by Lua; this
    // is it, and it is retained rather than immediate because a mod runs on the
    // server and its panel is drawn on a client -- see core/ui/panel.h.
    //
    // They return 1 or 0, and a refusal says why in the LOG rather than back in
    // Lua. That is a real limitation and it is stated: giving the reason to the
    // caller needs the err-buffer shape the event bus uses, and these are
    // generated, so it would have to be the generator that learned it.
    int (*ui_define)(void *ud, const char *name, float w, float h);
    int (*ui_label)(void *ud, const char *panel, float x, float y, const char *text);
    int (*ui_button)(void *ud, const char *panel, const char *id, float x, float y, float w,
                     float h, const char *text);
    int (*ui_field)(void *ud, const char *panel, const char *id, float x, float y, float w,
                    float h, const char *text);
    int (*ui_item)(void *ud, const char *panel, const char *id, float x, float y, float w,
                   float h, const char *item);
    int (*ui_box)(void *ud, const char *panel, float x, float y, float w, float h, int r, int g,
                  int b);
    int (*ui_model)(void *ud, const char *panel, float x, float y, float w, float h, int model, int tint,
                    const char *pieces);
    int (*ui_show)(void *ud, const char *name, int open, int player);

    // Containers: an inventory that belongs to a cell rather than to a session.
    //
    // Making one and showing one are separate calls, because they are separate
    // acts: a loot chest that is filled before anybody arrives has no player to
    // be shown to, and a container_show that was the only way to make one meant
    // no mod could put anything in a chest until somebody opened it.
    int (*container_make)(void *ud, int x, int y, int z, int slots);
    int (*container_add)(void *ud, int x, int y, int z, int item, int count, const char *role);
    // What each square of a block's container is for (SlotRole), and a block whose stacks do not age.
    int (*container_roles)(void *ud, int id, const char *roles);
    int (*node_keeps_drops)(void *ud, int id, int on);
    int (*container_accepts)(void *ud, int id, const char *role, const char *items);
    int (*container_progress)(void *ud, int x, int y, int z, float seconds);
    int (*container_scroll)(void *ud, int x, int y, int z, float pos, float size);
    int (*container_search)(void *ud, int x, int y, int z, const char *text);
    int (*container_show)(void *ud, int x, int y, int z, int slots, int player);
    int (*container_close)(void *ud, int player);
    int (*container_count)(void *ud, int x, int y, int z, int item, const char *role);
    float (*container_fill)(void *ud, int x, int y, int z);
    int (*container_revision)(void *ud, int x, int y, int z);
    int (*container_move)(void *ud, int fx, int fy, int fz, int tx, int ty, int tz, int count);
    int (*collect_drops)(void *ud, int x, int y, int z, int tx, int ty, int tz);
    // One stack at a time, as it is: what lies in a cell, moving it, and one out of a container.
    int (*drop_at)(void *ud, int x, int y, int z);
    int (*drop_send)(void *ud, int x, int y, int z, float tx, float ty, float tz);
    int (*container_drop)(void *ud, int x, int y, int z, float tx, float ty, float tz, int count);
    int (*container_item)(void *ud, int x, int y, int z, int slot);

    // Takes up to `count` of an item out of the container at that cell and
    // returns how many it took.
    int (*container_take)(void *ud, int x, int y, int z, int item, int count, const char *role);

    // What is inside, on the ground. A container has no world to fall into and
    // core/item knows nothing about entities, so this is the layer that can.
    int (*container_spill)(void *ud, int x, int y, int z, int from);

    // A crafting table: this player's grid becomes three by three at that cell.
    // The two-by-two everybody carries needs no call -- it is always there.
    int (*craft_show)(void *ud, int x, int y, int z, int player);
    int (*craft_close)(void *ud, int player);
    int (*drop_item)(void *ud, int x, int y, int z, int item, int count);

    // How much of a player is left, and moving it. `amount` is points, and the
    // return is what they have AFTER -- so a mod that wants to know whether it
    // killed somebody reads a zero rather than being told separately.
    int (*damage)(void *ud, int player, int amount);
    int (*heal)(void *ud, int player, int amount);
    int (*health)(void *ud, int player);
    int (*container_clear)(void *ud, int x, int y, int z);

    // The event bus, from a mod's side. The bus itself lives above this layer,
    // because an event emitted in one VM has to reach handlers in another and
    // no VM can be trusted to hold the list.
    //
    // `define` declares a schema: field names with types, and whether the event
    // can be cancelled. `on` registers a handler and is handed the reference
    // the VM will use to find the function again. `emit` runs the handlers.
    // All three return 0 on failure and write why into `err`.
    // Entities a mod owns, with fields a mod declared (DESIGN.md 6.1, 8).
    //
    // Ids are doubles and not ints on purpose: an entity id carries a
    // generation in its upper bits, so it outgrows 32 bits the first time
    // anything is recycled, and a Lua number holds it exactly either way.
    //
    // `ecs_component` declares a set of named number fields. `ecs_phase`
    // declares a point in the tick and what it comes after -- a NUMBER, so
    // that two mods that never met still run in a stated order rather than in
    // load order. `ecs_system` runs a handler over every entity that has a
    // component, in batches.
    int (*ecs_component)(void *ud, const char *name, const char *const *fields, int count,
                         char *err, size_t err_size);
    int (*ecs_phase)(void *ud, const char *name, int order, char *err, size_t err_size);
    int (*ecs_system)(void *ud, const char *name, const char *phase, const char *component,
                      int handler_ref, char *err, size_t err_size);

    double (*ecs_spawn)(void *ud, const char *component, double x, double y, double z);
    int    (*ecs_set)(void *ud, double id, const char *field, double value);
    int    (*ecs_get)(void *ud, double id, const char *field, double *out);
    int    (*ecs_kill)(void *ud, double id);
    // Whether other players are sent this entity (TODO.md OPEN-20); only an
    // entity a mod spawned, never a body the engine gave something.
    int    (*ecs_visible)(void *ud, double id, int on);
    int    (*ecs_model)(void *ud, double id, int model);
    int    (*ecs_health)(void *ud, double id, int set);
    int    (*ecs_clip)(void *ud, double id, const char *name);
    int    (*ecs_box)(void *ud, double id, double half, double height);

    // core.craft: a mod's thing made a machine (core/ecs/machine.h), on a server.
    int (*craft_make)(void *ud, double id, const char *kind, double mass, double half_w,
                      double half_h, double half_l, double accel, double top, double turn);
    int (*craft_keys)(void *ud, double id, double forward, double right, double up, double yaw);
    // x y z yaw pitch roll vx vy vz speed grounded impact throttle: 13 numbers.
    int (*craft_pose)(void *ud, double id, double *out);
    int (*craft_push)(void *ud, double id, double vx, double vy, double vz);
    int (*craft_mount)(void *ud, double id, double on, double x, double y, double z);
    int (*craft_aim)(void *ud, double id, double yaw, double pitch);
    int (*craft_seat)(void *ud, int player, double id, double x, double y, double z);

    // Which side this code runs on: "server", "client" -- a world's code on a
    // player's machine -- or "both" for a process that is its own server
    // (TODO.md OPEN-49).
    const char *(*side)(void *ud);
    int    (*ecs_count)(void *ud, const char *component);

    // A channel of this mod's own, and what it says on it (DESIGN.md 6.5).
    //
    // `net_open` is asked while the mod loads and answers with the channel
    // number, or 0 for a mod that asked too late -- the numbers are handed out
    // in the handshake, so a mod that asked after the first client joined would
    // get one nobody was told about.
    //
    // `net_send` takes a player id, or 0 for everybody in this world. What
    // arrives on the other side is an event, because a message that woke a
    // callback registered a different way would be a second kind of listening.
    // Drawing, from a mod's side (DESIGN.md 8). Only a process with a screen
    // sets these: on a server they are null, and the binding says so rather
    // than failing with a sentence about a call that "did not work".
    //
    // `render_pass` registers a handler the renderer calls once per camera,
    // inside the frame. `render_box` is only legal while one of those is
    // running -- outside it there is no camera to draw against, and a mod that
    // called it from on_step would be drawing into whatever was last bound.
    int (*render_pass)(void *ud, const char *name, int order, int handler_ref, char *err,
                       size_t err_size);
    int (*render_box)(void *ud, double x, double y, double z, double w, double h, double d,
                      int r, int g, int b, int a);
    // And a model of the world's, at rest, its feet at (x, y, z) turned by
    // `yaw` about the up: a thing drawn where only this machine knows it is
    // (a cube in the player's hands). Then tipped by `pitch` about its own
    // x and `roll` about its own z, before the yaw: a card held up in a hand.
    int (*render_model)(void *ud, int model, double x, double y, double z, double yaw,
                        double scale, double pitch, double roll);
    // And an effect of light (core.render.fx): a kind by name ("glow", "flame",
    // "ring", "frost", "bolt", "slash", "sparks", "vortex", "jet"), where and how big,
    // its colour, how far through its life it is (0..1), and for a bolt where
    // it ends.
    int (*render_fx)(void *ud, const char *kind, double x, double y, double z, double size, int r,
                     int g, int b, int a, double t, double x2, double y2, double z2);

    // Drawing over the screen rather than into the world (TODO.md OPEN-49): a
    // counter, a timer, a crosshair. A pass of its own and not a render pass,
    // because a render pass runs once per CAMERA and a portal on screen would
    // draw the count once for every world it shows. This one runs once a frame,
    // after the engine's own HUD. Positions are fractions of the screen and
    // sizes fractions of its height, so a square is square at any aspect.
    int (*hud_pass)(void *ud, const char *name, int order, int handler_ref, char *err,
                    size_t err_size);
    int (*hud_text)(void *ud, double x, double y, const char *text, double size, int align,
                    int r, int g, int b, int a);
    double (*hud_aspect)(void *ud);   // the screen's width over its height; 0 with no screen
    int (*hud_rect)(void *ud, double x, double y, double w, double h, int r, int g, int b,
                    int a);
    // A picture from this world's media (a mod's textures), by file name.
    int (*hud_image)(void *ud, const char *name, double x, double y, double w, double h, int a);
    // An item's square as the bag draws it, by name: a track piece as its plan.
    int (*hud_item)(void *ud, const char *name, double x, double y, double side);
    // Where the mouse cursor is, as fractions of the screen; 0 with no screen.
    int (*hud_mouse)(void *ud, double *x, double *y);

    int (*net_open)(void *ud);
    int (*net_send)(void *ud, int player, const char *text);

    // How a body moves in this world (TODO.md OPEN-44). Asked while the mod
    // loads and answered 1 or 0, for the reason net_open is: the numbers go
    // out in the welcome, and a world whose players were told different ones
    // is a world whose server corrects some of them.
    int (*movement)(void *ud, double walk, double sprint, double fly, double jump,
                    double gravity);

    // And how ONE body moves, at any time (TODO.md OPEN-47): a mod gives a
    // player numbers of their own, and that player is sent them.
    int (*body_movement)(void *ud, int player, double walk, double sprint, double fly,
                         double jump, double gravity, double rise);

    // Whether bodies in this world drive (core.drive): asked while the mod
    // loads, answered 1 or 0, out in the welcome with the movement numbers.
    int (*drive)(void *ud, double accel, double brake, double turn, double drag);

    // And whether ONE body drives (core.body_drive): core.drive and
    // core.body_width for a single player, at any time, sent to that player by
    // the message core.body_movement sends. A world where you walk up to a kart
    // and get in is this call.
    int (*body_drive)(void *ud, int player, double accel, double brake, double turn, double drag,
                      double width, double length);

    // And whether ONE body is in a boat (core.body_boat), sent as body_movement is.
    int (*body_boat)(void *ud, int player, int on);
    // And in a cart (core.body_cart): rails are ridden.
    int (*body_cart)(void *ud, int player, int on);

    // How wide a body is in this world, and how long when it drives
    // (core.body_width): the same moment and the same welcome.
    int (*body_width)(void *ud, double width, double length);

    // How high a body's eye is over its feet (core.eye_height), while the mods
    // load: it goes out in the welcome with the rest of MovementRules.
    int (*eye_height)(void *ud, double height);

    // Whether bodies in this world do parkour (core.parkour), while the mods
    // load: in the welcome with the rest of MovementRules.
    int (*parkour)(void *ud, int on);
    // And whether hands grab ledges there (core.ledges), the same way.
    int (*ledges)(void *ud, int on);

    // A sentence on one player's screen, where a refused edit's reason goes
    // (core.notice, TODO.md OPEN-11). Player 0 is everybody in this world.
    int (*notice)(void *ud, int player, const char *text);

    // A line in one player's chat (core.chat), player 0 everybody in this
    // world; and a /command this mod answers (core.command).
    int (*chat)(void *ud, int player, const char *text);
    int (*command)(void *ud, const char *name, const char *help, const char *perm);

    // What a player may do on this server (core.has_perm, core.role):
    // src/server/admin.h. Null in the single-process client.
    int         (*has_perm)(void *ud, int player, const char *perm);
    const char *(*role)(void *ud, int player);
    // Pages (src/core/ui/page.h): one a player may open by name, the markup
    // shown to one player, and whether a player's chat is held. Null in the
    // single-process client.
    int (*page_register)(void *ud, const char *name, const char *title, const char *perm);
    int (*page_show)(void *ud, int player, const char *name, const char *markup);
    int (*is_muted)(void *ud, int player);

    // What a mod keeps across a restart (core.storage_set, core.storage_get):
    // strings under names, the mod's own, in the world file. Null where
    // nothing is saved.
    int         (*storage_set)(void *ud, const char *key, const char *value);
    const char *(*storage_get)(void *ud, const char *key);
    // The same, kept once for every room of the server (core.shared_storage_*).
    int         (*shared_storage_set)(void *ud, const char *key, const char *value);
    const char *(*shared_storage_get)(void *ud, const char *key);

    int (*event_define)(void *ud, const char *event, const script_field *fields, int count,
                        int cancellable, char *err, size_t err_size);
    int (*event_on)(void *ud, const char *event, int priority, int handler_ref,
                    char *err, size_t err_size);
    int (*event_emit)(void *ud, const char *event, script_field *fields, int count,
                      int *out_cancelled, char *err, size_t err_size);

    void *ud;
} script_host;

typedef struct script_desc {
    script_host host;

    // Hard ceiling on the VM's heap. Exceeding it fails the allocation, which
    // surfaces as a Lua error rather than as the process being killed.
    size_t memory_limit_bytes;

    // Safepoints a single call may execute before it is interrupted. This is
    // what stops an infinite loop in a mod from freezing the server instead of
    // just failing that mod (DESIGN.md 5.6).
    unsigned interrupt_budget;

    // Globals removed before the sandbox is frozen and before any mod code
    // runs. The caller decides the list: this layer holds no policy about what
    // a mod may reach, only the mechanism for taking it away. Removing rather
    // than shadowing matters -- a sandboxed thread reads through to the frozen
    // global table, so a name left in place there is reachable from every mod.
    const char *const *strip_globals;
    int                strip_count;
} script_desc;

script_vm *script_create(const script_desc *desc);
void       script_destroy(script_vm *vm);

// Compiles and runs a chunk. On failure returns false and fills `err`.
bool script_run_source(script_vm *vm, const char *chunk_name, const char *source,
                       char *err, size_t err_size);
bool script_run_file(script_vm *vm, const char *path, char *err, size_t err_size);

// Calls a global function of one number argument, if it exists. Returns false
// on error; a missing function is not an error and returns true.
bool script_call_step(script_vm *vm, const char *fn_name, double dt,
                      char *err, size_t err_size);

// Reload support (DESIGN.md 5.7): a mod hands out its state as a string before
// its VM is thrown away, and gets it back in the new one. A string rather than
// a table because the mod decides what its state is and how to encode it -- a
// generic table serialiser would have to have an opinion about functions,
// cycles and userdata, and would be wrong about all three.
//
// A missing function is not an error: `out` comes back empty and `state` is
// ignored. A state larger than `out_size` is an error, not a truncation.
bool script_call_save(script_vm *vm, const char *fn_name, char *out, size_t out_size,
                      char *err, size_t err_size);
bool script_call_load(script_vm *vm, const char *fn_name, const char *state,
                      char *err, size_t err_size);

// Runs one handler registered through `core.event.on`. `fields` is updated in
// place with whatever the handler changed, and `result` says whether it asked
// for the event to be cancelled.
bool script_call_handler(script_vm *vm, int handler_ref, script_field *fields, int count,
                         int cancellable, script_event_result *result,
                         char *err, size_t err_size);

// Runs one system over one batch. The columns are written back, so a system
// that moved something moved it.
//
// Separate from script_call_handler because the shapes have nothing in common:
// an event carries a handful of named scalars and may be cancelled, a system
// carries thousands of numbers in columns and may not.
bool script_call_system(script_vm *vm, int handler_ref, script_batch *batch,
                        char *err, size_t err_size);

// Releases a handler reference. Destroying the VM releases them all, so this is
// for a mod that unregisters one while it runs.
void script_unref_handler(script_vm *vm, int handler_ref);

size_t      script_memory_used(script_vm *vm);
const char *script_backend_name(void);

#ifdef __cplusplus
} // extern "C"
#endif
