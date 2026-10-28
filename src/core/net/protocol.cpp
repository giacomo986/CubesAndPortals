#include <cstring>
#include "core/net/protocol.h"

#include "core/item/item_registry.h"
#include "core/track/piece.h"

#include "core/media/face_tiles.h"
#include "core/ui/panel.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdlib>

namespace cnp {
namespace net {

// One number, not two. A wire cap that drifted below the model's would truncate
// a panel with no error anywhere -- see kMaxWirePanelElements.
static_assert(kMaxWirePanelElements == ui::kMaxElements,  "wire and model disagree about elements");
static_assert(kMaxWirePanelText     == ui::kMaxPanelText, "wire and model disagree about text");

const char *scope_text(Scope s)
{
    switch (s) {
    case Scope::Enter:   return "player";
    case Scope::Observe: return "observer";
    }
    return "unknown";
}

const char *refuse_reason_text(RefuseReason r)
{
    switch (r) {
    case RefuseReason::None:           return "no reason";
    case RefuseReason::BadMagic:       return "not this protocol";
    case RefuseReason::VersionTooOld:  return "client is too old for this server";
    case RefuseReason::VersionTooNew:  return "server is too old for this client";
    case RefuseReason::BadName:        return "unusable player name";
    case RefuseReason::Malformed:      return "malformed handshake";
    case RefuseReason::ServerFull:     return "server is full";
    case RefuseReason::MissingFeature: return "client lacks a feature this server requires";
    case RefuseReason::ContentMismatch: return "client and server have different block tables";
    case RefuseReason::BadIdentity:    return "the player's identity was not proven";
    case RefuseReason::NotWatchable:   return "this world does not allow observers";
    case RefuseReason::NoSuchOpening:  return "this world has no opening with that number";
    case RefuseReason::ArrivalBlocked: return "the way out of that opening is blocked";
    case RefuseReason::BadTicket:      return "no ticket this world will honour";
    case RefuseReason::NoSuchRoom:     return "this server holds no world by that name";
    case RefuseReason::Banned:         return "banned here";
    case RefuseReason::NotAllowed:     return "not on this server's list";
    case RefuseReason::Kicked:         return "kicked";
    case RefuseReason::Restarting:     return "the server is restarting";
    case RefuseReason::Stopping:       return "the server stopped";
    case RefuseReason::Maintenance:    return "under maintenance";
    }
    return "unknown";
}

bool refusal_is_permanent(RefuseReason r)
{
    switch (r) {
    case RefuseReason::ServerFull:
    case RefuseReason::Restarting:
        return false;   // a moment, not a decision

    case RefuseReason::None:
    case RefuseReason::BadMagic:
    case RefuseReason::VersionTooOld:
    case RefuseReason::VersionTooNew:
    case RefuseReason::BadName:
    case RefuseReason::Malformed:
    case RefuseReason::MissingFeature:
    case RefuseReason::ContentMismatch:
    case RefuseReason::BadIdentity:
    case RefuseReason::NotWatchable:
    case RefuseReason::NoSuchOpening:
    case RefuseReason::ArrivalBlocked:
    case RefuseReason::BadTicket:
    case RefuseReason::NoSuchRoom:
    case RefuseReason::Banned:
    case RefuseReason::NotAllowed:
    case RefuseReason::Kicked:
    case RefuseReason::Stopping:
    case RefuseReason::Maintenance:
        return true;
    }
    // A reason from something newer. Treated as permanent, because retrying
    // something this build cannot name is the worse of the two guesses.
    return true;
}

bool valid_player_name(const std::string &name)
{
    if (name.empty() || name.size() > kMaxNameLength) return false;
    for (const char c : name) {
        const unsigned char u = (unsigned char)c;
        // Printable ASCII only, and no space at either end. Control characters
        // in a name are how a log line becomes two log lines.
        if (u < 0x21 || u > 0x7E) return false;
    }
    return true;
}

// --- Hello ------------------------------------------------------------------

std::vector<uint8_t> Hello::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Hello);
    w.u32(magic);
    w.u16(protocol);
    w.u16(min_protocol);
    w.str(engine_version);
    w.u64(content_hash);
    w.str(player_name);

    const size_t n = features.size() > kMaxFeatures ? kMaxFeatures : features.size();
    w.u16((uint16_t)n);
    for (size_t i = 0; i < n; ++i) w.str(features[i]);

    // Length-prefixed rather than fixed: "no identity" has to be a state the
    // wire can say, and a fixed 32 zero bytes would be an identity somebody
    // could claim to hold the key for.
    w.u16((uint16_t)identity.size());
    for (uint8_t b : identity) w.u8(b);
    w.u16((uint16_t)proof.size());
    for (uint8_t b : proof) w.u8(b);
    w.u8((uint8_t)scope);

    w.u8(has_ticket ? 1 : 0);
    if (has_ticket) {
        const auto blob = ticket.encode();
        w.bytes(blob.data(), blob.size());
    }
    w.str(room);
    w.u32(livery);

    return w.data();
}

bool Hello::decode(ByteReader &r, Hello &out)
{
    out = Hello{};

    if (r.u8() != (uint8_t)MsgType::Hello) return false;

    out.magic        = r.u32();
    out.protocol     = r.u16();
    out.min_protocol = r.u16();
    if (!r.str(out.engine_version, kMaxVersionString)) return false;
    out.content_hash = r.u64();
    if (!r.str(out.player_name, kMaxNameLength)) return false;

    size_t n = 0;
    if (!r.count(n, kMaxFeatures)) return false;
    out.features.reserve(n);   // n is capped above, so this cannot be a lie
    for (size_t i = 0; i < n; ++i) {
        std::string f;
        if (!r.str(f, kMaxFeatureName)) return false;
        out.features.push_back(std::move(f));
    }

    size_t id_len = 0;
    if (!r.count(id_len, kSignPublicBytes)) return false;
    out.identity.resize(id_len);
    for (size_t i = 0; i < id_len; ++i) out.identity[i] = r.u8();

    size_t proof_len = 0;
    if (!r.count(proof_len, kSignatureBytes)) return false;
    out.proof.resize(proof_len);
    for (size_t i = 0; i < proof_len; ++i) out.proof[i] = r.u8();

    const uint8_t scope = r.u8();
    if (scope > (uint8_t)Scope::Observe) return false;
    out.scope = (Scope)scope;

    const uint8_t has_ticket = r.u8();
    if (has_ticket > 1) return false;
    out.has_ticket = has_ticket == 1;
    if (out.has_ticket && !PortalTicket::decode(r, out.ticket)) return false;

    if (!r.str(out.room, kMaxRoomName)) return false;
    out.livery = r.u32();
    if (!r.ok() || out.livery >= kLiveryMax) return false;

    // Half an identity is not an identity. A key with no proof is a claim
    // anybody can make about anybody, and a proof with no key is 64 bytes of
    // nothing -- both are refused here rather than left for the verifier to
    // trip over.
    if (id_len != 0 && id_len != kSignPublicBytes) return false;
    if (proof_len != 0 && proof_len != kSignatureBytes) return false;
    if ((id_len == 0) != (proof_len == 0)) return false;

    // Trailing bytes mean the sender wrote something this parser did not read.
    // Treated as malformed rather than ignored: a message half understood is
    // how a protocol drifts into two protocols.
    return r.done();
}

std::vector<uint8_t> identity_challenge(const uint8_t binding[32], const std::string &name)
{
    static constexpr char kLabel[] = "cnp1 player identity";

    std::vector<uint8_t> m;
    m.reserve(32 + sizeof(kLabel) + name.size());
    m.insert(m.end(), binding, binding + 32);
    m.insert(m.end(), (const uint8_t *)kLabel, (const uint8_t *)kLabel + sizeof(kLabel) - 1);
    m.insert(m.end(), name.begin(), name.end());
    return m;
}

// --- Welcome ----------------------------------------------------------------

std::vector<uint8_t> Welcome::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Welcome);
    w.u32(magic);
    w.u16(protocol);
    w.str(engine_version);
    w.u64(content_hash);
    w.u32(seed);
    w.u64(mapgen_hash);

    const size_t nf = features.size() > kMaxFeatures ? kMaxFeatures : features.size();
    w.u16((uint16_t)nf);
    for (size_t i = 0; i < nf; ++i) w.str(features[i]);

    const size_t nc = channels.size() > kMaxChannels ? (size_t)kMaxChannels : channels.size();
    w.u16((uint16_t)nc);
    for (size_t i = 0; i < nc; ++i) {
        w.str(channels[i].name);
        w.u8(channels[i].id);
    }

    const size_t nn = nodes.size() > kMaxNodeTable ? kMaxNodeTable : nodes.size();
    w.u16((uint16_t)nn);
    for (size_t i = 0; i < nn; ++i) w.str(nodes[i]);

    w.u8(has_signing_key ? 1 : 0);
    if (has_signing_key) w.bytes(signing_key, kSignPublicBytes);
    w.str(room);
    w.u8(has_sky ? 1 : 0);
    if (has_sky) w.bytes(sky, 3);
    w.u8(has_storm ? 1 : 0);
    if (has_storm) w.bytes(storm, 3);

    const size_t ni = items.size() > kMaxItemTable ? kMaxItemTable : items.size();
    w.u16((uint16_t)ni);
    for (size_t i = 0; i < ni; ++i) {
        w.str(items[i].name);
        w.u16(items[i].places);
        w.u16(items[i].max_stack);
        w.u8(items[i].dig_level);   // and how it digs, since protocol 36
        w.f32(items[i].dig_speed);
        w.str(items[i].digs);
        w.u8(items[i].wear);   // and what it is worn as, since protocol 58
        w.u8(items[i].armor);
        w.u16(items[i].durability);
        w.u8(items[i].arms);   // since protocol 95
    }

    // How each block digs, by id, since protocol 36.
    const size_t nd = node_digs.size() > kMaxNodeTable ? kMaxNodeTable : node_digs.size();
    w.u16((uint16_t)nd);
    for (size_t i = 0; i < nd; ++i) {
        w.f32(node_digs[i].hardness);
        w.u8(node_digs[i].dig_level);
        w.str(node_digs[i].material);
    }

    // What each block does to a body and to light, by id, since protocol 38.
    const size_t nbody = node_bodies.size() > kMaxNodeTable ? kMaxNodeTable : node_bodies.size();
    w.u16((uint16_t)nbody);
    for (size_t i = 0; i < nbody; ++i) {
        const NodeBodyDesc &b = node_bodies[i];
        w.u8((uint8_t)((b.solid ? 1 : 0) | (b.opaque ? 2 : 0) | (b.liquid ? 4 : 0) |
                       (b.climbable ? 8 : 0) | (b.conveyor > 0.0f && !b.rail ? 16 : 0) |
                       (b.rail ? 32 : 0)));
        w.u8(b.opacity);
        w.u8(b.fill);
        w.u8(b.light);
        if (b.conveyor > 0.0f || b.rail) {   // a belt, since protocol 78, or a rail (110)
            w.f32(b.conveyor);
            w.f32(b.conveyor_height);
            w.f32(b.conveyor_rise);
        }
    }

    const size_t nm = models.size() > kMaxModelTable ? kMaxModelTable : models.size();
    w.u16((uint16_t)nm);
    for (size_t i = 0; i < nm; ++i) w.str(models[i]);

    // The shapes go LAST, after both tables they name: a shape carries the id
    // of the block it belongs to and the id of the model it is drawn as, and a
    // reader that has not got those tables yet cannot tell a number past the end
    // of one from a number inside it.
    const size_t ns = shapes.size() > kMaxNodeShapes ? kMaxNodeShapes : shapes.size();
    w.u16((uint16_t)ns);
    for (size_t i = 0; i < ns; ++i) {
        const NodeShapeDesc &d = shapes[i];
        w.u16(d.node);
        w.u8(d.turn);
        w.u16(d.model);

        // A u16 and not a u8, because the reader's count() reads a u16 and a
        // length written one width and read another is a message that parses
        // as garbage rather than failing.
        const size_t nb = d.boxes.size() > kMaxNodeBoxes ? kMaxNodeBoxes : d.boxes.size();
        w.u16((uint16_t)nb);
        for (size_t b = 0; b < nb; ++b) {
            for (int a = 0; a < 3; ++a) w.f32(d.boxes[b].min[a]);
            for (int a = 0; a < 3; ++a) w.f32(d.boxes[b].max[a]);
            w.str(d.boxes[b].part);
        }
        const PieceShape &p = d.piece;
        for (const int8_t v : {(int8_t)p.plan, (int8_t)p.size, p.climb, p.grade0, p.grade1, p.bank0,
                               p.bank1, (int8_t)p.curbs, (int8_t)p.wall, (int8_t)p.width})
            w.u8((uint8_t)v);
        w.u16(p.fill);
    }

    // How a body moves here, since protocol 28. Floats as they are held, for
    // the reason the corners above are: the client moves by these and the
    // server checks by them, and two ends rounding differently are two ends
    // that disagree about a sprint.
    w.f32(movement.walk);
    w.f32(movement.sprint);
    w.f32(movement.fly);
    w.f32(movement.jump);
    w.f32(movement.gravity);

    // Whether bodies here drive, since protocol 33 (core.drive).
    w.f32(movement.accel);
    w.f32(movement.brake);
    w.f32(movement.turn);
    w.f32(movement.drag);

    // How wide a body is, since protocol 35 (core.body_width), and how long,
    // since 44.
    w.f32(movement.width);
    w.f32(movement.length);

    // How high above where it stood a body may be, since protocol 45.
    w.f32(movement.rise);

    // How high its eye is, since protocol 50.
    w.f32(movement.eye);

    // Whether bodies here do parkour, since protocol 87 (bit 0), and hang
    // from no ledge, since 101 (bit 1).
    w.u8((movement.parkour ? 1 : 0) | (movement.ledges ? 0 : 2));

    // What the player's own body wears, since protocol 32.
    w.u16(player_model);

    return w.data();
}

