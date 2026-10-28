#pragma once

// The creatures: bodies nobody drives, that walk the world on their own, go for
// the nearest player and hurt them.
//
// A creature is one of the KINDS a mod registers (core.register_mob): what it
// looks like, how much of it there is, how fast it walks and how it fights.
// Two ways to fight, and the kind says which:
//
//   Melee   walks up to the player and hits them when close enough.
//   Archer  keeps its distance, and shoots an arrow -- a body of its own that
//           flies, falls, and hurts whoever it hits -- while it can see them.
//   Pet     fights nobody: comes out by day on the grass, strolls, and is
//           tamed by a player who uses the item it wants on it (Tame). Tamed,
//           it follows them, sits and stands again when they use it again.
//
// Everything here runs on the server, once a tick, in step_mobs. What a client
// sees is a body streamed like any other, and the Working tag while it swings
// or draws, which is what plays its "attack" or "shoot" clip.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "flecs.h"

#include "core/ecs/health.h"
#include "core/item/inventory.h"
#include "core/physics/aabb.h"
#include "core/voxel/pathfind.h"

namespace cnp {
class World;
class NodeRegistry;
class ItemRegistry;
}

namespace cnp::ecs {

// How big a creature is, for whoever has to aim at one: the size of the
// models the creatures wear (1.8, the robot's skeleton) and a little narrower
// than a player.
inline constexpr float kMobHalfWidth = 0.3f;
inline constexpr float kMobHeight    = 1.8f;

// What a bare hand takes off, and how often it may.
//
// The cooldown is not politeness. Without it a client that sends a hit every
// frame kills anything in front of it inside one tick, and the server would be
// believing a rate it has no way to check.
inline constexpr uint16_t kPunch       = 1;
inline constexpr double   kHitCooldown = 0.4;

// A kind of creature, as a mod declared it.
struct MobKind {
    enum class Fight : uint8_t { Melee, Archer, Pet, Worker };

    std::string name;              // "mobs:zombie"
    uint16_t    model  = 0;        // what it wears; 0 is a box
    uint16_t    health = 20;
    float       speed  = 2.0f;     // m/s walking at a player
    Fight       fight  = Fight::Melee;
    uint16_t    damage = 2;        // a blow, or an arrow
    float       reach  = 1.6f;     // how close a blow lands; how far an archer shoots
    float       every  = 1.0f;     // seconds between blows or arrows
    ItemId      drop   = kNoItem;  // what it leaves
    std::string drop_name;         // the same, by name, until the item exists
    uint16_t    drop_count = 1;
    uint16_t    shot_model = 0;    // what an archer's arrow wears; 0 is a box
    bool        opens_doors = false;   // a shut door on its way is opened, not gone round

    // Flies, this high over whoever it goes for (and holds its height when it
    // has nobody): no gravity, straight at them, no way through the blocks.
    // 0 walks.
    float       hover = 0.0f;
    // Its shots fly straight, slower, and burst where they land: a fireball.
    bool        fireball = false;
    // Comes out of the dark only standing on this block (a cinder on a
    // fortress's bricks); empty is anywhere.
    std::string spawn_on;
    // Whose sounds it makes (core.mob_voice): "<voice>_say" and so on, its own
    // name when empty -- a dyed sheep bleats as a sheep.
    std::string voice;
    const std::string &voice_name() const { return voice.empty() ? name : voice; }
    // How far off it notices a player and goes for them (core.mob_sight).
    float       notice = 16.0f;
    // The part of its blows and arrows armour does not take (core.mob_pierce),
    // 0..1: a village guard's sword through a diamond chestplate.
    float       pierce = 0.0f;

    // A pet's (core.mob_tame): the item that tames it, by name until the item
    // exists, and what it wears once it has an owner -- a collar.
    std::string tame_name;
    ItemId      tame       = kNoItem;
    uint16_t    tame_model = 0;

