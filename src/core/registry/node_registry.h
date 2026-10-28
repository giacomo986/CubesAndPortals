#pragma once

#include "core/item/slot_rules.h"
#include "core/media/face_tiles.h"
#include "core/registry/model_registry.h"
#include "core/voxel/chunk.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace cnp {

// What a cell's facing byte means for this block (TODO.md OPEN-27).
//
//   None         the byte is 0 and nothing turns
//   Upright      0..3 quarter turns about the vertical -- a chest, a furnace
//   Facedir      0..23, any of the twenty-four orientations of a cube -- a log
//                lying down, a stair, a piston
//   Wallmounted  0..5, the FACE the block's front points at -- a ladder, a sign
//   Height       1..32, how full the cell is from the floor up, in 32nds -- the
//                cells under a track piece's road (core/track/piece.h)
//
// Upright is a subset of Facedir and not a separate rule: orientations 0..3 ARE
// the four upright turns (media/face_tiles.h says why the numbering is that way
// round). A block declares the smaller kind when the smaller kind is what it
// means, so that a refusal can say so -- a chest turned onto its side is a
// mistake somebody should be told about rather than a chest on its side.
enum class NodeTurn : uint8_t {
    None = 0,
    Upright,
    Facedir,
    Wallmounted,
    Height,
};

// A piece of a track (core/track/piece.h): a block that is the corner cell of a
// smooth road several nodes across, the rest of which is `fill` blocks. What a
// kart drives on is the road itself; what a foot stands on and the server
// checks against are the fill cells, each a box under the road. `plan` 0 is
// every block that is not a piece.
struct PieceShape {
    uint8_t plan   = 0;   // 0 none, 1 straight, 2 a quarter turn to the right,
                          // 3 and 4 a chicane: one square across, to the right and the left,
                          // 5 a quarter turn to the left
    uint8_t size   = 1;   // in units of 8 nodes: a straight's length, a turn's radius
    int8_t  climb  = 0;   // nodes the road rises from its entry to its exit
    int8_t  grade0 = 0;   // and how steeply it leaves the entry, in nodes per unit ...
    int8_t  grade1 = 0;   // ... and arrives at the exit
    int8_t  bank0  = 0;   // nodes the left edge stands above the right, at the entry ...
    int8_t  bank1  = 0;   // ... and at the exit (negative: the right edge is higher)
    uint8_t curbs  = 1;   // a raised edge either side; 0 is a platform
    // A wall ride (straights only): bank0 and bank1 are then how far the road
    // is rolled about its middle, in degrees -- 90 stands it up with its left
    // edge four nodes up and its right four down, -90 the other way.
    uint8_t wall   = 0;
    uint8_t width  = 1;   // squares across: 1, or 2 for a wide road
    BlockId fill   = kAir;
};

inline constexpr int kFaceCount = 6;   // +X, -X, +Y, -Y, +Z, -Z

// What a block is, as data.
//
// Everything the engine asks about a block goes through this struct, and a mod
// fills it in from Lua. That is the difference between "you can add a texture"
// and "you can add a block": nothing in the mesher, the lighting or the
// renderer knows the id of stone.
//
// The fields here are the ones M1 already needed. Traits proper -- a mod
// declaring its own property that the engine consults through a dispatch table
// (DESIGN.md 6.2) -- arrive with the ECS in M8. This is deliberately the small
// version: a fixed set of properties the core understands, not yet an open one.
struct NodeDef {
    std::string name;               // "core:stone", namespaced; the id is an implementation detail

    // Which mod registered it, empty for the engine's own. Reloading a mod
    // needs to tell that mod's blocks from everybody else's, and the name's
    // prefix is a convention rather than a guarantee -- a mod may register
    // anything it likes.
    std::string owner;

    uint16_t tex[kFaceCount] = {0, 0, 0, 0, 0, 0};   // texture layer per face

    bool    solid        = true;    // stops movement
    bool    opaque       = true;    // stops light

    // How much of what is behind this block survives, 0..255. 255 draws in the
    // opaque pass and is the only value that costs nothing; anything less moves
    // the block into the sorted alpha pass, so it is a rendering decision
    // rather than a look. Kept separate from `opaque`, which is about LIGHT:
    // leaves stop neither and glass stops neither, but a stained window would
    // stop light and still be see-through.
    uint8_t opacity      = 255;
    uint8_t light_source = 0;       // 0..15, emitted block light