bool Welcome::decode(ByteReader &r, Welcome &out)
{
    out = Welcome{};

    if (r.u8() != (uint8_t)MsgType::Welcome) return false;

    out.magic      = r.u32();
    out.protocol = r.u16();
    if (!r.str(out.engine_version, kMaxVersionString)) return false;
    out.content_hash = r.u64();
    out.seed         = r.u32();
    out.mapgen_hash  = r.u64();

    size_t nf = 0;
    if (!r.count(nf, kMaxFeatures)) return false;
    out.features.reserve(nf);
    for (size_t i = 0; i < nf; ++i) {
        std::string f;
        if (!r.str(f, kMaxFeatureName)) return false;
        out.features.push_back(std::move(f));
    }

    size_t nc = 0;
    if (!r.count(nc, kMaxChannels)) return false;
    out.channels.reserve(nc);
    for (size_t i = 0; i < nc; ++i) {
        ChannelAssignment a;
        if (!r.str(a.name, kMaxFeatureName)) return false;
        a.id = r.u8();
        // A server that hands out channel 1 for a mod would have it collide
        // with chunk streaming. The client checks rather than trusting: it is
        // the side that will index an array with this number.
        if (!r.ok() || a.id < kModChannelBase || a.id >= kMaxChannels) return false;
        out.channels.push_back(std::move(a));
    }

    size_t nn = 0;
    if (!r.count(nn, kMaxNodeTable)) return false;
    out.nodes.reserve(nn);
    for (size_t i = 0; i < nn; ++i) {
        std::string name;
        if (!r.str(name, kMaxNodeName)) return false;
        out.nodes.push_back(std::move(name));
    }

    out.has_signing_key = r.u8() != 0;
    if (out.has_signing_key && !r.bytes(out.signing_key, kSignPublicBytes)) return false;
    if (!r.str(out.room, kMaxRoomName)) return false;
    const uint8_t has_sky = r.u8();
    if (has_sky > 1) return false;
    out.has_sky = has_sky == 1;
    if (out.has_sky && !r.bytes(out.sky, 3)) return false;
    const uint8_t has_storm = r.u8();
    if (has_storm > 1) return false;
    out.has_storm = has_storm == 1;
    if (out.has_storm && !r.bytes(out.storm, 3)) return false;

    size_t ni = 0;
    if (!r.count(ni, kMaxItemTable)) return false;
    out.items.reserve(ni);
    for (size_t i = 0; i < ni; ++i) {
        ItemDesc d;
        if (!r.str(d.name, kMaxNodeName)) return false;
        d.places    = r.u16();
        d.max_stack = r.u16();
        d.dig_level = r.u8();
        d.dig_speed = r.f32();
        if (!r.str(d.digs, kMaxNodeName)) return false;
        d.wear       = r.u8();
        d.armor      = r.u8();
        d.durability = r.u16();
        d.arms       = r.u8();
        if (!r.ok() || !std::isfinite(d.dig_speed) || d.wear > kWornBack || d.arms > kMaxArms)
            return false;
        out.items.push_back(std::move(d));
    }

    size_t nd = 0;
    if (!r.count(nd, kMaxNodeTable)) return false;
    out.node_digs.reserve(nd);
    for (size_t i = 0; i < nd; ++i) {
        NodeDigDesc d;
        d.hardness  = r.f32();
        d.dig_level = r.u8();
        if (!r.str(d.material, kMaxNodeName)) return false;
        // A duration: negative or not a number is a bar that never fills.
        if (!r.ok() || !std::isfinite(d.hardness) || d.hardness < 0.0f) return false;
        out.node_digs.push_back(std::move(d));
    }

    size_t nbody = 0;
    if (!r.count(nbody, kMaxNodeTable)) return false;
    out.node_bodies.reserve(nbody);
    for (size_t i = 0; i < nbody; ++i) {
        const uint8_t flags = r.u8();
        NodeBodyDesc  body;
        body.opacity = r.u8();
        body.fill    = r.u8();
        body.light   = r.u8();
        // Six flags and nothing else (climbable since 53, conveyor since 78, rail
        // since 110): a bit past them is a writer that means something this
        // reader does not know. And a fill of 1..8, because an empty cell of
        // water is air and a fuller one than a cell is nothing.
        if (!r.ok() || flags > 63 || (flags & 48) == 48 || body.fill < 1 || body.fill > 8 ||
            body.light > 15)
            return false;
        body.solid  = (flags & 1) != 0;
        body.opaque = (flags & 2) != 0;
        body.liquid = (flags & 4) != 0;
        body.climbable = (flags & 8) != 0;
        if ((flags & 16) != 0) {   // a belt (78): a speed that goes somewhere, and a top
            body.conveyor        = r.f32();
            body.conveyor_height = r.f32();
            body.conveyor_rise   = r.f32();
            if (!r.ok() || !std::isfinite(body.conveyor) || body.conveyor <= 0.0f ||
                !std::isfinite(body.conveyor_height) || !std::isfinite(body.conveyor_rise))
                return false;
        }
        if ((flags & 32) != 0) {   // a rail (110): the speed it brings a cart to, 0 a brake
            body.rail            = true;
            body.conveyor        = r.f32();
            body.conveyor_height = r.f32();
            body.conveyor_rise   = r.f32();
            if (!r.ok() || !std::isfinite(body.conveyor) || body.conveyor < 0.0f ||
                !std::isfinite(body.conveyor_height) || !std::isfinite(body.conveyor_rise))
                return false;
        }
        out.node_bodies.push_back(body);
    }

    size_t nm = 0;
    if (!r.count(nm, kMaxModelTable)) return false;
    out.models.reserve(nm);
    for (size_t i = 0; i < nm; ++i) {
        std::string name;
        if (!r.str(name, kMaxNodeName)) return false;
        out.models.push_back(std::move(name));
    }

    // The shapes last, after both the tables they name.
    size_t ns = 0;
    if (!r.count(ns, kMaxNodeShapes)) return false;
    out.shapes.reserve(ns);
    for (size_t i = 0; i < ns; ++i) {
        NodeShapeDesc d;
        d.node  = r.u16();
        d.turn  = r.u8();
        d.model = r.u16();

        // An id past the end of the table it indexes: refused here rather than
        // carried to whoever indexes an array with it. A model id is a position
        // PLUS ONE, like a body's, so the table's own size is a legal value and
        // zero means "drawn from the block table".
        if (!r.ok() || d.node >= out.nodes.size()) return false;
        if (d.model > out.models.size()) return false;

        size_t nb = 0;
        if (!r.count(nb, kMaxNodeBoxes)) return false;
        d.boxes.resize(nb);
        for (size_t b = 0; b < nb; ++b) {
            for (int a = 0; a < 3; ++a) d.boxes[b].min[a] = r.f32();
            for (int a = 0; a < 3; ++a) d.boxes[b].max[a] = r.f32();
            if (!r.str(d.boxes[b].part, kMaxNodePartName)) return false;
        }
        PieceShape &p = d.piece;
        p.plan   = r.u8();
        p.size   = r.u8();
        p.climb  = (int8_t)r.u8();
        p.grade0 = (int8_t)r.u8();
        p.grade1 = (int8_t)r.u8();
        p.bank0  = (int8_t)r.u8();
        p.bank1  = (int8_t)r.u8();
        p.curbs  = r.u8();
        p.wall   = r.u8();
        p.width  = r.u8();
        p.fill   = r.u16();
        if (!r.ok()) return false;
        // A piece that is no piece, or whose fill is past the table: refused
        // here, where the reader still knows it came off a wire.
        if (p.plan != 0 && (!piece_valid(p) || p.fill >= out.nodes.size())) return false;
        out.shapes.push_back(std::move(d));
    }

    out.movement.walk    = r.f32();
    out.movement.sprint  = r.f32();
    out.movement.fly     = r.f32();
    out.movement.jump    = r.f32();
    out.movement.gravity = r.f32();
    out.movement.accel   = r.f32();
    out.movement.brake   = r.f32();
    out.movement.turn    = r.f32();
    out.movement.drag    = r.f32();
    out.movement.width   = r.f32();
    out.movement.length  = r.f32();
    out.movement.rise    = r.f32();
    out.movement.eye     = r.f32();
    const uint8_t moves  = r.u8();
    out.movement.parkour = (moves & 1) != 0;
    out.movement.ledges  = (moves & 2) == 0;
    if (!r.ok() || !movement_rules_problem(out.movement).empty()) return false;

    // A model this welcome does not list is refused, as a body's is: the id is
    // a position in the table above plus one.
    out.player_model = r.u16();
    if (!r.ok() || out.player_model > out.models.size()) return false;

    return r.done();
}

// --- Move -------------------------------------------------------------------

std::vector<uint8_t> Move::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Move);
    w.u32(seq);
    w.f32(x); w.f32(y); w.f32(z);
    w.f32(look_x); w.f32(look_y); w.f32(look_z);
    w.u8(scope);
    w.u8(flags);
    w.u32(epoch);
    w.u8(gen);
    w.f32(vx); w.f32(vy); w.f32(vz);
    w.u32(first_drive);
    w.u16((uint16_t)drive.size());   // ByteReader::count reads sixteen bits
    for (const DriveRecord &d : drive) {
        const DriveEvent &e = d.event;
        const bool built = e.put || e.built;
        w.u8((uint8_t)((e.put ? 1 : 0) | (e.keys.jump ? 2 : 0) | (built ? 4 : 0) |
                       (e.keys.boost ? 8 : 0) | (e.keys.drift ? 16 : 0)));
        w.u8(d.gen);
        if (e.put) {
            w.f32(e.pos.x); w.f32(e.pos.y); w.f32(e.pos.z);
            w.f32(e.vel.x); w.f32(e.vel.y); w.f32(e.vel.z);
            w.f32(e.yaw);
        } else {
            // Exact: the keys were put in 127ths before the chassis read them.
            w.u8((uint8_t)(int8_t)std::lround(e.keys.forward * 127.0f));
            w.u8((uint8_t)(int8_t)std::lround(e.keys.right * 127.0f));
        }
        if (built) w.u32(e.missing);
    }
    return w.data();
}

bool Move::decode(ByteReader &r, Move &out)
{
    out = Move{};

    if (r.u8() != (uint8_t)MsgType::Move) return false;

    out.seq    = r.u32();
    out.x      = r.f32();
    out.y      = r.f32();
    out.z      = r.f32();
    out.look_x = r.f32();
    out.look_y = r.f32();
    out.look_z = r.f32();
    out.scope  = r.u8();
    out.flags  = r.u8();
    out.epoch  = r.u32();
    out.gen    = r.u8();
    out.vx     = r.f32();
    out.vy     = r.f32();
    out.vz     = r.f32();
    out.first_drive = r.u32();

    // Every float in a record refused as the claim's own are, below; a key
    // outside -127..127 is not one the client's chassis could have read.
    const auto sane = [](float v) { return v == v && v <= 1e9f && v >= -1e9f; };
    size_t n = 0;
    if (!r.count(n, kMaxDriveRecords)) return false;
    out.drive.resize(n);
    for (DriveRecord &d : out.drive) {
        const uint8_t kind = r.u8();
        d.gen              = r.u8();
        if (kind & ~31u) return false;
        DriveEvent &e = d.event;
        e.put         = (kind & 1) != 0;
        e.keys.jump   = (kind & 2) != 0;
        e.built       = (kind & 4) != 0;
        e.keys.boost  = (kind & 8) != 0;
        e.keys.drift  = (kind & 16) != 0;   // 105
        if (e.put && !e.built) return false;   // a put always builds the blocks around it
        if (e.put) {
            e.pos.x = r.f32(); e.pos.y = r.f32(); e.pos.z = r.f32();
            e.vel.x = r.f32(); e.vel.y = r.f32(); e.vel.z = r.f32();
            e.yaw   = r.f32();
            for (const float v : {e.pos.x, e.pos.y, e.pos.z, e.vel.x, e.vel.y, e.vel.z, e.yaw})
                if (!sane(v)) return false;
        } else {
            const int8_t f = (int8_t)r.u8(), t = (int8_t)r.u8();
            if (f == -128 || t == -128) return false;
            e.keys.forward = (float)f / 127.0f;
            e.keys.right   = (float)t / 127.0f;
        }
        if (e.built) e.missing = r.u32();
    }
    if (!r.done()) return false;
    if (out.flags & ~kMoveFlying) return false;
    if (!sane(out.vx) || !sane(out.vy) || !sane(out.vz)) return false;

    // A scope this build does not know is not a claim it can decide what to do
    // with, and guessing either way is wrong: taken as a body it would be
    // checked, taken as a window it would move the interest centre.
    if (out.scope != (uint8_t)Scope::Enter && out.scope != (uint8_t)Scope::Observe) return false;

    // NaN and infinity are what a float field carries when somebody is having a
    // go at you, and they poison every comparison downstream: a chunk distance
    // of NaN sorts wherever the sort happens to put it and never leaves the
    // queue. Refused here so nothing further down has to think about it.
    const float coords[6] = {out.x, out.y, out.z, out.look_x, out.look_y, out.look_z};
    for (const float v : coords) {
        if (!(v == v)) return false;                       // NaN
        if (v > 1e9f || v < -1e9f) return false;           // infinity, and absurd
    }
    return true;
}

