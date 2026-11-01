#include "client/ui/title.h"

#include "client/gfx/gfx.h"
#include "core/version.h"

namespace cnp {

void panel_add(ui::Panel &p, ui::ElemKind kind, std::string id, std::string text, float x,
               float y, float w)
{
    ui::Element e;
    e.kind = kind;
    e.id   = std::move(id);
    e.text = std::move(text);
    e.x    = x;
    e.y    = kind == ui::ElemKind::Label ? y + 0.33f : y;
    e.w    = w;
    e.h    = 1.0f;
    p.elements.push_back(std::move(e));
}

void panel_button(ui::Panel &p, const char *id, std::string text, float x, float y, float w)
{
    panel_add(p, ui::ElemKind::Button, id, std::move(text), x, y, w);
}

void panel_label(ui::Panel &p, std::string text, float x, float y)
{
    panel_add(p, ui::ElemKind::Label, "", std::move(text), x, y, 0.0f);
}

// Behind the title screen: a sky, a row of blocks and a pair of openings on
// them, out of rectangles, so it needs nothing loaded.
void draw_title_backdrop()
{
    const float w = (float)gfx_screen_width(), h = (float)gfx_screen_height();
    const float ground = h * 0.72f, block = w / 24.0f;

    for (int i = 0; i < 24; ++i) {
        const float t      = (float)i / 23.0f;
        const float sky[4] = {0.18f + 0.32f * t, 0.30f + 0.38f * t, 0.55f + 0.33f * t, 1.0f};
        gfx_2d_rect(0.0f, ground * (float)i / 24.0f, w, ground / 24.0f + 1.0f, sky);
    }
    const float dirt[4] = {0.45f, 0.31f, 0.20f, 1.0f}, grass[4] = {0.33f, 0.62f, 0.25f, 1.0f},
                seam[4] = {0.0f, 0.0f, 0.0f, 0.25f};
    gfx_2d_rect(0.0f, ground, w, h - ground, dirt);
    gfx_2d_rect(0.0f, ground, w, block * 0.3f, grass);
    for (float y = ground; y < h; y += block) gfx_2d_rect(0.0f, y, w, 2.0f, seam);
    for (float x = 0.0f; x < w; x += block) gfx_2d_rect(x, ground, 2.0f, h - ground, seam);

    const float frame[4] = {0.22f, 0.22f, 0.26f, 1.0f};
    const float blue[4] = {0.20f, 0.55f, 1.0f, 1.0f}, orange[4] = {1.0f, 0.55f, 0.12f, 1.0f};
    for (const float cx : {w * 0.14f, w * 0.86f}) {
        const float bw = block * 1.2f, bh = block * 2.4f, e = block * 0.25f;
        gfx_2d_rect(cx - bw * 0.5f - e, ground - bh - e, bw + 2.0f * e, bh + e, frame);
        gfx_2d_rect(cx - bw * 0.5f, ground - bh, bw, bh, cx < w * 0.5f ? blue : orange);
    }
}

// The name of the game over it all, after the panel so its sheet does not dim it.
void draw_title_name()
{
    const float w = (float)gfx_screen_width(), h = (float)gfx_screen_height();
    const char *name = "Cubes and Portals";
    const float size = h * 0.09f, tw = gfx_2d_text_width(name, size);
    const float shadow[4] = {0.0f, 0.0f, 0.0f, 0.6f}, white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    gfx_2d_text(name, (w - tw) * 0.5f + size * 0.06f, h * 0.08f + size * 0.06f, size, shadow);
    gfx_2d_text(name, (w - tw) * 0.5f, h * 0.08f, size, white);

    // The version in a corner, for whoever reports a bug from an alpha.
    const std::string version = std::string("v") + kEngineVersion;
    const float       small = h * 0.022f, grey[4] = {1.0f, 1.0f, 1.0f, 0.55f};
    gfx_2d_text(version.c_str(), w - gfx_2d_text_width(version.c_str(), small) - small,
                h - small * 1.8f, small, grey);
}

} // namespace cnp
