#pragma once

#include "client/gfx/gfx.h"
#include "client/render/water.h"
#include "core/math/mat4.h"
#include "core/media/gltf.h"
#include "core/physics/aabb.h"
#include "core/portal/portal_clip.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cnp {

// A solid that can go through a portal.
//
// A box for now, because everything this engine has that moves is box shaped:
// the player's collision volume is one, and there are no models yet. What
// matters is not the shape but that it is FILLED, because the whole point is
// that it gets cut and a cut is invisible on a wireframe.
struct Body {
    Vec3  center;
    Vec3  half{0.3f, 0.9f, 0.3f};
    float colour[4]{0.95f, 0.75f, 0.15f, 1.0f};

    // Which way it is facing, and what it looks like. Both are ignored by this
    // renderer, which draws a box and has nothing to turn: they are here
    // because the list of bodies is what the frame loop hands to whatever
    // draws them, and a second list for the ones wearing a model would be the
    // same bodies in two places.
    //
    // `model` is the id off the wire -- a position in the world's model table
    // plus one -- and 0 is a body with nothing said about it, which is the box.
    float    yaw   = 0.0f;
    uint16_t model = 0;

    // How a chassis sits (Player::body_pitch, body_roll): nose up and right
    // side down, about its feet. Zero for everything that is not the player's
    // own kart -- the wire carries a body's yaw and nothing else.
    float pitch = 0.0f;
    float roll  = 0.0f;

    // What its wheels are doing, for a model that has some (wheel_pose):
    // -1 .. 1 of full lock, right positive, and the metres rolled forwards.
    // Read off how the body has moved (animate_body), not off the wire.
    float steer     = 0.0f;
    float travelled = 0.0f;

    // What its model plays: the clip fading out, then the one fading in. And
    // who sits in it when it is a vehicle (a model id, 0 for nobody), playing
    // what a rider plays.
    ClipAt   clips[3];   // the body's two (a crossfade), and an arms-only clip over them
    uint16_t rider = 0;
    ClipAt   rider_clips[2];
    bool     rider_headless = false;   // the rider seen from inside its own head
    bool     arms_only = false;        // the body itself seen from its own head, in a seat: its arms alone
    bool     dress_in_view = false;   // with arms_only: what a mod dresses it in too (a cart)

    // What is in its hand, by item name ("tools:pick"), when this machine
    // knows. A tool_<short name> bone is shown for it and every other tool_*
    // hidden, and the model "<item>_held" is drawn in the palm when the world
    // has one; an unknown hand leaves the tools to the clip.
    bool        hand_known = false;
    std::string hand;
    uint16_t    hand_block = 0;   // the block it holds, 0 (air) for none

    // A stack on the ground: `hand` and `hand_block` are what lies there, drawn
    // as the thing itself about `center`, turned by `yaw` (draw_drop).
    bool dropped = false;
    bool falling = false;   // and it is a block falling (core.fall): `hand_block`, whole

    // What it wears, head to feet, by item name ("armor:iron_helmet"): each is
    // drawn as the model of that name on this body's skeleton. Empty for none.
    std::string worn[5];
    uint16_t    worn_tint[5] = {0, 0, 0, 0, 0};   // each one's colours, tag >> kTagIdBits
    // What the back's arms hold, left and right, by item name: drawn in the
    // claws of the worn back piece (draw_worn). Empty for none.
    std::string arm[2];
    // What a mod dresses it in (core.player_wear, protocol 102): model ids on
    // its skeleton, as worn pieces, and their colours. 0 for none.
    uint16_t    dress[10]      = {};
    uint16_t    dress_tint[10] = {};
    bool        arms_working = false;   // at work: the arms holding something strike
    uint32_t    hand_tag = 0;                      // the held stack's (ItemStack::tag); 0 for others'

    // A person's colours and extras (net::Hello::livery): its model's tint_1..3
    // in them, and "<model>_<extra>" worn. 0 for the model as made.
    uint32_t livery = 0;

    // Under 1 while it fades out (fade_the_gone): a model drawn see-through.
    float alpha = 1.0f;

    // Who it is (the entity's id; kOwnBody for this machine's own), for what
    // follows a body from one frame to the next: the marks its tyres leave.
    uint64_t id = 0;
    // A kart's nitro burning and its tyres sliding (net::EntityState::boosting,
    // DriveLook): flames out of its exhausts, marks on the road.
    bool boosting = false;
    bool sliding  = false;

    AABB bounds() const
    {
        return AABB{Vec3{center.x - half.x, center.y - half.y, center.z - half.z},
                    Vec3{center.x + half.x, center.y + half.y, center.z + half.z}};
    }
};

// Draws bodies, whole or cut.
//
// The cut is done with a clip plane rather than by building new geometry, and
// that is the decision worth defending: rebuilding a mesh clipped to a plane
// every frame costs a mesh rebuild every frame, and produces a solid whose
// interior is hollow -- you would see through the cut into an empty shell. A
// clip plane costs one uniform and shows the same hollow interior, which is
// what a real portal-puzzle cut shows too; capping the cut with a face is a
// separate piece of work and it is on M4's list rather than pretended.
class BodyRenderer {
public:
    bool init();
    void shutdown();

    // The whole body, unclipped.
    void draw(const Body &body, const Mat4 &projection, const Mat4 &view);

    // The body clipped to keep the positive side of `plane`, with an optional
    // extra world-space transform applied first -- which is what turns the
    // original into the copy that comes out of the far side.
    void draw_clipped(const Body &body, const Plane &plane, const Mat4 &transform,
                      const Mat4 &projection, const Mat4 &view);

    // How many boxes this renderer has drawn. For the gate that asks whether a
    // body wearing a model still costs a box, which is a question about the
    // DRAW and not about the pixels -- a box can be drawn and invisible: a
    // character is exactly as big as the body it stands in, so the box hides
    // inside it, and a box that meets a portal is cut away by a clip plane and
    // paints nothing at all.
    long draws() const { return m_draws; }
    void reset_draws() { m_draws = 0; }

private:
    void draw_impl(const Body &body, const Plane *plane, const Mat4 &transform,
                   const Mat4 &projection, const Mat4 &view);

    gfx_shader m_shader{0};
    gfx_buffer m_vb{0};
    gfx_mesh   m_mesh{0};
    int        m_vertex_count = 0;
    int        m_loc_color    = -1;
    WaterUniforms m_water;
    long       m_draws        = 0;
};

} // namespace cnp