// --- Correct ----------------------------------------------------------------

std::vector<uint8_t> Correct::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Correct);
    w.u32(seq);
    w.u32(epoch);
    w.f32(x); w.f32(y); w.f32(z);
    w.u8(flags);
    w.f32(vx); w.f32(vy); w.f32(vz);
    w.f32(yaw);
    w.f32(nitro);
    w.str(reason);
    return w.data();
}

bool Correct::decode(ByteReader &r, Correct &out)
{
    out = Correct{};

    if (r.u8() != (uint8_t)MsgType::Correct) return false;

    out.seq   = r.u32();
    out.epoch = r.u32();
    out.x     = r.f32();
    out.y     = r.f32();
    out.z     = r.f32();
    out.flags = r.u8();
    out.vx    = r.f32();
    out.vy    = r.f32();
    out.vz    = r.f32();
    out.yaw   = r.f32();
    out.nitro = r.f32();
    if (!r.str(out.reason, 256)) return false;
    if (!r.done()) return false;
    if (out.flags & ~kCorrectDrive) return false;
    if (!(out.nitro >= -1.0f && out.nitro <= 1.0f)) return false;

    const float coords[7] = {out.x, out.y, out.z, out.vx, out.vy, out.vz, out.yaw};
    for (const float v : coords) {
        if (!(v == v)) return false;
        if (v > 1e9f || v < -1e9f) return false;
    }
    return true;
}

// --- Teleport ---------------------------------------------------------------

std::vector<uint8_t> Teleport::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Teleport);
    w.u32(seq);
    w.u32(epoch);
    w.f32(x); w.f32(y); w.f32(z);
    w.f32(look_x); w.f32(look_y); w.f32(look_z);
    w.f32(vx); w.f32(vy); w.f32(vz);
    w.f32(roll);
    w.str(reason);
    return w.data();
}

bool Teleport::decode(ByteReader &r, Teleport &out)
{
    out = Teleport{};

    if (r.u8() != (uint8_t)MsgType::Teleport) return false;

    out.seq    = r.u32();
    out.epoch  = r.u32();
    out.x      = r.f32();
    out.y      = r.f32();
    out.z      = r.f32();
    out.look_x = r.f32();
    out.look_y = r.f32();
    out.look_z = r.f32();
    out.vx     = r.f32();
    out.vy     = r.f32();
    out.vz     = r.f32();
    out.roll   = r.f32();
    if (!r.str(out.reason, 256)) return false;
    if (!r.done()) return false;

    // Same refusal as every other float on the wire: a NaN or a number no world
    // is that big reaches the physics and stays there.
    const float all[10] = {out.x, out.y, out.z, out.look_x, out.look_y, out.look_z,
                           out.vx, out.vy, out.vz, out.roll};
    for (const float v : all) {
        if (!(v == v)) return false;
        if (v > 1e9f || v < -1e9f) return false;
    }

    // A look that is not a direction is not a look. Checked here rather than
    // trusted, because it lands straight in the camera basis.
    const float len = std::sqrt(out.look_x * out.look_x + out.look_y * out.look_y +
                                out.look_z * out.look_z);
    if (len < 0.5f || len > 2.0f) return false;

    return true;
}

// One opening's description, checked before anything reads it.
//
// A NaN centre draws nothing and moves a player nowhere findable; an axis of 7
// indexes off the end of the three the enum has; a zero half-width is an opening
// with no area, which divides by zero when the image is fitted to it.
//
// One copy, because two messages carry one of these and two checks would be two
// things to forget to update.
bool portal_desc_sane(const PortalDesc &d)
{
    const float all[5] = {d.cx, d.cy, d.cz, d.half_w, d.half_h};
    for (const float v : all) {
        if (!(v == v)) return false;
        if (v > 1e9f || v < -1e9f) return false;
    }
    if (d.axis > 2) return false;
    if (d.normal_sign != 1 && d.normal_sign != -1) return false;
    if (d.rot > 3) return false;
    if (d.half_w <= 0.0f || d.half_h <= 0.0f) return false;
    if (d.half_w > 64.0f || d.half_h > 64.0f) return false;
    if (d.kind > 1) return false;
    return true;
}

// --- ChangeScope ------------------------------------------------------------

namespace {

void write_desc(ByteWriter &w, const PortalDesc &d)
{
    w.f32(d.cx); w.f32(d.cy); w.f32(d.cz);
    w.u8(d.axis);
    w.u8((uint8_t)d.normal_sign);
    w.u8(d.rot);
    w.f32(d.half_w); w.f32(d.half_h);
    w.u8(d.kind);
}

void read_desc(ByteReader &r, PortalDesc &d)
{
    d.cx          = r.f32();
    d.cy          = r.f32();
    d.cz          = r.f32();
    d.axis        = r.u8();
    d.normal_sign = (int8_t)r.u8();
    d.rot         = r.u8();
    d.half_w      = r.f32();
    d.half_h      = r.f32();
    d.kind        = r.u8();
}

} // namespace

std::vector<uint8_t> ChangeScope::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::ChangeScope);
    w.u8(scope);
    w.u32(portal_id);
    w.u8(has_ticket ? 1 : 0);
    if (has_ticket) {
        const auto blob = ticket.encode();
        w.bytes(blob.data(), blob.size());
    }
    write_desc(w, from);
    w.f32(x); w.f32(y); w.f32(z);
    w.f32(look_x); w.f32(look_y); w.f32(look_z);
    w.f32(vx); w.f32(vy); w.f32(vz);
    return w.data();
}

bool ChangeScope::decode(ByteReader &r, ChangeScope &out)
{
    out = ChangeScope{};

    if (r.u8() != (uint8_t)MsgType::ChangeScope) return false;

    out.scope     = r.u8();
    out.portal_id = r.u32();

    const uint8_t has_ticket = r.u8();
    if (has_ticket > 1) return false;
    out.has_ticket = has_ticket == 1;
    if (out.has_ticket && !PortalTicket::decode(r, out.ticket)) return false;

    read_desc(r, out.from);
    out.x      = r.f32();
    out.y      = r.f32();
    out.z      = r.f32();
    out.look_x = r.f32();
    out.look_y = r.f32();
    out.look_z = r.f32();
    out.vx     = r.f32();
    out.vy     = r.f32();
    out.vz     = r.f32();
    if (!r.done()) return false;

    if (out.scope > (uint8_t)Scope::Observe) return false;

    // A session stepping back to watching sends no pose, and checking one it
    // did not send would refuse every honest demotion.
    if (out.scope != (uint8_t)Scope::Enter) return true;

    if (out.portal_id >= (uint32_t)kMaxOpenings) return false;
    if (!portal_desc_sane(out.from)) return false;

    const float all[9] = {out.x, out.y, out.z, out.look_x, out.look_y, out.look_z,
                          out.vx, out.vy, out.vz};
    for (const float v : all) {
        if (!(v == v)) return false;
        if (v > 1e9f || v < -1e9f) return false;
    }

    // The look lands in the camera basis on the way back out. Same rule as a
    // Teleport's.
    const float len = std::sqrt(out.look_x * out.look_x + out.look_y * out.look_y +
                                out.look_z * out.look_z);
    if (len < 0.5f || len > 2.0f) return false;

    return true;
}

// --- PlacePortal ------------------------------------------------------------

std::vector<uint8_t> PlacePortal::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::PlacePortal);
    w.u8(which);
    w.i32(x); w.i32(y); w.i32(z);
    w.u8((uint8_t)nx); w.u8((uint8_t)ny); w.u8((uint8_t)nz);
    return w.data();
}

bool PlacePortal::decode(ByteReader &r, PlacePortal &out)
{
    out = PlacePortal{};

    if (r.u8() != (uint8_t)MsgType::PlacePortal) return false;

    out.which = r.u8();
    out.x     = r.i32();
    out.y     = r.i32();
    out.z     = r.i32();
    out.nx    = (int8_t)r.u8();
    out.ny    = (int8_t)r.u8();
    out.nz    = (int8_t)r.u8();
    if (!r.done()) return false;

    // There are two openings, and a third index is not a bigger world -- it is
    // a write past the end of a two-element array.
    if (out.which > 1) return false;

    // Six faces. Anything else is either a diagonal, which no cube has, or a
    // zero vector, which would put the opening inside the wall it was placed
    // on. Checked here so that everything downstream may assume a face.
    const int sum = std::abs((int)out.nx) + std::abs((int)out.ny) + std::abs((int)out.nz);
    if (sum != 1) return false;

    // A node index no world reaches. The server checks the node against its own
    // bounds anyway; this refuses the arithmetic before it is done, because
    // `x + nx` on INT32_MIN is undefined behaviour and the bounds check would
    // never run.
    const int32_t kFar = 1 << 24;
    if (out.x < -kFar || out.x > kFar) return false;
    if (out.y < -kFar || out.y > kFar) return false;
    if (out.z < -kFar || out.z > kFar) return false;

    return true;
}

// --- Portals ----------------------------------------------------------------

std::vector<uint8_t> Portals::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Portals);
    w.u32(version);
    w.u32(present);

    for (int i = 0; i < kMaxOpenings; ++i) {
        if (!has(i)) continue;
        const PortalDesc &d = opening[i];
        w.f32(d.cx); w.f32(d.cy); w.f32(d.cz);
        w.u8(d.axis);
        w.u8((uint8_t)d.normal_sign);
        w.u8(d.rot);
        w.f32(d.half_w); w.f32(d.half_h);
        w.u8(d.kind);
        w.u8(d.colour);
        w.u8((uint8_t)link[i]);
    }

    // Up to the last one with anything in it, as it is: a world with none
    // sends a zero, and one with a gap or half a destination is sent so and
    // refused by the decoder rather than quietly tidied here.
    int n = 0;
    for (int k = 0; k < kMaxMouths; ++k) {
        const PortalAway &a = away[k];
        if (!a.host.empty() || a.port != 0 || !a.room.empty() || a.has_key) n = k + 1;
    }
    w.u8((uint8_t)n);
    for (int k = 0; k < n; ++k) {
        const PortalAway &a = away[k];
        w.str(a.host);
        w.u16(a.port);
        w.u16(a.portal);
        w.u8(a.has_key ? 1 : 0);
        if (a.has_key) w.bytes(a.key, kSignPublicBytes);
        w.str(a.room);
    }
    return w.data();
}

bool Portals::decode(ByteReader &r, Portals &out)
{
    out = Portals{};

    if (r.u8() != (uint8_t)MsgType::Portals) return false;

    out.version = r.u32();
    out.present = r.u32();

    for (int i = 0; i < kMaxOpenings; ++i) {
        if (!out.has(i)) continue;
        PortalDesc &d = out.opening[i];
        d.cx          = r.f32();
        d.cy          = r.f32();
        d.cz          = r.f32();
        d.axis        = r.u8();
        d.normal_sign = (int8_t)r.u8();
        d.rot         = r.u8();
        d.half_w      = r.f32();
        d.half_h      = r.f32();
        d.kind        = r.u8();
        d.colour      = r.u8();
        out.link[i]   = (int8_t)r.u8();

        // An index this table does not have is a write past its end in every
        // array a client keeps per opening. -1 is "leads nowhere".
        if (out.link[i] < -1 || out.link[i] >= kMaxOpenings) return false;
    }

    const int n = r.u8();
    if (n > kMaxMouths) return false;
    for (int k = 0; k < n; ++k) {
        PortalAway &a = out.away[k];
        // A host name long enough to be a payload is not a host name. 255 is
        // what DNS allows and more than anything real uses.
        if (!r.str(a.host, 255)) return false;
        a.port    = r.u16();
        a.portal  = r.u16();
        a.has_key = r.u8() != 0;
        if (a.has_key && !r.bytes(a.key, kSignPublicBytes)) return false;
        if (!r.str(a.room, kMaxRoomName)) return false;

        // A key for a link that goes nowhere is a field describing something
        // that does not exist. Refused rather than ignored: the two travel
        // together and a message where only one is set was built by something
        // that does not agree with this file about what they mean.
        if (a.has_key && !a.leads_away()) return false;

        // Half a destination is not one. A host with no port is somewhere
        // nobody can connect to, and a port with no host is a number.
        if (a.host.empty() != (a.port == 0)) return false;

        // And an opening this protocol cannot have: any of a world's sixteen
        // (since 71: a mouth may come out of an opening a mod opened, the
        // Inferno track's way back), and past them something newer than this
        // parser, which is a thing to refuse rather than to index with.
        if (a.portal >= kMaxOpenings) return false;

        // Every one sent leads away: a gap would be a second mouth beside a
        // local first opening, half a pair and half a mouth, which no client
        // can draw -- the first opening's crossing would need a second end
        // here that leads somewhere else.
        if (!a.leads_away()) return false;
    }
    if (!r.done()) return false;

    // Every field lands in the renderer's basis and in the body's transform.
    for (int i = 0; i < kMaxOpenings; ++i) {
        if (!out.has(i)) continue;
        if (!portal_desc_sane(out.opening[i])) return false;
    }

    return true;
}