    // A sheep's (core.mob_shear): shorn with this item, leaving its fleece
    // where it stands and wearing `shorn_model` until it grows back.
    std::string shear_name, fleece_name;
    ItemId      shear        = kNoItem;
    ItemId      fleece       = kNoItem;
    uint16_t    fleece_count = 1;
    uint16_t    shorn_model  = 0;
    float       regrow       = 300.0f;   // seconds

    // A chicken's (core.mob_lays): what it leaves on the ground now and then,
    // every lay_min to lay_max seconds, and once more when it dies.
    std::string lay_name;
    ItemId      lay     = kNoItem;
    float       lay_min = 0.0f, lay_max = 0.0f;

    // One more thing it leaves when it dies, 0 to extra_max of it
    // (core.mob_drop): a cow's leather.
    std::string extra_name;
    ItemId      extra     = kNoItem;
    uint16_t    extra_max = 0;

    // A wild pet's (core.mob_breed): fed this, it is in love for kLove; two
    // of a kind in love near each other go to each other and make a young
    // one, half the size, wearing `baby_model`, grown up `grow` seconds on.
    std::string breed_name;
    ItemId      breed      = kNoItem;
    uint16_t    baby_model = 0;
    float       grow       = 1200.0f;
    uint16_t    hearts_model = 0;   // over its head while it is in love; 0 none