    // Water, and whatever else a mod makes one of (core.node_liquid): a body
    // goes through it and swims in it, light goes through it, and it is drawn
    // in the alpha pass. Never solid and never opaque while this is set.
    bool    liquid       = false;

    // Takes the season's colour (core.node_foliage, core.foliage): leaves that
    // turn in autumn. Only drawn: set on a player's machine by the world's code.
    bool    foliage      = false;

    // How full a cell of it is, in eighths: 8 is a whole cell, and less is a
    // surface lower than the top of the cell. A mod's water is a source (8) and
    // levels of water running off it (1..7), and this is how a client that
    // never saw the mod draws each of them at its own height, with a surface
    // that slopes from the fuller neighbour to the emptier one.
    uint8_t liquid_fill  = 8;

    // Whether the player can break it. The floor of the world says no: digging
    // through the last layer drops you out of the world, and a world you can
    // fall out of is a world with no floor, not a hard mode.
    bool    breakable    = true;

    // Falls when nothing solid is under it (core.node_falls): sand, gravel, an
    // anvil. The engine checks it on every write to its cell and to the cell
    // under it, and lets it go as a falling block (ecs::Falling).
    bool    falls        = false;

    // What a hand has to be holding for this to come out at all: the block does
    // not break below this level, however long anybody holds the button.
    //
    // Zero is a bare hand, which is what every block registered before this
    // asks for -- so adding it changes no block that exists. It is a LEVEL and
    // not a kind, and `material` below is what says which kind of work it is.
    //
    // Separate from `breakable`, and the two are not the same no. Bedrock is
    // not "a very hard block", it is the floor of the world, and no tool that
    // could ever exist takes it out.
    uint8_t dig_level    = 0;

    // How LONG it takes, in seconds, for a hand that digs at speed 1. Zero is
    // instant, which is what every block was before this and still is unless it
    // says otherwise.
    //
    // A duration and not a "hardness" out of which a duration is computed by a
    // formula nobody can read. What a mod wants to say is "this takes about two
    // seconds", and the tool divides it -- see ItemDef::dig_speed.
    float   hardness     = 0.0f;

    // A box inside the node, in node space: 0 is one wall of the cell and 1 is
    // the opposite one. A block that declares any of these is not a cube any
    // more -- see NodeDef::parts.
    struct Box {
        float min[3] = {0.0f, 0.0f, 0.0f};
        float max[3] = {1.0f, 1.0f, 1.0f};

        // Which part of the block this is, by a name the content chooses --
        // "lid", "door". Empty for a box that is simply part of the shape. It
        // is what lets a thing that MOVES be named without the engine learning
        // what a lid is.
        std::string part;
    };

    // The boxes this block is drawn as, or empty for a cube.
    //
    // A block with boxes is not meshed into the chunk at all: the mesher leaves
    // its cell alone and hands the cell to whoever draws the boxes, because the
    // chunk's vertex format has no room for them. Positions in it are bytes,
    // 0..32 inclusive, one per node -- eighths would need 0..256, which is one
    // value more than a byte holds. DESIGN.md 2.6 calls a finer fixed point "a
    // scale change rather than a format change"; it is that plus one, and it
    // would cost every chunk in the world memory it does not need for a handful
    // of chests.
    //
    // It also stops being OPAQUE, whatever it said: a box that fills part of a
    // cell hides none of its neighbour's faces, and a chest that hid them would
    // have a hole in the floor behind it.
    std::vector<Box> parts;

    // What this block is DRAWN as, when a cube and a set of boxes are not it: a
    // model, by its id in the world's model table (registry/model_registry.h) --
    // the same glTF pipeline a body wears, which is M11's `drawtype = mesh`.
    //
    // kNoModel is every block that has ever existed here and means "drawn from
    // this table": the boxes if it has any, a cube if it has not.
    //
    // The model is the PICTURE and the boxes are the block: a model is not
    // asked where a body may go or what a ray found, because a mesh is a
    // thousand triangles and a collision is a handful of boxes. A mod that
    // wants a shape under its model declares boxes as well, which is what
    // The original engine's `collision_box` beside `drawtype = mesh` is for.
    //
    // A client that has not got the file draws the boxes, or the cube: a
    // missing model is never a hole, which is the bargain the bodies already
    // strike (client/render/model_store.h).
    ModelId model = kNoModel;