// --- tickets ----------------------------------------------------------------

namespace {

// Written and read in one place, so the two can never drift: a ticket that
// encodes one way and verifies over another is a signature that checks nothing.
void write_ticket_fields(ByteWriter &w, const PortalTicket &t)
{
    w.bytes(t.issuer, kSignPublicBytes);
    w.bytes(t.subject, kSignPublicBytes);
    w.bytes(t.audience, kSignPublicBytes);
    w.u32(t.portal_id);
    w.u8(t.scopes);
    w.u64(t.issued);
    w.u64(t.expires);
}

} // namespace

std::vector<uint8_t> ticket_challenge(const PortalTicket &t)
{
    ByteWriter w;
    // The label, so a ticket cannot be read as any other thing this protocol
    // signs -- and so that nothing this protocol signs later can be read as a
    // ticket. Not NUL terminated: the fields after it are fixed width, so
    // there is nothing for a boundary to be ambiguous about.
    const char *label = "cnp1 portal ticket";
    for (const char *c = label; *c; ++c) w.u8((uint8_t)*c);
    write_ticket_fields(w, t);
    return w.data();
}

bool ticket_signed_by_issuer(const PortalTicket &t)
{
    const std::vector<uint8_t> msg = ticket_challenge(t);
    return verify_message(t.issuer, msg.data(), msg.size(), t.signature);
}

std::vector<uint8_t> PortalTicket::encode() const
{
    ByteWriter w;
    write_ticket_fields(w, *this);
    w.bytes(signature, kSignatureBytes);
    return w.data();
}

bool PortalTicket::decode(ByteReader &r, PortalTicket &out)
{
    out = PortalTicket{};

    if (!r.bytes(out.issuer, kSignPublicBytes)) return false;
    if (!r.bytes(out.subject, kSignPublicBytes)) return false;
    if (!r.bytes(out.audience, kSignPublicBytes)) return false;
    out.portal_id = r.u32();
    out.scopes    = r.u8();
    out.issued    = r.u64();
    out.expires   = r.u64();
    if (!r.bytes(out.signature, kSignatureBytes)) return false;

    // A ticket for nothing, and a ticket for a scope this build has no name
    // for. Both are refused here rather than carried to a check that would have
    // to guess what they meant.
    if (out.scopes == 0 || (out.scopes & ~kTicketScopes) != 0) return false;

    // An opening this protocol does not have. Same bound as everywhere else.
    if (out.portal_id >= (uint32_t)kMaxOpenings) return false;

    // A window that is not one. Backwards or empty is not a short ticket, it is
    // a ticket that was built wrong or bent on the way here.
    if (out.expires <= out.issued) return false;

    return true;
}

std::vector<uint8_t> TicketRequest::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::TicketRequest);
    w.u8(scopes);
    w.u8(opening);
    return w.data();
}

bool TicketRequest::decode(ByteReader &r, TicketRequest &out)
{
    out = TicketRequest{};

    if (r.u8() != (uint8_t)MsgType::TicketRequest) return false;
    out.scopes  = r.u8();
    out.opening = r.u8();
    if (!r.done()) return false;

    if (out.scopes == 0 || (out.scopes & ~kTicketScopes) != 0) return false;
    if (out.opening >= kMaxMouths) return false;
    return true;
}

std::vector<uint8_t> Ticket::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Ticket);
    w.u8(granted ? 1 : 0);
    if (granted) {
        const auto blob = ticket.encode();
        w.bytes(blob.data(), blob.size());
    } else {
        w.u8((uint8_t)reason);
        w.str(detail);
    }
    return w.data();
}

bool Ticket::decode(ByteReader &r, Ticket &out)
{
    out = Ticket{};

    if (r.u8() != (uint8_t)MsgType::Ticket) return false;

    const uint8_t granted = r.u8();
    if (granted > 1) return false;
    out.granted = granted == 1;

    if (out.granted) {
        if (!PortalTicket::decode(r, out.ticket)) return false;
    } else {
        const uint8_t reason = r.u8();
        out.reason = reason <= (uint8_t)RefuseReason::Last ? (RefuseReason)reason
                                                           : RefuseReason::None;
        if (!r.str(out.detail, 256)) return false;
    }

    if (!r.done()) return false;
    return true;
}

// --- Heartbeat --------------------------------------------------------------

std::vector<uint8_t> Heartbeat::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Heartbeat);
    w.u32(seq);
    w.u16(players);
    w.u16(max_players);
    w.u16(observers);
    w.u16(max_observers);
    w.u8(flags);
    return w.data();
}

bool Heartbeat::decode(ByteReader &r, Heartbeat &out)
{
    out = Heartbeat{};

    if (r.u8() != (uint8_t)MsgType::Heartbeat) return false;

    out.seq           = r.u32();
    out.players       = r.u16();
    out.max_players   = r.u16();
    out.observers     = r.u16();
    out.max_observers = r.u16();
    out.flags         = r.u8();
    if (!r.done()) return false;

    // Counts that cannot both be true. Not a security matter -- these come from
    // a server the client has already decided to talk to -- but a full bar
    // drawn from 9 out of 8 is a rendering bug traced back to here, and the
    // check is two comparisons.
    if (out.players > out.max_players) return false;
    if (out.observers > out.max_observers) return false;

    // Flags this build has no name for. Refused rather than masked off: a
    // heartbeat is small and frequent, and quietly ignoring half of one is how
    // a client ends up acting on a world state that was never sent.
    if ((out.flags & ~(uint8_t)(kWorldWatchable | kWorldEnterable)) != 0) return false;

    return true;
}

// --- Redirect ---------------------------------------------------------------

std::vector<uint8_t> Redirect::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Redirect);
    w.str(host);
    w.u16(port);
    w.u32(portal_id);
    w.str(room);
    w.u8(has_ticket ? 1 : 0);
    if (has_ticket) {
        const auto blob = ticket.encode();
        w.bytes(blob.data(), blob.size());
    }
    w.str(reason);
    return w.data();
}

bool Redirect::decode(ByteReader &r, Redirect &out)
{
    out = Redirect{};

    if (r.u8() != (uint8_t)MsgType::Redirect) return false;

    // Same bound as a Portals destination: a host name long enough to be a
    // payload is not a host name.
    if (!r.str(out.host, 255)) return false;
    out.port      = r.u16();
    out.portal_id = r.u32();
    if (!r.str(out.room, kMaxRoomName)) return false;

    const uint8_t has_ticket = r.u8();
    if (has_ticket > 1) return false;
    out.has_ticket = has_ticket == 1;
    if (out.has_ticket && !PortalTicket::decode(r, out.ticket)) return false;

    if (!r.str(out.reason, 256)) return false;
    if (!r.done()) return false;

    // Somewhere nobody can connect to. A redirect this client cannot act on is
    // worse than none: it would close a working session to go nowhere.
    // No host is this same server, which only a room can be.
    if (out.host.empty() != (out.port == 0)) return false;
    if (out.host.empty() && out.room.empty()) return false;
    if (out.portal_id >= (uint32_t)kMaxOpenings) return false;

    return true;
}

// ------------------------------------------------------------------ media

std::vector<uint8_t> MediaOffer::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::MediaOffer);
    w.u16((uint16_t)items.size());
    for (const auto &e : items) {
        w.str(e.name);
        w.bytes(e.hash.bytes, kContentHashBytes);
        w.u32(e.size);
    }
    return w.data();
}

bool MediaOffer::decode(ByteReader &r, MediaOffer &out)
{
    out = MediaOffer{};

    if (r.u8() != (uint8_t)MsgType::MediaOffer) return false;

    const size_t n = r.u16();
    if (n > kMaxMediaItems) return false;
    out.items.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        MediaEntry e;
        // A file name, not a path. 255 is generous for one; anything with a
        // separator in it is refused below.
        if (!r.str(e.name, 255)) return false;
        if (!r.bytes(e.hash.bytes, kContentHashBytes)) return false;
        e.size = r.u32();

        if (e.name.empty() || e.size == 0 || e.size > kMaxMediaBytes) return false;

        // The name reaches a filesystem on the client only through the hash --
        // the cache is keyed by content and never by this. It is still refused
        // here, because a name is also what a log line prints and what a block
        // looks itself up by, and a server that sends "../../.ssh/id_ed25519"
        // should be told no at the parser rather than trusted to be harmless
        // three layers up.
        if (e.name.find('/') != std::string::npos || e.name.find('\\') != std::string::npos ||
            e.name.find("..") != std::string::npos)
            return false;

        if (e.hash.empty()) return false;
        out.items.push_back(std::move(e));
    }

    if (!r.done()) return false;
    return true;
}

std::vector<uint8_t> MediaMissing::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::MediaMissing);
    w.u16((uint16_t)hashes.size());
    for (const auto &h : hashes) w.bytes(h.bytes, kContentHashBytes);
    return w.data();
}

bool MediaMissing::decode(ByteReader &r, MediaMissing &out)
{
    out = MediaMissing{};

    if (r.u8() != (uint8_t)MsgType::MediaMissing) return false;

    const size_t n = r.u16();
    if (n > kMediaPerRequest) return false;   // never more than one request's worth
    out.hashes.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        ContentHash h;
        if (!r.bytes(h.bytes, kContentHashBytes)) return false;
        if (h.empty()) return false;
        out.hashes.push_back(h);
    }
    return r.done();
}

std::vector<uint8_t> MediaNeed::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::MediaNeed);
    w.u16((uint16_t)hashes.size());
    for (const auto &h : hashes) w.bytes(h.bytes, kContentHashBytes);
    return w.data();
}

bool MediaNeed::decode(ByteReader &r, MediaNeed &out)
{
    out = MediaNeed{};

    if (r.u8() != (uint8_t)MsgType::MediaNeed) return false;

    const size_t n = r.u16();
    if (n > kMediaPerRequest) return false;
    out.hashes.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        ContentHash h;
        if (!r.bytes(h.bytes, kContentHashBytes)) return false;
        if (h.empty()) return false;
        out.hashes.push_back(h);
    }

    if (!r.done()) return false;
    return true;
}

std::vector<uint8_t> MediaData::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::MediaData);
    w.bytes(hash.bytes, kContentHashBytes);
    w.u32((uint32_t)bytes.size());
    w.bytes(bytes.data(), bytes.size());
    return w.data();
}

bool MediaData::decode(ByteReader &r, MediaData &out)
{
    out = MediaData{};

    if (r.u8() != (uint8_t)MsgType::MediaData) return false;
    if (!r.bytes(out.hash.bytes, kContentHashBytes)) return false;

    const uint32_t n = r.u32();
    if (n == 0 || n > kMaxMediaBytes) return false;
    if (r.remaining() < n) return false;   // before the allocation, not after

    out.bytes.resize(n);
    if (!r.bytes(out.bytes.data(), n)) return false;
    if (!r.done()) return false;

    if (out.hash.empty()) return false;

    // The message says what it carries. Checked here, once, so that nothing
    // above this line has to remember to: a blob whose bytes do not hash to
    // the name it arrived under is not a file that got corrupted, it is a
    // server answering a request for one thing with another.
    if (hash_content(out.bytes) != out.hash) return false;

    return true;
}

// --- ScopeDenied ------------------------------------------------------------

std::vector<uint8_t> ScopeDenied::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::ScopeDenied);
    w.u8(scope);
    w.u8((uint8_t)reason);
    w.str(detail);
    return w.data();
}

bool ScopeDenied::decode(ByteReader &r, ScopeDenied &out)
{
    out = ScopeDenied{};

    if (r.u8() != (uint8_t)MsgType::ScopeDenied) return false;

    out.scope = r.u8();
    if (out.scope > (uint8_t)Scope::Observe) return false;

    const uint8_t reason = r.u8();
    // Same rule as a Refuse: a reason from something newer arrives as no reason
    // rather than as a value nothing has a name for. RefuseReason::Last is what
    // makes that survive a reason being added.
    out.reason = reason <= (uint8_t)RefuseReason::Last ? (RefuseReason)reason : RefuseReason::None;

    if (!r.str(out.detail, 256)) return false;
    if (!r.done()) return false;

    return true;
}

// --- Entities ---------------------------------------------------------------