    // The body it is moved, struck and aimed at in: never bigger than the
    // creatures' own, and smaller for a model that is (size_mob_kind).
    float height = kMobHeight;
    float half   = kMobHalfWidth;
    // How far from its middle another creature is kept: its model's own
    // half width, not held to the body's (a cow is wider than 0.6).
    float spread = kMobHalfWidth;
};

// Whether creatures of this kind go for players: what keeps a sleeper
// awake, what a spell aims at, what comes out of the dark.
inline bool fights(const MobKind &k)
{
    return k.fight == MobKind::Fight::Melee || k.fight == MobKind::Fight::Archer;
}

// A kind's body from its model's file: as tall as the model and as wide as it
// is side to side, each at most the creatures' own. Left as it is when the
// file does not read. Returns whether the model has a "death" clip -- what a
// client plays while a creature that died fades out, and what every kind is
// expected to have (docs/MODDING.md); true for a file that does not read, which
// is said elsewhere.
bool size_mob_kind(MobKind &k, const std::string &model_file);

// A kind from what core.register_mob was given, each number held to what the
// engine takes; false, and why, for a way to fight it does not know. `models`
// is how many models the world has, for the two ids that name one.
bool make_mob_kind(MobKind &out, std::string &why, const char *name, int model, int health,
                   double speed, const char *fight, int damage, double reach, double every,
                   const char *drop, int drop_count, int shot_model, int opens_doors,
                   double hover, int fireball, const char *spawn_on, size_t models);

// The items the kinds name, found by name once every mod has loaded (an item
// may be registered after the creature that drops it).
void find_mob_items(std::vector<MobKind> &kinds, const ItemRegistry &items);

// core.mob_shear and core.mob_lays on the kind of that name: 1, or 0 for a
// kind nobody registered. Every number held to what the engine takes.
int mob_shear(std::vector<MobKind> &kinds, const char *name, const char *shears,
              const char *fleece, int count, int shorn_model, double regrow);
int mob_lays(std::vector<MobKind> &kinds, const char *name, const char *item, double min_s,
             double max_s);
int mob_drop(std::vector<MobKind> &kinds, const char *name, const char *item, int max);
int mob_sight(std::vector<MobKind> &kinds, const char *name, double nodes);

// A mod's gun (core.shoot): the first body that can be hurt -- a creature, or
// a mod's entity with health -- whose box a ray from `from` along `dir` (unit)
// enters within `max`. It loses `damage` and a creature is knocked back. A
// player's body is never struck: that is the session's business.
struct ShotHit {
    flecs::entity_t target = 0;
    float           distance = 0.0f;
    bool            killed = false;
};
bool shoot_bodies(flecs::world &w, Vec3 from, Vec3 dir, float max, uint16_t damage, ShotHit &out);

// One creature by id (core.mob_hurt): a mod's own bullet, which chose its
// target itself -- a team's shot that passes its friends. 2 when that killed
// it, 1 when struck, 0 for one that is gone or already dead.
int mob_hurt(flecs::world &w, uint32_t id, uint16_t damage, float from_x, float from_z);

// A burst (core.blast): every such body with its middle within r of the
// centre loses `damage`, half of it at the edge, and a creature is thrown
// away from it. Returns how many it killed.
int blast_bodies(flecs::world &w, Vec3 centre, float r, uint16_t damage);

// Moves every creature whose feet are in the cell (x, y, z) by (dx, dy, dz) --
// a piston's push. Returns how many moved. Where it lands is not checked: the
// caller pushed the blocks out of the way first.
int push_mobs(flecs::world &w, int x, int y, int z, int dx, int dy, int dz);

// How many creatures that fight -- not a pet's kind -- have their feet within r
// of (x, y, z). What keeps a sleeper awake.
int monsters_near(flecs::world &w, const std::vector<MobKind> &kinds, float x, float y, float z,
                  float r);
// Where those creatures' feet are, the nearest first, as x y z triples into
// `out` (room for `max` floats); returns how many floats it wrote.
int mobs_near(flecs::world &w, const std::vector<MobKind> &kinds, float x, float y, float z,
              float r, float *out, int max);
// The same creatures by id (the low 32 bits of the entity), nearest first;
// returns how many. And where one of them is, false once it is gone.
int  mob_ids(flecs::world &w, const std::vector<MobKind> &kinds, float x, float y, float z, float r, bool all,
             int *out, int max);
bool mob_pos(flecs::world &w, uint32_t id, float *x, float *y, float *z);
// Those within r walk and strike at `factor` of their pace until now + seconds
// (a frost spell). Returns how many.
int slow_mobs(flecs::world &w, const std::vector<MobKind> &kinds, float x, float y, float z, float r,
              float factor, double seconds, double now);
int mob_breed(std::vector<MobKind> &kinds, const char *name, const char *item, int baby_model,
              double grow, int hearts_model);

// A worker (Fight::Worker), told by its mod. `id` is the low 32 bits of the
// entity, as mob_ids and spawn_mob give it; 0 for one that is gone or not a
// worker.
//   mob_goto   walk to the cell whose floor is (x, y, z), along a way
//   mob_going  1 while it is still on its way there
//   mob_act    play `clip` ("" stops) and hold `held` (kNoItem: nothing)
//   mob_face   turn to (x, z)
//   mob_place  put at (x, y, z) feet, stopped: climbing what it cannot walk up
//   mob_remove gone, as its mod takes it back (a robot picked up)
int mob_goto(flecs::world &w, const std::vector<MobKind> &kinds, uint32_t id, int x, int y, int z);
int mob_going(flecs::world &w, uint32_t id);
int mob_act(flecs::world &w, const std::vector<MobKind> &kinds, uint32_t id, const char *clip,
            ItemId held);
int mob_face(flecs::world &w, uint32_t id, float x, float z);
int mob_remove(flecs::world &w, uint32_t id);

// For a herder's mod (mods/animals, mods/helpers), any creature by id:
//   mob_kind_name  its kind's name, "" for none
//   mob_love       in love as if fed (feed_mob's rules but the food); 1 if so
//   mob_keep       a wild pet kept or let go (Tame::kept); 1 if it is one
//   mob_shear_now  shorn as if by its shears (shear_mob): how much fleece,
//                  left on the ground unless `drop` is false; 0 if not shorn
std::string mob_kind_name(flecs::world &w, const std::vector<MobKind> &kinds, uint32_t id);
int mob_love(flecs::world &w, uint32_t id, double now);
int mob_keep(flecs::world &w, uint32_t id, bool on);
int mob_shear_now(flecs::world &w, uint32_t id, double now, bool drop);
// A creature turned into another kind where it stands (core.mob_become),
// keeping its health, age, fleece's regrowth and stable: a sheep dyed. Only
// between kinds that fight alike. 1 if it was.
int mob_become(flecs::world &w, const std::vector<MobKind> &kinds, uint32_t id, const std::string &kind);
// Who a kind sounds like (core.mob_voice). 1 if it is a kind.
int mob_voice(std::vector<MobKind> &kinds, const std::string &name, const std::string &voice);
int mob_place(flecs::world &w, uint32_t id, float x, float y, float z);
// One creature's time between blows or arrows, as a share of its kind's.
int mob_blow_every(flecs::world &w, uint32_t id, float factor);
// Sets a worker going at that speed, in m/s: a thrown thing. Spent on the
// ground as a knock is. Returns 1, or 0 for one that is gone or not a worker.
int mob_velocity(flecs::world &w, uint32_t id, float vx, float vy, float vz);

// Not sent to one player's client (core.mob_unseen): a thing that player
// carries, which their own client draws where their eye is. Peer 0 is seen
// by everybody again.
struct UnseenBy {
    uint32_t peer = 0;
};
int mob_unseen(flecs::world &w, uint32_t id, uint32_t peer);
//   mob_hunt   after that player (peer; 0 called off), fighting as its kind
int mob_hunt(flecs::world &w, uint32_t id, uint32_t peer);
//   mob_attack after that creature (0 called off), fighting as its kind
int mob_attack(flecs::world &w, uint32_t id, uint32_t prey);
// core.mob_pierce on the kind of that name: 1, or 0 for a kind nobody registered.
int mob_pierce(std::vector<MobKind> &kinds, const char *name, double part);   // put there, at rest   // gone at once, leaving nothing

// Into the table, or over the kind of that name in its place, so a reloaded mod
// changes the creatures already walking about. Returns its number, from 1.
int add_mob_kind(std::vector<MobKind> &kinds, const MobKind &k);

// A door opened by a creature (or shut): the cell becomes its other block,
// keeping its turn, and so does the cell above or below if it is a door too --
// both halves of a door two cells tall move together. `write` puts a block and
// its turn in a cell, which is the host's business.
void swap_door(const World &world, const NodeRegistry &reg, int x, int y, int z,
               const std::function<void(int x, int y, int z, BlockId b, uint8_t facing)> &write);

// This body is a creature: something that can be hurt, that dies, and that a
// player is allowed to strike.
struct Mob {};

// What a worker is doing, as its mod said (core.mob_act): streamed, so a
// client plays the clip and draws the thing in its hand (protocol 81).
struct Act {
    std::string clip;
    ItemId      held = kNoItem;
};

// A person's clip a mod named (core.player_clip), drawn by everybody else
// until it is taken off: holding cards at a table. Not an Act: a body with an
// Act is a worker, and a right click on it goes to its mod.
struct Posed {
    std::string clip;
    // Facing this way while held (core.player_clip's dx, dz), whatever the
    // look -- sat on a chair; `faced` false keeps the look's.
    float yaw   = 0.0f;
    bool  faced = false;
};

// What a mod dresses a person in (core.player_wear), slot by slot: a model on
// their rig and its tint, 0 for an empty slot. Drawn by everybody else.
inline constexpr int kDressSlots = 10;
struct Dressed {
    uint16_t model[kDressSlots] = {};
    uint16_t tint[kDressSlots]  = {};
};

// The colours and extras a player picked to look like (net::Hello::livery,
// protocol 93), shown to everybody else. Only on a player who picked any.
struct Livery {
    uint32_t bits = 0;
};

// A pet: what tames it, and once tamed whose it is (a player's name, so it is
// still theirs when they come back) and whether it is sitting. Sitting is
// streamed as Working, which plays its "sit" clip.
struct Tame {
    ItemId      with  = kNoItem;
    uint16_t    model = 0;       // worn once tamed
    std::string owner;           // empty: wild
    bool        sitting = false;
    // Wild and KEPT (core.mob_keep): a stable's, never sent away and saved
    // with the world as the tamed are (its line's owner is "*").
    bool        kept = false;
};

// The tamed pets, one line each -- "kind x y z sitting owner" -- for the world
// file, and back: each line a pet of that kind put there, still its owner's.
// A line naming a kind nobody registered any more is skipped. Wild ones are
// not kept: the day makes more.
std::vector<std::string> pets_to_lines(flecs::world &w, const std::vector<MobKind> &kinds);
int pets_from_lines(flecs::world &w, const std::vector<MobKind> &kinds,
                    const std::vector<std::string> &lines, double now);

// A player used `held` on a pet: tamed if it is wild and that is its item
// (true in `took`: one of them is spent), sat down or stood up if it is
// theirs. False, and why, when nothing happened.
bool use_pet(flecs::entity e, const std::string &who, ItemId held, bool &took, std::string &why);

// A player used `held` on a creature that is shorn with it (core.mob_shear):
// its fleece is left on the ground, it wears its shorn model until it grows
// back, and it panics. False when it is not that item, or already shorn.
bool shear_mob(flecs::world &w, flecs::entity e, ItemId held, double now, bool drop = true);

// A creature that is shorn (core.mob_shear): its kind's shearing, on the
// body so a right click needs no table of kinds.
struct Fleece {
    ItemId   shears = kNoItem;
    ItemId   wool   = kNoItem;
    uint16_t count  = 1;
    uint16_t shorn_model = 0;
    float    regrow = 300.0f;
};

// A player used `held` on a wild creature that is bred with it (Breed): in
// love, if it is grown up and has not had a young one in kBreedAgain. True
// when it ate it, which spends one.
bool feed_mob(flecs::entity e, ItemId held, double now);

// What a creature is bred with (core.mob_breed), on the body like Fleece.
struct Breed {
    ItemId food = kNoItem;
};

// Hearts over a creature in love: a body of their own (a Thing, drawn by the
// kind's hearts_model) that goes where it goes and is gone with its love.
struct Hearts {
    flecs::entity_t of = 0;
};

// How hard two creatures in each other are pushed apart: m/s per metre
// they overlap.
inline constexpr float  kCrowdPush  = 6.0f;
// The side of the columns creatures are filed in to find who they touch: twice
// the widest spread (size_mob_kind holds it to 1), so a touch is never more
// than one column over.
inline constexpr float  kCrowdCell  = 2.0f;

inline constexpr double kLove       = 30.0;    // in love this long after eating
inline constexpr double kBreedAgain = 300.0;   // and not again before this
inline constexpr float  kLoveSeek   = 8.0f;    // goes to one in love this near

// How long a wild pet runs from a blow or the shears, and how fast next to
// its walk.
inline constexpr double kPanic      = 5.0;
inline constexpr float  kPanicSpeed = 1.6f;

// Under the open sky by day, losing a point a second: streamed (protocol 55),
// so a client can show it.
struct Burning {};

// What a body leaves behind when it dies.
//
// On the entity, so that what a thing drops is a fact about the thing rather
// than a table elsewhere that has to be kept in step with whoever spawned it.
struct Drops {
    ItemId   item  = kNoItem;
    uint16_t count = 1;
    ItemId   also  = kNoItem;   // one more: what a creature lays (core.mob_lays)
    ItemId   extra = kNoItem;   // and 0 to extra_max of this (core.mob_drop)
    uint16_t extra_max = 0;
};

// What a creature is thinking. Not streamed: a client sees where it is and
// whether it is swinging, and that is all anybody else needs to know.
struct Brain {
    uint16_t kind = 0;          // index into the kinds step_mobs is given
    float    height = kMobHeight;   // its kind's body, for whoever strikes it
    float    half   = kMobHalfWidth;
    float    spread = kMobHalfWidth;   // its kind's, halved for a young one
    float    vy   = 0.0f;       // falling, or jumping
    float    heading = 0.0f;    // where it walks when it has nobody to go for
    bool     strolling = false;
    double   rethink_at = 0.0;  // when it next picks a heading
    double   next_blow  = 0.0;  // not before this
    double   busy_until = 0.0;  // swinging or drawing: Working until then
    double   loose_at   = -1.0; // an arrow leaves the string then
    double   stuck_since = -1.0; // walking into something since then
    double   detour_until = 0.0; // going round it, sideways, until then
    float    detour_x = 0.0f, detour_z = 0.0f;
    float    kx = 0.0f, kz = 0.0f;  // knocked back by a blow: m/s, spent on the ground
    double   burn_at = 0.0;         // in the sun: the next point it loses then
    double   shade_at = 0.0;        // when it next looks for shade, burning
    bool     has_shade = false;
    PathCell shade;                 // the cell out of the sun it walks to

