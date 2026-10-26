#pragma once

#include <string>
#include <vector>

#include "script/script.h"

// Render passes a mod registers (DESIGN.md 8, "render pass registrabili").
//
// What makes this a PASS and not the core:tick event that already exists: a
// tick handler runs between frames, with no camera set up and nothing it is
// allowed to draw into. A pass runs INSIDE the render, at the point in the
// pipeline the engine calls it, with the projection and the view of the camera
// being drawn -- which is the whole difference, because a portal draws the same
// world several times from several cameras and a mod's drawing has to happen
// once per camera and not once per frame.
//
// The order number orders passes against EACH OTHER and not against the
// engine's own drawing. Said out loud because it is the obvious thing to assume
// otherwise: there is one point in the frame where mods draw -- after the
// opaque world and the bodies, before the openings -- and a mod that wants to
// be painted over by another asks for a bigger number.

namespace cnp {

struct ModPass {
    std::string name;
    std::string owner;
    int         order = 0;
    script_vm  *vm    = nullptr;
    int         ref   = 0;
    uint64_t    seq   = 0;   // ties go to whoever registered first, not to luck
};

class ModPasses {
public:
    // Registering the same name twice from the same VM replaces it. From
    // another VM of the same mod -- a reload, or another world's copy -- it is
    // a second entry: the reload's old VM drops its own (drop_vm), and two
    // worlds each draw theirs. From a different mod it is an error, like an
    // event's schema.
    bool add(const std::string &name, int order, script_vm *vm, int handler_ref,
             const std::string &owner, std::string &error);

    void drop_vm(script_vm *vm);

    // In the order they draw. Sorted on demand rather than kept sorted: this is
    // a handful of entries, registered at load and read once per camera.
    std::vector<const ModPass *> in_order() const;

    size_t size() const { return m_passes.size(); }

private:
    std::vector<ModPass> m_passes;
    uint64_t             m_seq = 0;
};

} // namespace cnp