std::vector<uint8_t> Entities::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Entities);
    w.u16(frame);
    w.u8(part);
    w.u8(parts);

    const size_t n = players.size() > kEntitiesPerPart ? kEntitiesPerPart : players.size();
    w.u16((uint16_t)n);
    for (size_t i = 0; i < n; ++i) {
        const EntityState &e = players[i];
        w.u64(e.id);
        w.f32(e.x); w.f32(e.y); w.f32(e.z);
        w.f32(e.yaw); w.f32(e.pitch);
        w.u8((uint8_t)e.kind);
        w.u16(e.item);
        w.u16(e.model);
        const bool dressed = e.kind == EntityKind::Player &&
                             (e.worn[0] | e.worn[1] | e.worn[2] | e.worn[3] | e.worn[4]) != 0;
        const bool chassis = (e.kind == EntityKind::Player || e.kind == EntityKind::Thing) && e.chassis;
        const bool tinted  = e.kind == EntityKind::Player && e.item_tint != 0;
        const bool clip    = (e.kind == EntityKind::Mob || e.kind == EntityKind::Thing) && !e.clip.empty();
        const bool posed   = e.kind == EntityKind::Player && !e.clip.empty();   // 92
        const bool livery  = e.kind == EntityKind::Player && e.livery != 0;      // 93
        w.u8((e.working ? 1 : 0) | (e.crouching || clip ? 2 : 0) | (e.burning || posed ? 4 : 0) |
             (dressed ? 8 : 0) | (chassis ? 16 : 0) |
             ((e.kind == EntityKind::Drop && e.falling) || livery ? 32 : 0) |
             ((e.kind == EntityKind::Player && e.lying) || (e.kind == EntityKind::Mob && e.held) ? 64 : 0) |
             (tinted ? 128 : 0));
        if (e.kind == EntityKind::Player) {
            int pieces = 0;
            for (const uint16_t m : e.dress) pieces += m != 0;
            w.u8((e.flying ? 1 : 0) | (pieces > 0 ? 2 : 0) |   // 97, 102
                 (e.chassis && e.boosting ? 4 : 0) | (e.chassis && e.sliding ? 8 : 0));   // 103
            if (pieces > 0) {
                w.u8((uint8_t)pieces);
                for (int k = 0; k < 10; ++k)
                    if (e.dress[k] != 0) {
                        w.u8((uint8_t)k);
                        w.u16(e.dress[k]);
                        w.u16(e.dress_tint[k]);
                    }
            }
        }
        if (dressed)
            for (const uint16_t id : e.worn) w.u16(id);
        if (dressed)
            for (const uint16_t t : e.worn_tint) w.u16(t);
        if (dressed)
            for (const uint16_t id : e.arm) w.u16(id);   // 96
        if (chassis && e.kind == EntityKind::Thing) {
            w.f32(e.body_roll);   // a thing's roll (98); its pitch is the look's
            for (const float f : {e.hull_w, e.hull_h, e.hull_l, e.hull_ride})   // its hull (100)
                w.u8((uint8_t)std::clamp(std::lround(f * 16.0f), 0L, 255L));
        } else if (chassis) {
            w.f32(e.body_pitch); w.f32(e.body_roll);
            w.f32(e.base_dx); w.f32(e.base_dy); w.f32(e.base_dz);
        }
        if (tinted) w.u16(e.item_tint);
        if (clip || posed) {
            const size_t len = std::min(e.clip.size(), kMaxClipName);
            w.u8((uint8_t)len);
            for (size_t k = 0; k < len; ++k) w.u8((uint8_t)e.clip[k]);
        }
        if (livery) w.u32(e.livery);
    }
    return w.data();
}

bool Entities::decode(ByteReader &r, Entities &out)
{
    out = Entities{};

    if (r.u8() != (uint8_t)MsgType::Entities) return false;
    out.frame = r.u16();
    out.part  = r.u8();
    out.parts = r.u8();
    if (out.parts == 0 || out.parts > kMaxEntityParts || out.part >= out.parts) return false;

    size_t n = 0;
    if (!r.count(n, kEntitiesPerPart)) return false;

    out.players.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        EntityState e;
        e.id    = r.u64();
        e.x     = r.f32();
        e.y     = r.f32();
        e.z     = r.f32();
        e.yaw   = r.f32();
        e.pitch = r.f32();

        const uint8_t kind = r.u8();
        if (kind > (uint8_t)EntityKind::Thing) return false;   // a kind nobody here draws
        e.kind = (EntityKind)kind;

        e.item = r.u16();
        if (!r.ok()) return false;

        // The two fields have to agree. A drop with no stack is a thing the
        // client would draw as an empty box, and a player carrying a stack id
        // is a body somebody meant to be a drop -- either way one of the two
        // fields is wrong, and a decoder that picks the one it prefers makes
        // that choice on every client differently.
        // Since 48 a Player's is what they hold, and since 81 a creature's,
        // so only a thing must have none.
        if (e.kind == EntityKind::Drop && e.item == 0) return false;
        if (e.kind == EntityKind::Thing && e.item != 0) return false;

        // And what it looks like, by the same rule. A stack on the ground is
        // drawn as the thing it is; one wearing a model would be two answers to
        // one question, and each client would pick a different one.
        e.model = r.u16();
        if (!r.ok()) return false;
        if (e.kind == EntityKind::Drop && e.model != 0) return false;

        uint8_t flags = r.u8();
        if (!r.ok()) return false;
        if (e.kind == EntityKind::Player) {   // 97
            const uint8_t more = r.u8();
            if (!r.ok() || (more & ~15u)) return false;
            e.flying   = (more & 1) != 0;
            e.boosting = (more & 4) != 0;
            e.sliding  = (more & 8) != 0;
            if (more & 2) {   // dressed by a mod (102): never an empty list
                const uint8_t pieces = r.u8();
                if (!r.ok() || pieces == 0 || pieces > 10) return false;
                for (uint8_t i = 0; i < pieces; ++i) {
                    const uint8_t k = r.u8();
                    if (!r.ok() || k >= 10 || e.dress[k] != 0) return false;
                    e.dress[k]      = r.u16();
                    e.dress_tint[k] = r.u16();
                    if (!r.ok() || e.dress[k] == 0) return false;
                }
            }
        }
        if ((flags & 128) && e.kind != EntityKind::Player) return false;  // only a person holds (76)
        // Only a person lies (75), and only a creature is held (91).
        if ((flags & 64) && e.kind != EntityKind::Player && e.kind != EntityKind::Mob) return false;
        e.lying = (flags & 64) != 0 && e.kind == EntityKind::Player;
        e.held  = (flags & 64) != 0 && e.kind == EntityKind::Mob;
        // Only a drop falls (73), and on a person flag 32 is their livery (93).
        if ((flags & 32) && e.kind != EntityKind::Drop && e.kind != EntityKind::Player) return false;
        const bool livery = (flags & 32) != 0 && e.kind == EntityKind::Player;
        e.falling = (flags & 32) != 0 && e.kind == EntityKind::Drop;
        if ((flags & 8) && e.kind != EntityKind::Player) return false;   // only a person wears (58)
        if (flags & 8)
            for (uint16_t &id : e.worn) id = r.u16();
        if (flags & 8)
            for (uint16_t &t : e.worn_tint) t = r.u16();
        if (flags & 8)
            for (uint16_t &id : e.arm) id = r.u16();
        // Only a person drives (66), and a mod's thing is rolled (98).
        if ((flags & 16) && e.kind != EntityKind::Player && e.kind != EntityKind::Thing) return false;
        if ((flags & 16) && e.kind == EntityKind::Thing) {
            e.chassis   = true;
            e.body_roll = r.f32();
            if (!r.ok() || !(e.body_roll >= -3.2f && e.body_roll <= 3.2f)) return false;
            for (float *f : {&e.hull_w, &e.hull_h, &e.hull_l, &e.hull_ride}) *f = r.u8() / 16.0f;
            if (!r.ok()) return false;
        } else if (flags & 16) {
            e.chassis    = true;
            e.body_pitch = r.f32();
            e.body_roll  = r.f32();
            e.base_dx    = r.f32();
            e.base_dy    = r.f32();
            e.base_dz    = r.f32();
            // A chassis tips within a turn and its model's bottom is under it
            // by less than its height: anything else is not a kart.
            for (const float a : {e.body_pitch, e.body_roll})
                if (!(a >= -3.2f && a <= 3.2f)) return false;
            for (const float d : {e.base_dx, e.base_dy, e.base_dz})
                if (!(d >= -4.0f && d <= 4.0f)) return false;
        }
        if ((e.boosting || e.sliding) && !e.chassis) return false;   // only a kart (103)
        if (flags & 128) e.item_tint = r.u16();
        if (!r.ok()) return false;
        // On a creature flag 2 is a clip its mod named (81), and on a mod's
        // thing (88): a length and the name.
        if ((flags & 2) && (e.kind == EntityKind::Mob || e.kind == EntityKind::Thing)) {
            const uint8_t len = r.u8();
            if (!r.ok() || len == 0 || len > kMaxClipName) return false;
            for (uint8_t k = 0; k < len; ++k) e.clip.push_back((char)r.u8());
            if (!r.ok()) return false;
            flags &= (uint8_t)~2;
        }
        // On a person flag 4 is the same (92, core.player_clip).
        if ((flags & 4) && e.kind == EntityKind::Player) {
            const uint8_t len = r.u8();
            if (!r.ok() || len == 0 || len > kMaxClipName) return false;
            for (uint8_t k = 0; k < len; ++k) e.clip.push_back((char)r.u8());
            if (!r.ok()) return false;
            flags &= (uint8_t)~4;
        }
        if (livery) {
            e.livery = r.u32();
            if (!r.ok() || e.livery == 0 || e.livery >= kLiveryMax) return false;
        }
        // Since 52 a creature is Working while it swings or draws; only a
        // person crouches, and a stack or a thing does neither.
        if ((flags & 2) && e.kind != EntityKind::Player) return false;
        if ((flags & 1) && e.kind != EntityKind::Player && e.kind != EntityKind::Mob) return false;
        if ((flags & 4) && e.kind != EntityKind::Mob) return false;   // only a creature burns (55)
        e.working   = (flags & 1) != 0;
        e.crouching = (flags & 2) != 0;
        e.burning   = (flags & 4) != 0;

        // Same reason as a position claim: a NaN here ends up in a distance,
        // a sort, or an interpolation, and poisons all three.
        const float coords[5] = {e.x, e.y, e.z, e.yaw, e.pitch};
        for (const float v : coords) {
            if (!(v == v)) return false;
            if (v > 1e9f || v < -1e9f) return false;
        }
        out.players.push_back(e);
    }

    return r.done();
}

// --- Refuse -----------------------------------------------------------------

std::vector<uint8_t> Refuse::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Refuse);
    w.u32(magic);
    w.u8((uint8_t)reason);
    w.str(detail);
    return w.data();
}

bool Refuse::decode(ByteReader &r, Refuse &out)
{
    out = Refuse{};

    if (r.u8() != (uint8_t)MsgType::Refuse) return false;

    out.magic          = r.u32();
    const uint8_t code = r.u8();
    if (!r.str(out.detail, 256)) return false;
    if (!r.done()) return false;

    // An unknown code from a newer server is not a parse failure: the detail
    // string still tells the player something, and refusing to read the
    // refusal is a worse failure mode than an unnamed reason.
    // The ceiling is the last value of the enum, taken from the enum. It used
    // to be a hand-written name, so the first reason added after it reached the
    // client as "no reason": the server refused correctly and the client could
    // not say why.
    out.reason = (code <= (uint8_t)RefuseReason::Last) ? (RefuseReason)code : RefuseReason::None;
    return true;
}

// --- MoveItem ---------------------------------------------------------------

std::vector<uint8_t> MoveItem::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::MoveItem);
    w.u8(from_side);
    w.u8(to_side);
    w.u16(from);
    w.u16(to);
    w.u16(count);
    return w.data();
}

std::vector<uint8_t> Vitals::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Vitals);
    w.u16(health);
    w.u16(max);
    return w.data();
}

bool Vitals::decode(ByteReader &r, Vitals &out)
{
    out = Vitals{};

    if (r.u8() != (uint8_t)MsgType::Vitals) return false;

    out.health = r.u16();
    out.max    = r.u16();

    if (!r.done()) return false;

    // A body with a maximum of nothing cannot be drawn and cannot be hurt, and
    // one holding more than it can hold is a bar past the end of itself.
    if (out.max == 0) return false;
    if (out.health > out.max) return false;

    return true;
}

std::vector<uint8_t> Push::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Push);
    w.f32(x);
    w.f32(y);
    w.f32(z);
    w.u8(set ? 1 : 0);
    return w.data();
}

bool Push::decode(ByteReader &r, Push &out)
{
    out = Push{};
    if (r.u8() != (uint8_t)MsgType::Push) return false;
    out.x = r.f32();
    out.y = r.f32();
    out.z = r.f32();
    const uint8_t set = r.u8();
    if (!r.done() || set > 1) return false;
    out.set = set == 1;
    for (const float v : {out.x, out.y, out.z})
        if (!std::isfinite(v) || std::fabs(v) > kMax) return false;
    return true;
}

std::vector<uint8_t> Seal::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Seal);
    w.u32(mask);
    return w.data();
}

bool Seal::decode(ByteReader &r, Seal &out)
{
    out = Seal{};
    if (r.u8() != (uint8_t)MsgType::Seal) return false;
    out.mask = r.u32();
    return r.done() && (out.mask >> kMaxMouths) == 0;
}

bool sound_name_ok(const std::string &name)
{
    if (name.empty() || name.size() > 64) return false;
    size_t colons = 0;
    for (const char c : name) {
        if (c == ':') ++colons;
        else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    const size_t at = name.find(':');
    return colons == 1 && at > 0 && at + 1 < name.size();
}

std::vector<uint8_t> Sound::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Sound);
    w.str(name);
    w.f32(x);
    w.f32(y);
    w.f32(z);
    return w.data();
}