    // What breaking it gives, when that is ANOTHER block's item: half of a
    // double chest gives a chest, a lit furnace a furnace. kAir is itself.
    // A block that drops as another has no item of its own, so nobody holds
    // "half a double chest" and the creative page does not list it.
    BlockId drops_as = kAir;

    // Or an item that is no block at all, by name: diamond ore gives a
    // diamond. Empty is the rule above. Such a block has no item of its own.
    std::string drops_item;

    // A body in a cell of it climbs: no gravity, up with jump, down slowly or
    // not at all while crouching -- a ladder, a vine. Travels (protocol 53):
    // a client predicting a climb has to know where one can be climbed.
    bool climbable = false;

    // A conveyor belt: what stands on it -- a stack, a player, a creature -- is
    // carried this many m/s the way an upright cell runs (away from whoever put
    // it down). 0 is no belt. Its top is `conveyor_height` over the cell's floor
    // where things come in and `conveyor_rise` more where they leave: a ramp.
    // Travels (protocol 78): a client carries its own player.
    float conveyor        = 0.0f;
    float conveyor_height = 1.0f;
    float conveyor_rise   = 0.0f;

    // A rail (core.node_conveyor's `rail`, protocol 110): a belt nobody is
    // carried by. A body in a cart (MovementRules::cart) rides it as a slide,
    // level or up as well as down, and `conveyor` is the speed it brings the
    // cart to -- 0 a brake.
    bool rail = false;

    // A stack lying on it does not grow older (and on a belt it never does):
    // a sorter. Things kept moving about a factory would otherwise vanish five
    // minutes after they first fell out of a chest (kDropLifetime).
    // Server-side, like the drops.
    bool keeps_drops = false;

    // What each square of a container at a cell of it is for (SlotRole in
    // core/item/inventory.h), core.container_roles, and what each role takes,
    // core.container_accepts. Empty: every square Store, taking anything.
    SlotRules slot_rules;

    // A door, for whatever finds a way through the world (core/voxel/
    // pathfind.h): 1 shut, 2 open, and `door_other` the block it becomes when
    // it is opened or shut. Server-side knowledge; nothing draws or moves by it.
    uint8_t door       = 0;
    BlockId door_other = kAir;

    // What this block is made of, as a name a mod chooses: "stone", "soil",
    // "wood". Empty is a block that does not say, and a block that does not say
    // is dug by any tool for whatever that tool is worth -- which is every
    // block registered before materials existed.
    //
    // A string and not an id, deliberately. It is read twice per dig and never
    // in a loop over cells, so interning it would buy nothing and would need a
    // table both registries and the wire agreed on. See ItemDef::digs for the
    // other half: a block says what it IS, a tool says what it is FOR.
    std::string material;

    // Colour of the generated placeholder texture, until real media arrives.
    uint8_t colour[3] = {180, 180, 180};
    bool    checker   = true;

    // ------------------------------------------------------------ turning
    //
    // A block with a FRONT: placing it turns the CELL it goes into to face
    // whoever put it down. The turn itself is a byte on the cell (Chunk's
    // facing plane), read by the mesher when it picks which picture goes on
    // which face; what the block declares is what that byte MEANS.
    //
    // This was four ids per block until OPEN-27, one per quarter turn, and the
    // three extra ones carried the base's pictures already permuted. That works
    // and it does not scale: twenty-four orientations would be twenty-four rows
    // for one block, and every caller asking "which block is this" had to
    // remember to ask base_of() first. A byte a cell costs 32768 bytes on a
    // chunk that uses one and nothing on a chunk that does not.
    NodeTurn turn = NodeTurn::None;

    // A track piece, or plan 0. See PieceShape.
    PieceShape piece;

    // Whether this block is the whole cell drawn as a cube: no boxes, no model,
    // no road. The one question behind "does it hide its neighbours' faces"
    // and "does the chunk mesher draw it".
    bool is_cube() const
    {
        return parts.empty() && model == kNoModel && turn != NodeTurn::Height && piece.plan == 0;
    }
};

