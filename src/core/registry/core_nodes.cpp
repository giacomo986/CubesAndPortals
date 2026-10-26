#include "core/registry/core_nodes.h"

namespace cnp {

const std::vector<std::string> &core_node_names()
{
    // The order below is the order in register_core_nodes, and the two are
    // checked against each other by the worldgen test rather than kept in step
    // by hand.
    static const std::vector<std::string> names = {
        "core:stone",
        "core:grass",
        "core:bedrock",
        "core:frame",
        "core:glass",
        "core:dirt",
        "core:sand",
        "core:snow",
    };
    return names;
}

uint64_t registry_content_hash(const NodeRegistry &reg)
{
    // FNV-1a over every name in id order, with a separator so that "ab","c" and
    // "a","bc" are different tables rather than the same byte stream.
    uint64_t h = 1469598103934665603ull;

    for (size_t id = 0; id < reg.size(); ++id) {
        const std::string &name = reg.get((BlockId)id).name;
        for (const char c : name) {
            h ^= (uint64_t)(unsigned char)c;
            h *= 1099511628211ull;
        }
        h ^= 0xFFull;   // separator
        h *= 1099511628211ull;
    }
    return h;
}

std::vector<std::string> node_table(const NodeRegistry &reg)
{
    std::vector<std::string> names;
    names.reserve(reg.size());
    for (size_t id = 0; id < reg.size(); ++id) names.push_back(reg.get((BlockId)id).name);
    return names;
}

size_t adopt_node_table(NodeRegistry &dst, const std::vector<std::string> &names,
                        const NodeRegistry &local)
{
    dst = NodeRegistry{};   // starts with core:air at id 0, as every registry does

    size_t unknown = 0;

    // Registered in the order the names arrive, which is what makes the ids
    // come out identical: the registry assigns the next free id each time, so
    // walking the table in order reproduces it exactly.
    for (size_t i = 0; i < names.size(); ++i) {
        const std::string &name = names[i];
        if (i == 0) continue;   // air, already there

        const BlockId known = local.find(name);
        if (known != kAir) {
            NodeDef def = local.get(known);
            def.name    = name;
            dst.register_node(def);
            continue;
        }

        // A block this build has never heard of: the world stays walkable and
        // the gap is obvious rather than invisible.
        dst.register_unknown(name);
        ++unknown;
    }

    return unknown;
}

CoreNodes register_core_nodes(NodeRegistry &reg)
{
    CoreNodes ids;

    // The hardnesses below are SECONDS at a bare hand's speed, and they are the
    // first values in this game that make digging cost anything at all -- the
    // clock was built in OPEN-31 and then nothing declared a number for it, so
    // every block still came away in one frame.
    //
    // None of them asks for a tool. dig_level stays 0 on every engine block on
    // purpose: the tools that beat a hand are content and live in a mod, so a
    // core block that demanded one would be a block nobody could dig with the
    // mods turned off. What a level is for is in mods/tools.
    //
    // They do say what they are MADE of, and that is not a level: "stone",
    // "soil", "wood", "glass" are what a tool is FOR, so a shovel is quick
    // through dirt and no help at all against a wall. Four names and not one
    // per block -- a material every block has to itself is a table where every
    // tool is its own tool, which is a list and not a rule.
    NodeDef stone;
    stone.name = "core:stone";
    stone.colour[0] = 150; stone.colour[1] = 150; stone.colour[2] = 158;
    stone.hardness  = 1.5f;   // the slowest thing that ships: a wall is work
    stone.material  = "stone";
    ids.stone = reg.register_node(stone);

    NodeDef grass;
    grass.name = "core:grass";
    grass.colour[0] = 110; grass.colour[1] = 170; grass.colour[2] = 90;
    grass.hardness  = 0.6f;
    grass.material  = "soil";
    ids.grass = reg.register_node(grass);

    // The floor of the world. Somebody dug straight down to y=0 within a minute
    // of picking the game up, went through the last layer and fell out of the
    // world -- so there is a layer now that does not break.
    NodeDef bedrock;
    bedrock.name = "core:bedrock";
    bedrock.colour[0] = 60; bedrock.colour[1] = 58; bedrock.colour[2] = 66;
    bedrock.breakable = false;
    // And no hardness, deliberately: a duration is not a refusal. Bedrock made
    // unbreakable by being very slow is bedrock somebody digs through in an
    // afternoon, and the refusal above is asked first anyway. No material
    // either: nothing is ever the right tool for the floor of the world.
    ids.bedrock = reg.register_node(bedrock);

    NodeDef frame;
    frame.name = "core:frame";
    frame.colour[0] = 170; frame.colour[1] = 140; frame.colour[2] = 100;
    frame.checker = false;
    frame.hardness = 0.9f;
    frame.material = "wood";
    ids.frame = reg.register_node(frame);

    // The first block in this engine that is not one colour or nothing.
    //
    // Solid, so it holds a player up; not opaque, so light goes through it and
    // the room behind a window is lit; and an opacity below 255, which is what
    // moves it out of the opaque pass into the sorted one. Three separate
    // properties, on purpose -- the fourth combination, a stained window that
    // stops light and still shows through, is a legal node here and would be
    // one flag away.
    NodeDef glass;
    glass.name = "core:glass";
    glass.colour[0] = 205; glass.colour[1] = 235; glass.colour[2] = 245;
    glass.checker = false;
    glass.opaque  = false;
    glass.opacity = 110;
    glass.hardness = 0.35f;   // brittle, but not free
    glass.material = "glass";
    ids.glass = reg.register_node(glass);

    // The biome blocks. Nothing in the engine treats them specially -- they are
    // a colour, a name and how long they take, and the mapgen looks them up by
    // name like a mod would.
    NodeDef dirt;
    dirt.name = "core:dirt";
    dirt.colour[0] = 134; dirt.colour[1] = 96; dirt.colour[2] = 67;
    dirt.hardness  = 0.45f;
    dirt.material  = "soil";
    ids.dirt = reg.register_node(dirt);

    NodeDef sand;
    sand.name = "core:sand";
    sand.colour[0] = 214; sand.colour[1] = 198; sand.colour[2] = 142;
    sand.hardness  = 0.3f;
    sand.material  = "soil";
    ids.sand = reg.register_node(sand);

    NodeDef snow;
    snow.name = "core:snow";
    snow.colour[0] = 232; snow.colour[1] = 238; snow.colour[2] = 245;
    snow.hardness  = 0.15f;   // loose ground comes up in a moment
    snow.material  = "soil";
    ids.snow = reg.register_node(snow);

    return ids;
}

} // namespace cnp