    // Struck or shorn: a wild pet runs from there until panic_until.
    bool     scared = false;
    float    scared_x = 0.0f, scared_z = 0.0f;
    double   panic_until = 0.0;
    double   lay_at      = 0.0;     // lays its item then (core.mob_lays); 0: not yet planned
    double   shorn_until = 0.0;     // shorn until then; 0: in its fleece
    double   slow_until  = 0.0;     // slowed (core.slow_mobs) until then, to `slow`
    float    slow        = 1.0f;
    float    blow_every  = 1.0f;    // its kind's time between blows times this (core.mob_blow_every)
    double   love_until  = 0.0;     // fed: looks for another of its kind until then
    double   breed_again = 0.0;     // not in love again before then
    double   grown_at    = 0.0;     // a young one grows up then; 0: grown
    flecs::entity_t parent = 0;     // a young one keeps near it
    flecs::entity_t hearts = 0;     // over its head while in love (Hearts)

    // What it last sounded like: the health it had when last heard, so a drop
    // in it is a "hurt"; and when it next says something of its own ("say").
    uint16_t heard_hp = 0;
    double   say_at   = 0.0;

    // A door it opened, shut again behind it once it is through (stood in its
    // cell), or once it is far from it without having gone through.
    bool     door_opened = false;
    bool     door_passed = false;
    PathCell door_at;