// How many values a cell's facing byte may take for a block of each kind, and
// whether a given byte is one of them. One function, because the server refuses
// on it, the single-process client refuses on it, and a mod's core.set_facing
// refuses on it -- three places and a rule written three times is a rule a cell
// can be on both sides of.
inline int node_turn_values(NodeTurn turn)
{
    switch (turn) {
    case NodeTurn::None:        return 1;
    case NodeTurn::Upright:     return 4;
    case NodeTurn::Wallmounted: return 6;
    case NodeTurn::Facedir:     return kOrientations;
    case NodeTurn::Height:      return 33;
    }
    return 1;
}

inline bool node_turn_allows(NodeTurn turn, unsigned facing)
{
    return facing < (unsigned)node_turn_values(turn);
}

// The orientation a cell is actually in: the byte, except for a wallmounted
// block, whose byte is a FACE -- where its front points -- and not one of the
// twenty-four. The mesher asks this and nothing else, so the difference between
// the kinds lives here and not on its hot path.
inline int node_cell_orientation(NodeTurn turn, unsigned facing)
{
    if (!node_turn_allows(turn, facing)) return 0;
    if (turn == NodeTurn::Height) return 0;
    if (turn == NodeTurn::Wallmounted) return orientation_facing((int)facing);
    return (int)facing;
}

// The byte a PLACEMENT writes, for each kind.
//
// `dx`/`dz` are the cell's centre minus where the placer is standing -- their
// POSITION, which is the authority's own state, and not where they were
// looking: there is no look direction on the wire, and inventing one would be a
// field a modified client fills in. `n*` is the face they clicked, which IS on
// the wire, because the cell is derived from it.
//
// A facedir block gets one of the four upright turns from a placement and not
// one of the twenty-four. The other twenty exist for a mod (core.set_facing)
// and for whatever tool later turns a cell somebody has already placed; a
// placement has no way to say "on its side" that a player could aim.
inline uint8_t node_placement_facing(NodeTurn turn, float dx, float dz, int nx, int ny, int nz)
{
    switch (turn) {
    case NodeTurn::None:
        return 0;
    case NodeTurn::Height:
        return 32;
    case NodeTurn::Upright:
    case NodeTurn::Facedir:
        return (uint8_t)turn_towards(dx, dz);
    case NodeTurn::Wallmounted: {
        // Attached to the face that was clicked, so its front points back out
        // along that face's normal: the top of a block puts a ladder facing up,
        // the side of one puts it facing out of the wall.
        const int face = face_of_normal(nx, ny, nz);
        return (uint8_t)(face < 0 ? kFaceTop : face);
    }
    }
    return 0;
}

// The name a mod writes and the kind it means, in one place for the same reason
// as above: the client, the server and the texture baker all read the same
// manifest word.
inline const char *node_turn_name(NodeTurn turn)
{
    switch (turn) {
    case NodeTurn::None:        return "";
    case NodeTurn::Upright:     return "upright";
    case NodeTurn::Wallmounted: return "wallmounted";
    case NodeTurn::Facedir:     return "facedir";
    case NodeTurn::Height:      return "height";
    }
    return "";
}

// An unknown word is None and says so through `ok`. Refusing is the caller's
// decision -- a mod that misspelled a kind must hear about it, and the texture
// baker reading a world it did not build must not stop.
inline NodeTurn node_turn_from_name(const char *name, bool *ok = nullptr)
{
    const std::string s = (name ? name : "");
    if (ok) *ok = true;
    if (s.empty() || s == "none")  return NodeTurn::None;
    if (s == "upright")            return NodeTurn::Upright;
    if (s == "facedir")            return NodeTurn::Facedir;
    if (s == "wallmounted")        return NodeTurn::Wallmounted;
    if (s == "height")             return NodeTurn::Height;
    if (ok) *ok = false;
    return NodeTurn::None;
}

// The block table. Ids are dense and assigned in registration order, so the
// mesher can index straight into a flat array on its hot path rather than
// hashing a string per face.
class NodeRegistry {
public:
    NodeRegistry();   // registers core:air as id 0