bool Sound::decode(ByteReader &r, Sound &out)
{
    out = Sound{};
    if (r.u8() != (uint8_t)MsgType::Sound) return false;
    if (!r.str(out.name, 64)) return false;
    out.x    = r.f32();
    out.y    = r.f32();
    out.z    = r.f32();
    if (!r.done() || !sound_name_ok(out.name)) return false;
    for (const float v : {out.x, out.y, out.z})
        if (!std::isfinite(v) || std::fabs(v) > 1e7f) return false;
    return true;
}

std::vector<uint8_t> Lie::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Lie);
    w.u8(on ? 1 : 0);
    w.f32(x);
    w.f32(y);
    w.f32(z);
    w.f32(head);
    return w.data();
}

bool Lie::decode(ByteReader &r, Lie &out)
{
    out = Lie{};
    if (r.u8() != (uint8_t)MsgType::Lie) return false;
    const uint8_t on = r.u8();
    out.x    = r.f32();
    out.y    = r.f32();
    out.z    = r.f32();
    out.head = r.f32();
    if (!r.done() || on > 1) return false;
    out.on = on == 1;
    for (const float v : {out.x, out.y, out.z})
        if (!std::isfinite(v) || std::fabs(v) > 1e7f) return false;
    return std::isfinite(out.head) && std::fabs(out.head) <= 7.0f;
}

std::vector<uint8_t> Ride::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Ride);
    w.u64(host);
    w.f32(x);
    w.f32(y);
    w.f32(z);
    w.f32(seat_x);
    w.f32(seat_y);
    w.f32(seat_z);
    return w.data();
}

bool Ride::decode(ByteReader &r, Ride &out)
{
    out = Ride{};
    if (r.u8() != (uint8_t)MsgType::Ride) return false;
    out.host = r.u64();
    out.x    = r.f32();
    out.y    = r.f32();
    out.z    = r.f32();
    out.seat_x = r.f32();
    out.seat_y = r.f32();
    out.seat_z = r.f32();
    if (!r.done()) return false;
    for (const float v : {out.x, out.y, out.z})
        if (!std::isfinite(v) || std::fabs(v) > 1e7f) return false;
    for (const float v : {out.seat_x, out.seat_y, out.seat_z})
        if (!std::isfinite(v) || std::fabs(v) > Ride::kMax) return false;
    return true;
}

std::vector<uint8_t> BodyMovement::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::BodyMovement);
    w.f32(rules.walk);
    w.f32(rules.sprint);
    w.f32(rules.fly);
    w.f32(rules.jump);
    w.f32(rules.gravity);
    w.f32(rules.accel);
    w.f32(rules.brake);
    w.f32(rules.turn);
    w.f32(rules.drag);
    w.f32(rules.width);
    w.f32(rules.length);
    w.f32(rules.rise);
    w.f32(rules.eye);
    // Bit 2 a boat, which a 109 client ignores: it swims there instead.
    // Bit 3 a cart (110).
    w.u8((rules.parkour ? 1 : 0) | (rules.ledges ? 0 : 2) | (rules.boat ? 4 : 0) | (rules.cart ? 8 : 0));
    w.u8(gen);
    w.f32(dig);
    return w.data();
}

bool BodyMovement::decode(ByteReader &r, BodyMovement &out)
{
    out = BodyMovement{};

    if (r.u8() != (uint8_t)MsgType::BodyMovement) return false;

    out.rules.walk    = r.f32();
    out.rules.sprint  = r.f32();
    out.rules.fly     = r.f32();
    out.rules.jump    = r.f32();
    out.rules.gravity = r.f32();
    out.rules.accel   = r.f32();
    out.rules.brake   = r.f32();
    out.rules.turn    = r.f32();
    out.rules.drag    = r.f32();
    out.rules.width   = r.f32();
    out.rules.length  = r.f32();
    out.rules.rise    = r.f32();
    out.rules.eye     = r.f32();
    const uint8_t moves = r.u8();
    out.rules.parkour = (moves & 1) != 0;
    out.rules.ledges  = (moves & 2) == 0;
    out.rules.boat    = (moves & 4) != 0;
    out.rules.cart    = (moves & 8) != 0;
    out.gen           = r.u8();
    out.dig           = r.f32();

    if (!r.done()) return false;
    if (!(out.dig >= 0.1f && out.dig <= 10.0f)) return false;
    return movement_rules_problem(out.rules).empty();
}

std::vector<uint8_t> Hit::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Hit);
    w.u64(target);
    w.u8(slot);
    w.u8(use ? 1 : 0);
    return w.data();
}

bool Hit::decode(ByteReader &r, Hit &out)
{
    out = Hit{};

    if (r.u8() != (uint8_t)MsgType::Hit) return false;

    out.target = r.u64();
    out.slot   = r.u8();
    out.use    = r.u8() != 0;

    if (!r.done()) return false;

    // Zero is not an entity. Everything else is a number the server looks up
    // and refuses if it names nothing -- there is no id range this side could
    // check against, and a check that has to guess is one that will be wrong.
    return out.target != 0;
}

std::vector<uint8_t> Time::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Time);
    w.u16(tod);
    w.u32(wind);
    return w.data();
}

bool Time::decode(ByteReader &r, Time &out)
{
    out = Time{};

    if (r.u8() != (uint8_t)MsgType::Time) return false;

    out.tod  = r.u16();
    out.wind = r.u32();

    if (!r.done()) return false;

    // Every one of the sixty-five thousand codes is a real time, so there is
    // nothing here to refuse. Said out loud because a decode with no check in
    // it usually means somebody forgot one.
    return true;
}

std::vector<uint8_t> QuickMove::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::QuickMove);
    w.u8(side);
    w.u16(slot);
    return w.data();
}

bool QuickMove::decode(ByteReader &r, QuickMove &out)
{
    out = QuickMove{};

    if (r.u8() != (uint8_t)MsgType::QuickMove) return false;

    out.side = r.u8();
    out.slot = r.u16();

    if (!r.done()) return false;

    // A side this build has no inventory for, refused rather than read as Bag
    // -- the same rule MoveItem states, for the same reason.
    if (out.side > (uint8_t)Side::Craft) return false;

    // And a slot no bag reaches, with the one exception that is not a slot at
    // all: the answer square. kCraftResultSlot IS kMaxInventorySlots, so this
    // is one comparison and not two rules that could disagree.
    const bool result = out.side == (uint8_t)Side::Craft && out.slot == kCraftResultSlot;
    if (!result && out.slot >= kMaxInventorySlots) return false;

    return true;
}

bool MoveItem::decode(ByteReader &r, MoveItem &out)
{
    out = MoveItem{};

    if (r.u8() != (uint8_t)MsgType::MoveItem) return false;

    out.from_side = r.u8();
    out.to_side   = r.u8();
    out.from  = r.u16();
    out.to    = r.u16();
    out.count = r.u16();
    if (!r.done()) return false;

    // A side this build has no inventory for. Refused rather than read as Bag:
    // both ends are the same protocol version, so a byte outside the enum did
    // not come from something newer.
    if (out.from_side > (uint8_t)Side::Craft) return false;
    if (out.to_side > (uint8_t)Side::Craft) return false;

    // A slot number no bag reaches. The server checks against the bag it
    // actually has; this refuses the obviously impossible before it gets there,
    // the same way a face is checked to be one of six.
    //
    // And it is the line that refuses a move INTO the result square, without a
    // second one saying so: kCraftResultSlot IS kMaxInventorySlots, so "the
    // result is not a place anything goes" and "that is not a slot any bag has"
    // are one fact. A separate check would be a second implementation of it,
    // and each would hide the other being wrong.
    const bool from_result =
        out.from_side == (uint8_t)Side::Craft && out.from == kCraftResultSlot;
    if ((!from_result && out.from >= kMaxInventorySlots) || out.to >= kMaxInventorySlots)
        return false;

    return true;
}

// --- DropItem ---------------------------------------------------------------

std::vector<uint8_t> DropItem::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::DropItem);
    w.u16(slot);
    w.u16(count);
    return w.data();
}

bool DropItem::decode(ByteReader &r, DropItem &out)
{
    out = DropItem{};

    if (r.u8() != (uint8_t)MsgType::DropItem) return false;

    out.slot  = r.u16();
    out.count = r.u16();
    if (!r.done()) return false;

    // Same rule as MoveItem: a slot no bag reaches is refused here rather than
    // being carried to somewhere that has to know about bags to say no.
    if (out.slot >= kMaxInventorySlots) return false;

    return true;
}

// --- Edit -------------------------------------------------------------------

std::vector<uint8_t> Edit::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Edit);
    w.i32(x); w.i32(y); w.i32(z);
    w.u8((uint8_t)nx); w.u8((uint8_t)ny); w.u8((uint8_t)nz);
    w.u8(breaking);
    w.u8(slot);
    w.u8((uint8_t)((turn & 3) | (place_only ? 4 : 0)));
    return w.data();
}

bool Edit::decode(ByteReader &r, Edit &out)
{
    out = Edit{};

    if (r.u8() != (uint8_t)MsgType::Edit) return false;

    out.x = r.i32();
    out.y = r.i32();
    out.z = r.i32();
    out.nx = (int8_t)r.u8();
    out.ny = (int8_t)r.u8();
    out.nz = (int8_t)r.u8();
    out.breaking = r.u8();
    out.slot     = r.u8();
    const uint8_t turn = r.u8();
    out.turn       = turn & 3;
    out.place_only = (turn & 4) != 0;
    if (!r.done()) return false;

    // The same two checks PlacePortal makes, and for the same reasons: a face
    // is one of six, and an index no world reaches is refused before `x + nx`
    // is computed, because that overflows on INT32_MIN and the bounds check
    // downstream would never run.
    const int sum = std::abs((int)out.nx) + std::abs((int)out.ny) + std::abs((int)out.nz);
    if (sum != 1) return false;

    const int32_t kFar = 1 << 24;
    if (out.x < -kFar || out.x > kFar) return false;
    if (out.y < -kFar || out.y > kFar) return false;
    if (out.z < -kFar || out.z > kFar) return false;

    return true;
}

// --- BlockChange ------------------------------------------------------------

std::vector<uint8_t> BlockChange::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::BlockChange);
    w.i32(x); w.i32(y); w.i32(z);
    w.u16(id);
    w.u8(facing);
    w.str(why);
    return w.data();
}

bool BlockChange::decode(ByteReader &r, BlockChange &out)
{
    out = BlockChange{};

    if (r.u8() != (uint8_t)MsgType::BlockChange) return false;

    out.x  = r.i32();
    out.y  = r.i32();
    out.z  = r.i32();
    out.id     = r.u16();
    out.facing = r.u8();

    // Four quarter turns and nothing else. A byte holding 200 is a cell turned
    // to a face that does not exist, and the mesher would index a permutation
    // with it -- so it is refused here, where every other field this message
    // carries is checked, and not masked into range somewhere further in.
    // One of the twenty-four orientations. WHICH of them this block allows is
    // the block's business and the block table is not on this side of the
    // parse: a byte that no kind allows is refused here, and a byte the kind in
    // question does not allow is refused where the block is known.
    if (out.facing >= kOrientations) return false;

    if (!r.str(out.why, 256)) return false;   // the same bound every detail string here uses

    return r.done();
}

// --- CellOpen ---------------------------------------------------------------

std::vector<uint8_t> CellOpen::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::CellOpen);
    w.i32(x); w.i32(y); w.i32(z);
    w.u8(open ? 1 : 0);
    return w.data();
}

bool CellOpen::decode(ByteReader &r, CellOpen &out)
{
    out = CellOpen{};

    if (r.u8() != (uint8_t)MsgType::CellOpen) return false;

    out.x = r.i32();
    out.y = r.i32();
    out.z = r.i32();

    // One or nothing. A byte holding 200 would be "open" to every test that
    // asks `!= 0` and "not open" to one that asks `== 1`, and the two are in
    // different files.
    const uint8_t flag = r.u8();
    if (flag > 1) return false;
    out.open = flag;

    return r.done();
}

// --- ChunksGone -------------------------------------------------------------

std::vector<uint8_t> ChunksGone::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::ChunksGone);
    const size_t n = chunks.size() < kMaxChunksGone ? chunks.size() : kMaxChunksGone;
    w.u16((uint16_t)n);
    for (size_t i = 0; i < n; ++i) {
        w.i32(chunks[i].x);
        w.i32(chunks[i].y);
        w.i32(chunks[i].z);
    }
    return w.data();
}

bool ChunksGone::decode(ByteReader &r, ChunksGone &out)
{
    out = ChunksGone{};

    if (r.u8() != (uint8_t)MsgType::ChunksGone) return false;

    const size_t n = r.u16();
    if (n > kMaxChunksGone) return false;

    out.chunks.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        ChunkPos p;
        p.x = r.i32();
        p.y = r.i32();
        p.z = r.i32();
        out.chunks.push_back(p);
    }
    return r.done();
}

// --- InventoryState ---------------------------------------------------------