    // The way to the player it is after (core/voxel/pathfind.h), planned
    // again every so often as they move, and the cell of it walked to next.
    std::vector<PathCell> path;
    size_t   next_cell = 0;
    double   replan_at = 0.0;
    PathCell planned_for;       // the player's cell it was planned to
    bool     way_reached = true;   // that plan gets there, not only as near as it can
    // Getting no nearer the next cell of it for a while: that cell is one it
    // cannot get into, avoided until avoid_until, and the way planned again.
    PathCell stuck_on;
    float    stuck_best = 0.0f;
    double   stuck_at   = 0.0;
    double   stuck_seen = 0.0;   // when it last walked its way (not walking is not stuck)
    std::vector<PathCell> avoid;
    double   avoid_until = 0.0;

    // A worker's errand (core.mob_goto): the cell it walks to, while going.
    bool     going = false;
    PathCell going_to;
    // And the player it is sent after (core.mob_hunt), by peer; 0 nobody.
    uint32_t hunt = 0;
    // And the creature it is sent after (core.mob_attack); 0 none.
    flecs::entity_t prey = 0;
};

// An arrow in flight, or stuck where it landed.
struct Arrow {
    float    vx = 0.0f, vy = 0.0f, vz = 0.0f;
    uint16_t damage = 0;
    double   landed_at = -1.0;   // stuck in a block since then; gone some seconds later
    double   shot_at   = 0.0;
    float    gravity   = -12.0f;  // 0: a fireball, straight on, gone where it lands
    // A player's arrow (loose_player_arrow): who loosed it -- their avatar, which
    // it never strikes; it strikes creatures instead of passing through them --
    // what walking over it once it has landed picks up (collect_drops; 0
    // nothing), and how fast the air slows it, 1/s.
    uint64_t shooter = 0;
    uint16_t item    = 0;
    float    drag    = 0.0f;
    uint16_t bare    = 0;   // of its damage, what armour does not take
};

// A player's arrow (core.shoot_arrow), from `from` at velocity `v`: it falls
// and slows as the classic block game's does, strikes the first creature or other player in
// its way for `damage`, and stays stuck where it lands for a minute, to be
// picked up as `item`.
void loose_player_arrow(flecs::world &w, Vec3 from, Vec3 v, uint16_t damage, uint16_t model,
                        uint64_t shooter, uint16_t item, double now);

// What a player's blow does to a creature: `from` is where the blow came from.
// Knocked away from it and a little up, like a player a creature strikes.
// About a node and a half in all: 0.24 s in the air, then the ground's grip.
// A wild pet panics too (kPanic).
inline constexpr float kKnock   = 4.0f;   // m/s along the ground
inline constexpr float kKnockUp = 3.0f;   // m/s up: 3^2 / 50, a fifth of a node
void knock_back(Brain &b, float from_x, float from_z, float x, float z);

// A player a creature may go for: whose body, which connection, where.
struct Target {
    uint64_t    entity = 0;
    uint32_t    peer   = 0;
    float       x = 0.0f, y = 0.0f, z = 0.0f;   // feet
    std::string name;                          // whose pets follow them
};

// The box a creature occupies, origin at its feet like a player's.
AABB mob_box(float x, float y, float z);

// One creature of that kind, standing at x y z (feet). `kinds` is the table
// `kind` indexes.
flecs::entity spawn_mob(flecs::world &w, const std::vector<MobKind> &kinds, uint16_t kind,
                        float x, float y, float z, double now);

// Where a creature's sounds go: a singleton on the world, set by whoever can
// be heard -- the server sends them out, a local world plays them. `kind`
// indexes the kinds step_mobs is given; `what` is "say" now and then, "hurt"
// when its health drops, "death" when reap_dead takes it, "attack" as a blow
// lands and "shoot" as an arrow leaves. Unset, creatures are silent.
struct MobVoice {
    std::function<void(uint16_t kind, const char *what, Vec3 at)> say;
};

// One tick of every creature and every arrow: they fall, walk, climb, think,
// swing and shoot, burn under the open sky by day (`daylight` above a half),
// and the arrows fly. `hurt` is how a blow or an arrow reaches a player --
// through their session, which is not this file's business -- with the knock
// it gives them in m/s, and `swap_door` how a creature that opens doors opens
// the one in that cell (NodeDef::door), and shuts it behind itself, which is a
// write to the world.
void step_mobs(flecs::world &w, const World &world, const NodeRegistry &reg,
               const std::vector<MobKind> &kinds, const std::vector<Target> &targets,
               float dt, double now, float daylight,
               const std::function<void(uint32_t peer, uint16_t amount, uint16_t bare, Vec3 push)> &hurt,
               const std::function<void(int x, int y, int z)> &swap_door);

// Creatures come out in the dark near players and go when nobody is near.
//
// Once in a while (the caller decides how often): for each player with fewer
// than `cap` creatures within 48 nodes, a few tries at a cell 18..40 nodes away
// with ground under it, room for a body above, and light no brighter than 7
// (the sun counted at `daylight`). Any creature 72 nodes from every player
// goes. Returns how many came and went.
struct SpawnResult {
    int came = 0;
    int went = 0;
};
SpawnResult spawn_and_despawn(flecs::world &w, const World &world, const NodeRegistry &reg,
                              const std::vector<MobKind> &kinds,
                              const std::vector<Target> &targets, float daylight, int cap,
                              uint32_t seed, double now);

// Bodies with nothing left: what they were carrying is spawned where they fell
// and the entity goes. Returns how many died.
//
// Only things with Mob on them. A player's avatar has Health too, and a player
// who dies is put back on their feet by the session that owns them -- destroying
// that entity here would take the body out from under a live connection.
size_t reap_dead(flecs::world &w, double now);

} // namespace cnp::ecs