    // Returns the id, or the existing id if the name is already taken -- in
    // which case the definition is replaced, which is what makes reloading a
    // mod possible without renumbering the world.
    BlockId register_node(const NodeDef &def);

    // A block nothing registered, under its own name: magenta, checkered and
    // solid, so the gap is obvious and nobody falls through it -- and an
    // orphan, so nobody can hold one. What a world file holds when the mod
    // that made a block is gone, and what a client is told of by a server
    // with blocks it lacks. The mod coming back gets the name back.
    BlockId register_unknown(const std::string &name);

    const NodeDef &get(BlockId id) const
    {
        return (id < m_defs.size()) ? m_defs[id] : m_defs[0];
    }

    // A name a saved world may still hold for a block that has another now.
    // Read only when a stored table is carried over (WorldDb::remap_blocks);
    // nothing else looks a block up by an old name.
    void alias(const std::string &old_name, const std::string &name) { m_alias[old_name] = name; }
    BlockId find_or_alias(const std::string &name) const
    {
        const BlockId id = find(name);
        const auto    it = m_alias.find(name);
        return (id != kAir || it == m_alias.end()) ? id : find(it->second);
    }

    BlockId find(const std::string &name) const
    {
        auto it = m_by_name.find(name);
        return (it == m_by_name.end()) ? kAir : it->second;
    }

    // Reloading a mod, in two calls around re-running its code. Whatever it
    // registered before and does not register again is an orphan: the id stays
    // -- the world is full of it -- but it becomes a loud placeholder rather
    // than silently keeping the old definition of a block the mod dropped.
    void   begin_reload(const std::string &owner);
    size_t end_reload(const std::string &owner);
    void   cancel_reload(const std::string &owner);   // the reload failed; nothing was dropped

    size_t size() const { return m_defs.size(); }
    const std::vector<NodeDef> &all() const { return m_defs; }

    // The two questions the voxel core asks constantly. Branch-free lookups
    // into flat arrays, kept in sync with the definitions, because these run
    // once per face per block.
    bool is_solid(BlockId id) const  { return id < m_solid.size()  && m_solid[id]  != 0; }
    bool is_opaque(BlockId id) const { return id < m_opaque.size() && m_opaque[id] != 0; }
    bool is_liquid(BlockId id) const { return id < m_liquid.size() && m_liquid[id] != 0; }
    bool is_climbable(BlockId id) const { return id < m_defs.size() && m_defs[id].climbable; }

    // In eighths of a cell, 1..8; 8 for anything that is not a liquid.
    uint8_t liquid_fill(BlockId id) const
    {
        return (id < m_defs.size() && m_defs[id].liquid) ? m_defs[id].liquid_fill : (uint8_t)8;
    }
    uint8_t light_source(BlockId id) const
    {
        return (id < m_light.size()) ? m_light[id] : 0;
    }

    bool is_breakable(BlockId id) const { return get(id).breakable; }

    // Which atlas tile each face wears. Written by whoever BUILT the atlas,
    // because that is the only place that knows how many distinct pictures
    // there turned out to be -- the registry cannot derive it, since two blocks
    // pointed at one file share a tile and a block with three pictures needs
    // three. Until somebody says, a node's tile is its own id, which is the
    // identity mapping the one-tile-per-node atlas had.
    //
    // Everything meshed from the old numbers is wrong the moment this changes,
    // so a caller sets these with a drained mesh pool and dirties the world
    // afterwards. WorldRenderer::rebuild_atlas does both.
    void set_face_tiles(BlockId id, const uint16_t tiles[kFaceCount])
    {
        if (id >= m_defs.size()) return;
        for (int f = 0; f < kFaceCount; ++f) m_defs[id].tex[f] = tiles[f];
    }