std::vector<uint8_t> InventoryState::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Inventory);
    w.u8(selected);

    const size_t n = slots.size() > kMaxInventorySlots ? kMaxInventorySlots : slots.size();
    w.u16((uint16_t)n);
    for (size_t i = 0; i < n; ++i) {
        w.u16(slots[i].item);
        w.u16(slots[i].count);
        w.u16(slots[i].used);
        w.u8(slots[i].ench);
        w.u32(slots[i].tag);
    }
    return w.data();
}

bool InventoryState::decode(ByteReader &r, InventoryState &out)
{
    out = InventoryState{};

    if (r.u8() != (uint8_t)MsgType::Inventory) return false;

    out.selected = r.u8();

    size_t n = 0;
    if (!r.count(n, kMaxInventorySlots)) return false;
    out.slots.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        Slot sl;
        sl.item  = r.u16();
        sl.count = r.u16();
        sl.used  = r.u16();
        sl.ench  = r.u8();
        sl.tag   = r.u32();
        if (!r.ok()) return false;

        // An item with no count and a count with no item are the same empty
        // slot, and a client that drew them differently would show a phantom.
        // Normalised here so nothing downstream has to know.
        if (sl.item == 0 || sl.count == 0) sl = Slot{};
        out.slots.push_back(sl);
    }

    return r.done();
}


// --- PanelShow --------------------------------------------------------------

std::vector<uint8_t> PanelShow::encode() const
{
    // `bw`, not `w`: this message HAS a field called w.
    ByteWriter bw;
    bw.u8((uint8_t)MsgType::PanelShow);
    bw.u8(open ? 1 : 0);

    // The name and the size go out even when nothing is up. They cost nothing,
    // and a "closed" message that says WHICH panel closed is the one a log line
    // can be written from -- a bare flag makes every close look the same.
    bw.str(name);
    bw.str(owner);
    bw.f32(w);
    bw.f32(h);

    const size_t n = elements.size() > kMaxWirePanelElements ? kMaxWirePanelElements
                                                             : elements.size();
    bw.u16((uint16_t)n);
    for (size_t i = 0; i < n; ++i) {
        const Elem &e = elements[i];
        bw.u8(e.kind);
        bw.str(e.id);
        bw.str(e.text);
        bw.f32(e.x);
        bw.f32(e.y);
        bw.f32(e.w);
        bw.f32(e.h);
        bw.u8(e.r);
        bw.u8(e.g);
        bw.u8(e.b);
    }
    return bw.data();
}

bool PanelShow::decode(ByteReader &r, PanelShow &out)
{
    out = PanelShow{};

    if (r.u8() != (uint8_t)MsgType::PanelShow) return false;

    out.open = r.u8() != 0;
    if (!r.str(out.name,  kMaxWirePanelText)) return false;
    if (!r.str(out.owner, kMaxWirePanelText)) return false;
    out.w = r.f32();
    out.h = r.f32();

    size_t n = 0;
    if (!r.count(n, kMaxWirePanelElements)) return false;
    out.elements.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        Elem e;
        e.kind = r.u8();
        if (!r.str(e.id,   kMaxWirePanelText)) return false;
        if (!r.str(e.text, kMaxWirePanelText)) return false;
        e.x = r.f32();
        e.y = r.f32();
        e.w = r.f32();
        e.h = r.f32();
        e.r = r.u8();
        e.g = r.u8();
        e.b = r.u8();
        if (!r.ok()) return false;

        // A kind this build has no drawing for is refused rather than drawn as
        // whatever kind 0 happens to be. Both sides are the same protocol
        // version, so a byte outside the enum did not come from a newer server;
        // it came from something that is not a server.
        if (e.kind > (uint8_t)ui::ElemKind::Model) return false;

        out.elements.push_back(std::move(e));
    }

    // Not a panel and not nothing: a size no screen can hold, or a panel that is
    // up and has no name to answer a click with. Refused here rather than
    // clamped for the reason ui::PanelSet refuses it -- see kMaxPanelCells.
    if (out.open) {
        if (out.name.empty()) return false;
        if (!(out.w > 0.0f) || !(out.h > 0.0f)) return false;
        if (out.w > ui::kMaxPanelCells || out.h > ui::kMaxPanelCells) return false;
    }

    return r.done();
}

// --- UiClick ----------------------------------------------------------------

std::vector<uint8_t> UiClick::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::UiClick);
    w.str(panel);
    w.str(id);
    w.str(text);
    return w.data();
}

bool UiClick::decode(ByteReader &r, UiClick &out)
{
    out = UiClick{};

    if (r.u8() != (uint8_t)MsgType::UiClick) return false;
    if (!r.str(out.panel, kMaxWirePanelText)) return false;
    if (!r.str(out.id,    kMaxWirePanelText)) return false;
    if (!r.str(out.text,  kMaxWirePanelText)) return false;

    // A click on nothing is not a click. The server would look up a panel named
    // "" and a button named "", find neither, and refuse -- but it would count
    // as a click first, and a peer that can spend the server's lookups for free
    // is a peer that has found a way to make noise.
    if (out.panel.empty() || out.id.empty()) return false;

    return r.done();
}

// --- ModData ----------------------------------------------------------------

std::vector<uint8_t> ModData::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::ModData);
    w.str(text);
    return w.data();
}

bool ModData::decode(ByteReader &r, ModData &out)
{
    out = ModData{};

    if (r.u8() != (uint8_t)MsgType::ModData) return false;

    // Capped, and the cap is the message's own rather than a mod's: a peer that
    // can make the server hold anything it likes has found a way to spend
    // memory for free. A mod with more to say than this sends twice.
    if (!r.str(out.text, kMaxWireModText)) return false;
    return r.done();
}

// --- Notice -----------------------------------------------------------------

std::vector<uint8_t> Hold::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Hold);
    w.u8(slot);
    w.u8((working ? 1 : 0) | (crouching ? 2 : 0));
    return w.data();
}

bool Hold::decode(ByteReader &r, Hold &out)
{
    out = Hold{};
    if (r.u8() != (uint8_t)MsgType::Hold) return false;
    out.slot          = r.u8();
    const uint8_t w   = r.u8();
    out.working       = (w & 1) != 0;
    out.crouching     = (w & 2) != 0;
    return r.done() && out.slot < 9 && w <= 3;   // Inventory::kHotbar, which this layer cannot see
}

std::vector<uint8_t> StatusQuery::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::StatusQuery);
    return w.data();
}

std::vector<uint8_t> Status::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Status);
    w.str(version);
    w.u16(players);
    w.u16(rooms);
    w.str(motd);
    w.str(maintenance);
    return w.data();
}

bool Status::decode(ByteReader &r, Status &out)
{
    out = Status{};
    if (r.u8() != (uint8_t)MsgType::Status) return false;
    if (!r.str(out.version, 64)) return false;
    out.players = r.u16();
    out.rooms   = r.u16();
    if (!r.str(out.motd, kMaxWireModText) || !r.str(out.maintenance, kMaxWireModText)) return false;
    return r.done();
}

std::vector<uint8_t> Notice::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Notice);
    w.str(text);
    return w.data();
}

bool Notice::decode(ByteReader &r, Notice &out)
{
    out = Notice{};

    if (r.u8() != (uint8_t)MsgType::Notice) return false;
    if (!r.str(out.text, kMaxWireModText)) return false;
    return r.done();
}

// --- Chat -------------------------------------------------------------------

std::vector<uint8_t> Dress::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Dress);
    w.u32(livery);
    return w.data();
}

bool Dress::decode(ByteReader &r, Dress &out)
{
    out = Dress{};
    if (r.u8() != (uint8_t)MsgType::Dress) return false;
    out.livery = r.u32();
    return r.done() && out.livery < kLiveryMax;
}

std::vector<uint8_t> OwnPose::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::OwnPose);
    w.str(clip);
    w.f32(yaw);
    w.u8(faced ? 1 : 0);
    for (int k = 0; k < 10; ++k) {
        w.u16(model[k]);
        w.u16(tint[k]);
    }
    return w.data();
}

bool OwnPose::decode(ByteReader &r, OwnPose &out)
{
    out = OwnPose{};
    if (r.u8() != (uint8_t)MsgType::OwnPose) return false;
    if (!r.str(out.clip, kMaxClipName)) return false;
    out.yaw   = r.f32();
    out.faced = r.u8() != 0;
    if (!std::isfinite(out.yaw)) return false;
    for (int k = 0; k < 10; ++k) {
        out.model[k] = r.u16();
        out.tint[k]  = r.u16();
    }
    return r.done();
}

std::vector<uint8_t> Chat::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Chat);
    w.str(text);
    return w.data();
}

bool Chat::decode(ByteReader &r, Chat &out)
{
    out = Chat{};

    if (r.u8() != (uint8_t)MsgType::Chat) return false;
    if (!r.str(out.text, kMaxWireModText)) return false;
    return r.done();
}

// --- CloseContainer ---------------------------------------------------------

std::vector<uint8_t> CloseContainer::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::CloseContainer);
    return w.data();
}

bool CloseContainer::decode(ByteReader &r, CloseContainer &out)
{
    out = CloseContainer{};
    if (r.u8() != (uint8_t)MsgType::CloseContainer) return false;
    return r.done();
}

// --- ContainerState ---------------------------------------------------------

std::vector<uint8_t> CraftState::encode() const
{
    ByteWriter w2;
    w2.u8((uint8_t)MsgType::Craft);
    w2.u8(w);
    w2.u8(h);
    w2.i32(x);
    w2.i32(y);
    w2.i32(z);

    const size_t n = (size_t)w * (size_t)h;
    for (size_t i = 0; i < n; ++i) {
        const bool have = i < slots.size();
        w2.u16(have ? slots[i].item : 0);
        w2.u16(have ? slots[i].count : 0);
        w2.u32(have ? slots[i].tag : 0);
    }

    w2.u16(result.item);
    w2.u16(result.count);
    w2.u32(result.tag);
    return w2.data();
}

bool CraftState::decode(ByteReader &r, CraftState &out)
{
    out = CraftState{};

    if (r.u8() != (uint8_t)MsgType::Craft) return false;

    out.w = r.u8();
    out.h = r.u8();
    out.x = r.i32();
    out.y = r.i32();
    out.z = r.i32();
    if (!r.ok()) return false;

    // A grid is two by two or three by three and nothing else. Checked before
    // anything is sized by it: w * h is a loop bound, and two bytes off the
    // wire must not decide how many times anything runs.
    if ((out.w != 2 || out.h != 2) && (out.w != 3 || out.h != 3)) return false;

    const size_t n = (size_t)out.w * (size_t)out.h;
    out.slots.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        InventoryState::Slot sl;
        sl.item  = r.u16();
        sl.count = r.u16();
        sl.tag   = r.u32();
        if (!r.ok()) return false;
        if (sl.item == 0 || sl.count == 0) { sl.item = 0; sl.count = 0; sl.tag = 0; }
        out.slots.push_back(sl);
    }

    out.result.item  = r.u16();
    out.result.count = r.u16();
    out.result.tag   = r.u32();
    if (out.result.item == 0 || out.result.count == 0) {
        out.result.item  = 0;
        out.result.count = 0;
    }

    return r.done();
}

std::vector<uint8_t> ContainerState::encode() const
{
    ByteWriter w;
    w.u8((uint8_t)MsgType::Container);
    w.u8(open ? 1 : 0);
    w.i32(x);
    w.i32(y);
    w.i32(z);

    const size_t n = slots.size() > kMaxInventorySlots ? kMaxInventorySlots : slots.size();
    w.u16((uint16_t)n);
    for (size_t i = 0; i < n; ++i) {
        w.u16(slots[i].item);
        w.u16(slots[i].count);
        w.u16(slots[i].used);
        w.u8(slots[i].ench);
        w.u32(slots[i].tag);
    }
    // And what each square is for (SlotRole), since protocol 79.
    for (size_t i = 0; i < n; ++i) w.u8(i < roles.size() ? roles[i] : (uint8_t)0);
    w.f32(progress);   // since protocol 80
    w.f32(rate);
    w.f32(scroll);     // since protocol 106
    w.f32(scroll_size);
    w.u8(search ? 1 : 0);
    w.str(query);
    return w.data();
}

bool ContainerState::decode(ByteReader &r, ContainerState &out)
{
    out = ContainerState{};

    if (r.u8() != (uint8_t)MsgType::Container) return false;

    out.open = r.u8() != 0;
    out.x    = r.i32();
    out.y    = r.i32();
    out.z    = r.i32();

    size_t n = 0;
    if (!r.count(n, kMaxInventorySlots)) return false;
    out.slots.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        InventoryState::Slot sl;
        sl.item  = r.u16();
        sl.count = r.u16();
        sl.used  = r.u16();
        sl.ench  = r.u8();
        sl.tag   = r.u32();
        if (!r.ok()) return false;

        // The same normalisation InventoryState does, and for the same reason:
        // an item with no count and a count with no item are one empty slot,
        // and a client that drew them differently would show a phantom.
        if (sl.item == 0 || sl.count == 0) sl = InventoryState::Slot{};
        out.slots.push_back(sl);
    }

    out.roles.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        const uint8_t role = r.u8();
        if (!r.ok() || role >= kRoleCount) return false;   // the six roles (79, 106), nothing past them
        out.roles.push_back(role);
    }
    out.progress = r.f32();
    out.rate     = r.f32();
    if (!r.ok() || !std::isfinite(out.progress) || !std::isfinite(out.rate) || out.progress > 1.0f ||
        out.rate < 0.0f || out.rate > 1000.0f)
        return false;
    out.scroll      = r.f32();
    out.scroll_size = r.f32();
    if (!r.ok() || !(out.scroll >= 0.0f && out.scroll <= 1.0f) ||
        !(out.scroll_size >= 0.0f && out.scroll_size <= 1.0f))
        return false;
    out.search = r.u8() != 0;
    if (!r.str(out.query, ui::kMaxPanelText)) return false;

    // A chest that is open and has no slots is not a chest. Closed, it has
    // none by construction, so the check belongs on this side of the flag.
    if (out.open && n == 0) return false;

    return r.done();
}

// --- dispatch ---------------------------------------------------------------

MsgType peek_type(const void *data, size_t size)
{
    if (data == nullptr || size == 0) return MsgType::Invalid;

    // No `default:` here, deliberately, and the comment is the finding: this
    // switch USED to have one, so adding a message type to the enum left it
    // silently unrecognised -- peek_type answered Invalid, on_message never
    // reached the handler, and the only symptom was a request that got no
    // reply. Without a default the compiler refuses to build until the new type
    // is listed. The byte that is not a MsgType at all falls out of the switch
    // to the return below, which is what the default was really for.
    const uint8_t t = ((const uint8_t *)data)[0];
    switch ((MsgType)t) {
    case MsgType::Invalid:
        return MsgType::Invalid;
    case MsgType::Hello:
    case MsgType::Welcome:
    case MsgType::Refuse:
    case MsgType::Chunk:
    case MsgType::Move:
    case MsgType::Correct:
    case MsgType::Entities:
    case MsgType::Teleport:
    case MsgType::PlacePortal:
    case MsgType::Portals:
    case MsgType::ChangeScope:
    case MsgType::ScopeDenied:
    case MsgType::TicketRequest:
    case MsgType::Ticket:
    case MsgType::Heartbeat:
    case MsgType::Redirect:
    case MsgType::MediaOffer:
    case MsgType::MediaNeed:
    case MsgType::MediaData:
    case MsgType::MediaMissing:
    case MsgType::Edit:
    case MsgType::BlockChange:
    case MsgType::Inventory:
    case MsgType::MoveItem:
    case MsgType::DropItem:
    case MsgType::PanelShow:
    case MsgType::UiClick:
    case MsgType::Container:
    case MsgType::CloseContainer:
    case MsgType::Craft:
    case MsgType::QuickMove:
    case MsgType::Vitals:
    case MsgType::Time:
    case MsgType::Hit:
    case MsgType::ModData:
    case MsgType::CellOpen:
    case MsgType::BodyMovement:
    case MsgType::ChunksGone:
    case MsgType::Notice:
    case MsgType::Hold:
    case MsgType::Push:
    case MsgType::Ride:
    case MsgType::Chat:
    case MsgType::Lie:
    case MsgType::Sound:
    case MsgType::Seal:
    case MsgType::Dress:
    case MsgType::StatusQuery:
    case MsgType::Status:
    case MsgType::OwnPose:
        return (MsgType)t;
    }

    return MsgType::Invalid;
}

// --- negotiation ------------------------------------------------------------

namespace {

// Hex, because every other hash this project prints is hex and a player
// comparing two numbers should not have to notice the base changed.
std::string hex64(uint64_t v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)v);
    return buf;
}

bool has(const std::vector<std::string> &v, const std::string &s)
{
    return std::find(v.begin(), v.end(), s) != v.end();
}

HandshakeResult refuse(RefuseReason reason, std::string detail)
{
    HandshakeResult res;
    res.accepted = false;
    res.reason   = reason;
    res.detail   = std::move(detail);
    return res;
}

} // namespace

HandshakeResult negotiate(const Hello &hello, const ServerConfig &config, bool vouched)
{
    if (hello.magic != kMagic)
        return refuse(RefuseReason::BadMagic, "this is not a Cubes and Portals server port");

    if (hello.min_protocol > hello.protocol)
        return refuse(RefuseReason::Malformed, "client sent an inverted version range");

    // The version both sides can speak is the highest inside both ranges. Which
    // side is behind decides the message, because "protocol mismatch" sends a
    // player to a forum and "your client is older than this server" sends them
    // to the download page.
    if (hello.protocol < config.min_protocol)
        return refuse(RefuseReason::VersionTooOld,
                      "client speaks up to " + std::to_string(hello.protocol) +
                          ", this server needs at least " + std::to_string(config.min_protocol));

    if (hello.min_protocol > config.protocol)
        return refuse(RefuseReason::VersionTooNew,
                      "client needs at least " + std::to_string(hello.min_protocol) +
                          ", this server speaks up to " + std::to_string(config.protocol));

    const uint16_t chosen = hello.protocol < config.protocol ? hello.protocol : config.protocol;

    if (!valid_player_name(hello.player_name))
        return refuse(RefuseReason::BadName,
                      "a name is 1 to 32 printable characters with no spaces");

    // Two capacities, because they are not the same resource. A world full of
    // players must still be watchable through a portal, and a world full of
    // watchers must still be playable.
    if (hello.scope == Scope::Observe) {
        if (!config.allow_observers && !vouched)
            return refuse(RefuseReason::NotWatchable, "this world does not allow observers");
        if (config.observers >= config.max_observers)
            return refuse(RefuseReason::ServerFull,
                          std::to_string(config.observers) + "/" +
                              std::to_string(config.max_observers) + " observers");
    } else if (config.players >= config.max_players) {
        return refuse(RefuseReason::ServerFull,
                      std::to_string(config.players) + "/" + std::to_string(config.max_players) +
                          " players");
    }

    for (const std::string &need : config.required_features) {
        if (!has(hello.features, need))
            return refuse(RefuseReason::MissingFeature, need);
    }

    // Same protocol, different block table. This is the case a version number
    // does not catch: both sides can be 0.3.0-m3 and speak protocol 1, and
    // still disagree about what block 3 is, because ids come from registration
    // order and one side registered a node the other has not got.
    //
    // Refused rather than warned about. The alternative is a player standing in
    // a world made of the wrong blocks, and the first thing they would report
    // is a rendering bug.
    const bool client_adopts = has(hello.features, kFeatureContentAdoption) &&
                               has(config.features, kFeatureContentAdoption);

    // A client that can adopt the server's table does not need to have the same
    // one: it is handed the table below and rebuilds its registry from it. A
    // client that cannot is refused, because it would read every chunk through
    // its own ids and draw a world out of the wrong blocks -- silently, which
    // is the part that makes it worth a refusal.
    if (!client_adopts && hello.content_hash != config.content_hash) {
        return refuse(RefuseReason::ContentMismatch,
                      "client table " + hex64(hello.content_hash) + ", server table " +
                          hex64(config.content_hash) +
                          " -- the two builds have different blocks or different mods (client '" +
                          hello.engine_version + "', server '" + config.engine_version + "')");
    }

    HandshakeResult res;
    res.accepted = true;

    Welcome &w   = res.welcome;
    w.protocol       = chosen;
    w.engine_version = config.engine_version;
    w.content_hash   = config.content_hash;
    w.seed           = config.seed;
    w.mapgen_hash = config.mapgen_hash;

    // To every client, adopting or not: a body moves by these whatever table it
    // reads blocks from (Welcome::movement).
    w.movement = config.movement;

    // Who this world is, as opposed to where it is. A server with no ticket key
    // has no such name and says so by leaving this empty -- which is honest and
    // costs it the ability to be recognised through a second address.
    if (config.has_sign_key) {
        std::memcpy(w.signing_key, config.sign_public, kSignPublicBytes);
        w.has_signing_key = true;
    }
    w.room    = config.room;
    w.has_sky = config.has_sky;
    std::memcpy(w.sky, config.sky, 3);
    w.has_storm = config.has_storm;
    std::memcpy(w.storm, config.storm, 3);

    // The intersection, walked in the server's order. Order matters: both sides
    // keep this table, and a set that is the same but arranged differently is
    // two tables that disagree the moment anyone indexes one.
    for (const std::string &f : config.features) {
        if (has(hello.features, f)) w.features.push_back(f);
        if (w.features.size() >= kMaxFeatures) break;
    }

    // The block table, but only to a client that said it would adopt it.
    // Sending 400 KB to a client that will ignore it is the kind of thing a
    // negotiated feature exists to prevent.
    if (client_adopts) {
        if (config.nodes.size() > kMaxNodeTable)
            return refuse(RefuseReason::ContentMismatch,
                          "this server has " + std::to_string(config.nodes.size()) +
                              " blocks, the protocol carries " +
                              std::to_string(kMaxNodeTable));
        w.nodes = config.nodes;

        if (config.shapes.size() > kMaxNodeShapes)
            return refuse(RefuseReason::ContentMismatch,
                          "this server has " + std::to_string(config.shapes.size()) +
                              " blocks with a shape, the protocol carries " +
                              std::to_string(kMaxNodeShapes));
        w.shapes = config.shapes;

        if (config.items.size() > kMaxItemTable)
            return refuse(RefuseReason::ContentMismatch,
                          "this server has " + std::to_string(config.items.size()) +
                              " items, the protocol carries " +
                              std::to_string(kMaxItemTable));
        w.items = config.items;
        w.node_digs   = config.node_digs;
        w.node_bodies = config.node_bodies;

        if (config.models.size() > kMaxModelTable)
            return refuse(RefuseReason::ContentMismatch,
                          "this server has " + std::to_string(config.models.size()) +
                              " models, the protocol carries " +
                              std::to_string(kMaxModelTable));
        w.models = config.models;
    }

    // Mod channels, assigned from 4 up. The names come from the server's mod
    // list, so every client on this server gets the same numbers; a client that
    // does not have one of these mods simply never sends on that channel.
    uint8_t next = kModChannelBase;
    for (const std::string &name : config.mod_channels) {
        if (next >= kMaxChannels) break;   // silently capped is wrong; see below
        w.channels.push_back(ChannelAssignment{name, next});
        ++next;
    }

    // Over the cap the server refuses the connection rather than handing out a
    // truncated table: a mod whose channel is missing would send on whatever it
    // assumed, which is another mod's channel.
    if (config.mod_channels.size() > (size_t)(kMaxChannels - kModChannelBase))
        return refuse(RefuseReason::Malformed,
                      "server has " + std::to_string(config.mod_channels.size()) +
                          " mod channels, the protocol has room for " +
                          std::to_string(kMaxChannels - kModChannelBase));

    return res;
}

bool accept_welcome(const Welcome &welcome, const Hello &sent, std::string &err)
{
    if (welcome.magic != kMagic) {
        err = "the welcome is not this protocol";
        return false;
    }

    if (welcome.protocol > sent.protocol || welcome.protocol < sent.min_protocol) {
        err = "server chose protocol " + std::to_string(welcome.protocol) +
              ", outside the range this client offered (" + std::to_string(sent.min_protocol) +
              ".." + std::to_string(sent.protocol) + ")";
        return false;
    }

    // Checked on both sides, and not out of symmetry: the server's check
    // protects the server's world from a client that would misread it, and this
    // one protects the player from a server that did not check. Neither side
    // gets to rely on the other having looked.
    const bool adopting = has(sent.features, kFeatureContentAdoption) &&
                          has(welcome.features, kFeatureContentAdoption);

    if (adopting) {
        // The table replaces the check: whatever the server's blocks are, they
        // are about to become this client's blocks. What is checked instead is
        // that the server actually sent one.
        if (welcome.nodes.empty()) {
            err = "the server agreed to send its block table and did not";
            return false;
        }
        // Position zero is air in every registry ever built, because the
        // registry's constructor puts it there. A table that does not start
        // with it is not a table this client can adopt.
        if (welcome.nodes[0] != "core:air") {
            err = "the server's block table does not start with core:air";
            return false;
        }
    } else if (welcome.content_hash != sent.content_hash) {
        err = "the server's block table (" + hex64(welcome.content_hash) +
              ") is not this build's (" + hex64(sent.content_hash) + "); server is '" +
              welcome.engine_version + "'";
        return false;
    }

    // A server cannot switch on something the client never said it could do.
    // Without this the client happily records a feature it has no code for, and
    // finds out later, in whichever subsystem assumed the table was honest.
    for (const std::string &f : welcome.features) {
        if (!has(sent.features, f)) {
            err = "server enabled '" + f + "', which this client did not offer";
            return false;
        }
    }

    // Decode already refused a channel outside the mod range. Duplicates it
    // cannot see, and two mods on one channel is two mods reading each other's
    // messages.
    for (size_t i = 0; i < welcome.channels.size(); ++i) {
        for (size_t j = i + 1; j < welcome.channels.size(); ++j) {
            if (welcome.channels[i].id == welcome.channels[j].id) {
                err = "channel " + std::to_string((int)welcome.channels[i].id) +
                      " was assigned twice";
                return false;
            }
            if (welcome.channels[i].name == welcome.channels[j].name) {
                err = "channel '" + welcome.channels[i].name + "' was assigned twice";
                return false;
            }
        }
    }

    err.clear();
    return true;
}

} // namespace net
} // namespace cnp