    // What a block's SHAPE is, set after the fact.
    //
    // This exists for one caller: a client adopting a server's block table
    // (core/net/client_session.cpp). A shape travels with the name since
    // protocol 27, and the server's is the one that counts -- a client whose
    // own copy of a mod draws a different slab is a client standing where the
    // world it is in says it cannot.
    //
    // Everything meshed from the old shape is wrong the moment this changes, so
    // it is called while the table is being built and not while a world is
    // being drawn from it.
    void set_shape(BlockId id, NodeTurn turn, std::vector<NodeDef::Box> boxes, ModelId model,
                   const PieceShape &piece = PieceShape{})
    {
        if (id >= m_defs.size()) return;
        m_defs[id].turn  = turn;
        m_defs[id].parts = std::move(boxes);
        m_defs[id].model = model;
        m_defs[id].piece = piece;

        // The same rule register_node applies: a block that is not a cube hides
        // none of its neighbours' faces. Written here too because this is the
        // other door into the table, and a block that arrived over the wire
        // with a shape would otherwise keep the opacity a placeholder was given.
        m_opaque[id] = (m_defs[id].opaque && m_defs[id].is_cube()) ? 1 : 0;
    }

    // How a block DIGS, set after the fact, for the same one caller: the
    // server's numbers since protocol 36, because the bar a client fills is
    // timed with them and the server breaks the block by its own.
    void set_dig(BlockId id, float hardness, uint8_t dig_level, std::string material)
    {
        if (id >= m_defs.size()) return;
        m_defs[id].hardness  = hardness;
        m_defs[id].dig_level = dig_level;
        m_defs[id].material  = std::move(material);
    }

    // What a block does to a body and to light, set after the fact, for the same
    // one caller: the server's since protocol 38. Before it a client knew a mod's
    // block only by its name and shape, so a block the server let bodies through
    // was a wall on every client without that mod -- and a lake was a floor.
    // And the light it gives (protocol 84): without it a client that relit
    // round a change counted a mod's lamp as dark, and a room went black.
    void set_light_source(BlockId id, uint8_t light)
    {
        if (id >= m_defs.size()) return;
        m_defs[id].light_source = light > 15 ? 15 : light;
        if (id < m_light.size()) m_light[id] = m_defs[id].light_source;
    }

    void set_conveyor(BlockId id, float speed, float height, float rise, bool rail = false)
    {
        if (id >= m_defs.size()) return;
        m_defs[id].conveyor        = speed;
        m_defs[id].conveyor_height = height;
        m_defs[id].conveyor_rise   = rise;
        m_defs[id].rail            = rail;
    }

    void set_body(BlockId id, bool solid, bool opaque, uint8_t opacity, bool liquid,
                  uint8_t fill = 8, bool climbable = false)
    {
        if (id >= m_defs.size()) return;
        m_defs[id].climbable   = climbable;
        m_defs[id].solid       = solid;
        m_defs[id].opaque      = opaque;
        m_defs[id].opacity     = opacity;
        m_defs[id].liquid      = liquid;
        m_defs[id].liquid_fill = fill;
        m_solid[id]   = solid ? 1 : 0;
        m_opaque[id]  = (opaque && m_defs[id].is_cube()) ? 1 : 0;
        m_opacity[id] = opacity;
        m_liquid[id]  = liquid ? 1 : 0;
    }

    // Whether a reload dropped this block. The id and the name stay -- the
    // world is full of it -- but nothing derived from it should behave as if
    // the mod still describes it. ItemRegistry asks this to tell a block that
    // survived a reload from one that only looks alive because its name is
    // still in the table.
    bool is_orphan(BlockId id) const { return id < m_orphan.size() && m_orphan[id] != 0; }

    uint8_t opacity(BlockId id) const
    {
        return (id < m_opacity.size()) ? m_opacity[id] : 255;
    }
    // Draws in the alpha pass. Air is not translucent -- it is nothing at all,
    // and a mesher that treats it as translucent emits faces for empty space.
    bool is_translucent(BlockId id) const { return id != kAir && opacity(id) < 255; }

private:
    std::vector<NodeDef> m_defs;
    std::unordered_map<std::string, BlockId> m_by_name;
    std::unordered_map<std::string, std::string> m_alias;

    std::vector<uint8_t> m_stale;   // set by begin_reload, cleared by re-registration
    std::vector<uint8_t> m_orphan;  // set by end_reload, cleared by re-registration
    std::vector<uint8_t> m_solid;
    std::vector<uint8_t> m_opaque;
    std::vector<uint8_t> m_light;
    std::vector<uint8_t> m_opacity;
    std::vector<uint8_t> m_liquid;
};

} // namespace cnp
