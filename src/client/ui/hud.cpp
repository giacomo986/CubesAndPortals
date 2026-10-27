#include "client/ui/hud.h"

#include "client/ui/lang.h"
#include "core/track/piece.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cnp::ui {

namespace {

// The bar's proportions, as fractions of the screen's shorter side rather than
// as pixels. A hotbar sized in pixels is a hotbar that is a postage stamp at
// 4K and covers the screen at 800x600, and every gate here reads a rectangle
// this returns rather than a constant, so changing these does not silently
// move what the gate is looking at.
constexpr float kSlotOfHeight = 0.075f;   // one slot's side
constexpr float kPadOfSlot    = 0.10f;    // gap between slots
constexpr float kBottomMargin = 0.02f;    // above the bottom edge

// The open bag's squares, smaller than the bar's: a double chest is six rows
// over the bag's four, and at this side both fit on the screen whole.
constexpr float kPanelSlotOfHeight = 0.048f;

constexpr float kBackdrop[4] = {0.05f, 0.05f, 0.06f, 0.72f};
constexpr float kSlotFill[4] = {0.16f, 0.16f, 0.18f, 0.85f};
constexpr float kSelected[4] = {0.95f, 0.90f, 0.55f, 1.00f};
constexpr float kBorder[4]   = {0.35f, 0.35f, 0.38f, 0.90f};
constexpr float kWhite[4]    = {1.00f, 1.00f, 1.00f, 1.00f};
constexpr float kShadow[4]   = {0.00f, 0.00f, 0.00f, 0.85f};

// The health bar. Red for what is there and a dark ghost for what is not: a bar
// that only drew what remains cannot be read as a fraction, because there is
// nothing to read it against.
constexpr float kHeart[4]      = {0.86f, 0.22f, 0.24f, 1.00f};
constexpr float kEmptyHeart[4] = {0.20f, 0.09f, 0.10f, 0.85f};
constexpr float kPicked[4]   = {0.45f, 0.85f, 0.95f, 1.00f};

// A rectangle drawn as four thin ones. There is no line primitive in gfx_2d_*
// and there does not need to be: a border is a rectangle with a hole, and the
// hole is what gets drawn over it.
void frame(float x, float y, float w, float h, float t, const float rgba[4])
{
    gfx_2d_rect(x, y, w, t, rgba);
    gfx_2d_rect(x, y + h - t, w, t, rgba);
    gfx_2d_rect(x, y, t, h, rgba);
    gfx_2d_rect(x + w - t, y, t, h, rgba);
}

// One square: its fill, what is in it, how many, and its border. Shared by the
// bar and the panel because they ARE the same square -- drawn twice with two
// bits of arithmetic is how the two come to disagree about what a slot looks
// like, and a player looking at both at once would see it.
void draw_slot(float sx, float sy, float sw, float sh, const ItemStack &stack,
               const ItemRegistry &items, const NodeRegistry &nodes, const IconSource &icons,
               const float border_rgba[4])
{
    gfx_2d_rect(sx, sy, sw, sh, kSlotFill);

    if (!stack.empty()) {
        const BlockId places = items.get(stack.item).places;
        const int     tiles  = icons.tiles > 0 ? icons.tiles : 1;

        // The item's OWN picture first, when somebody drew one. It is a tile in
        // the same atlas the blocks use -- see plan_tiles -- so a tool is drawn
        // exactly the way a block is, and a pack dresses it with one line in its
        // manifest. Zero means nobody drew it, and then the block it places is
        // the next best answer: the item form of stone should look like stone
        // whether or not a pack has a separate picture for the stack.
        const uint16_t own = items.get(stack.item).tex;

        const PieceShape *piece = places != kAir && nodes.get(places).piece.plan != 0
                                      ? &nodes.get(places).piece : nullptr;

        if (piece != nullptr) {
            // A track piece: every one is the same grey block, so it is drawn
            // as its plan from above instead -- entered at the bottom, its
            // road sampled on a grid and lighter where it is higher, which
            // tells a turn from a chicane and a climb from a bank.
            int   nx, nz;
            float lo, hi;
            piece_size(*piece, 2, nx, nz);
            piece_heights(*piece, 2, lo, hi);
            constexpr int kGrid = 16;   // samples along the longer side
            const float   inset = sw * 0.1f;
            const float   side  = (float)std::max(nx, nz);
            const float   cell  = (sw - inset * 2.0f) / (float)kGrid;
            const int     gx = (int)std::lround(kGrid * nx / side), gz = (int)std::lround(kGrid * nz / side);
            const float   x0 = sx + (sw - cell * (float)gx) * 0.5f, y0 = sy + (sh - cell * (float)gz) * 0.5f;
            bool          any = false;
            for (int i = 0; i < gx; ++i)
                for (int j = 0; j < gz; ++j) {
                    float y;
                    // Unturned a turn bends to -x, which is the right as it
                    // is driven: x runs right to left, z bottom to top.
                    if (!piece_surface(*piece, 2, side * ((float)i + 0.5f) / kGrid,
                                       side * ((float)j + 0.5f) / kGrid, y))
                        continue;
                    const float t      = hi > lo ? (y - lo) / (hi - lo) : 0.0f;
                    const float k      = 0.45f + 0.55f * t;
                    const float rgba[4] = {0.62f * k, 0.66f * k, 0.78f * k, 1.0f};
                    gfx_2d_rect(x0 + cell * (float)(gx - 1 - i), y0 + cell * (float)(gz - 1 - j),
                                cell + 0.5f, cell + 0.5f, rgba);
                    any = true;
                }
            // A wall standing up all along is a line from above: drawn as one,
            // along its middle.
            if (!any) {
                const float rgba[4] = {0.62f, 0.66f, 0.78f, 1.0f};
                gfx_2d_rect(x0 + cell * ((float)gx * 0.5f - 1.0f), y0, cell * 2.0f, cell * (float)gz, rgba);
            }
        } else if (own != 0 && icons.atlas.id != 0) {
            const float per = 1.0f / (float)tiles;
            const float col = (float)(own % tiles);
            const float row = (float)(own / tiles);

            const float inset = sw * 0.14f;
            gfx_2d_image(icons.atlas, col * per, row * per, per, per, sx + inset, sy + inset,
                         sw - inset * 2.0f, sh - inset * 2.0f, kWhite);
        } else if (places != kAir && icons.atlas.id != 0) {
            // The same arithmetic kWorldFS does: tile t is at column t % tiles,
            // row t / tiles, each one 1/tiles of the atlas. Written here rather
            // than shared with the shader because one is GLSL and the other is
            // not -- what keeps them honest is that the gate compares the icon
            // against the block.
            const int   tile = nodes.get(places).tex[0];
            const float per  = 1.0f / (float)tiles;
            const float col  = (float)(tile % tiles);
            const float row  = (float)(tile / tiles);

            const float inset = sw * 0.14f;
            gfx_2d_image(icons.atlas, col * per, row * per, per, per, sx + inset, sy + inset,
                         sw - inset * 2.0f, sh - inset * 2.0f, kWhite);
        } else {
            // An item with no block and no picture of its own -- a tool nobody
            // drew. Its name, a word a line: a bucket, a full bucket and an axe
            // drawn as the same grey square were three things a player could
            // only tell apart by hovering each one (seen in play, 2026-09-19).
            const std::string &name  = items.get(stack.item).name;
            const size_t       colon = name.find(':');
            const std::string  short_name =
                colon == std::string::npos ? name : name.substr(colon + 1);

            // The first word and the last, which for mod:iron_pick is "iron" and
            // "pick" and for mod:bucket_water is "bucket" and "water".
            const size_t first_end  = short_name.find('_');
            const size_t last_start = short_name.rfind('_');
            std::string  lines[2]   = {short_name.substr(0, first_end), ""};
            if (first_end != std::string::npos) lines[1] = short_name.substr(last_start + 1);

            const int   count = lines[1].empty() ? 1 : 2;
            const float room  = sw * 0.84f;
            float       size  = sw * 0.26f;
            for (const std::string &l : lines)
                if (!l.empty()) {
                    const float w = gfx_2d_text_width(l.c_str(), size);
                    if (w > room) size *= room / w;
                }

            // Higher when a count will be written in the bottom corner.
            const float top =
                sy + (sh - size * (float)count * 1.1f) * (stack.count > 1 ? 0.2f : 0.45f);
            for (int i = 0; i < count; ++i) {
                const float tw = gfx_2d_text_width(lines[i].c_str(), size);
                const float tx = sx + (sw - tw) * 0.5f;
                const float ty = top + (float)i * size * 1.1f;
                gfx_2d_text(lines[i].c_str(), tx + 1.0f, ty + 1.0f, size, kShadow);
                gfx_2d_text(lines[i].c_str(), tx, ty, size, kWhite);
            }
        }

        // Enchanted: a violet glint over it, stronger a level.
        if (stack.ench > 0) {
            const float glint[4] = {0.62f, 0.3f, 1.0f, 0.14f + 0.08f * (float)enchant_level(stack.ench)};
            gfx_2d_rect(sx + sw * 0.08f, sy + sh * 0.08f, sw * 0.84f, sh * 0.84f, glint);
        }

        // Its colours (ItemStack::tag), three marks down the right-hand side,
        // outlined so white on white still shows.
        for (int k = 0; k < 3; ++k) {
            const int dye = tag_colour(stack.tag, k);
            if (dye == 0) continue;
            const float m  = sw * 0.16f;
            const float mx = sx + sw * 0.78f, my = sy + sh * 0.1f + (float)k * (m + sh * 0.04f);
            const float c[4] = {kDyeColours[dye - 1][0] / 255.0f, kDyeColours[dye - 1][1] / 255.0f,
                                kDyeColours[dye - 1][2] / 255.0f, 1.0f};
            gfx_2d_rect(mx - 1.0f, my - 1.0f, m + 2.0f, m + 2.0f, kShadow);
            gfx_2d_rect(mx, my, m, m, c);
        }

        // How worn it is, once it is: a strip along the bottom, green going red.
        const uint16_t last = items.get(stack.item).durability;
        if (stack.used > 0 && last > 0) {
            const float left = 1.0f - (float)stack.used / (float)last;
            const float bx = sx + sw * 0.12f, by = sy + sh * 0.84f, bw = sw * 0.76f, bh = sh * 0.07f;
            const float bar[4] = {1.0f - left, left, 0.1f, 1.0f};
            gfx_2d_rect(bx, by, bw, bh, kShadow);
            gfx_2d_rect(bx, by, bw * (left > 0.0f ? left : 0.0f), bh, bar);
        }

        if (stack.count > 1) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%u", (unsigned)stack.count);
            const float size = sw * 0.34f;
            const float tw   = gfx_2d_text_width(buf, size);
            const float tx   = sx + sw - tw - sw * 0.08f;
            const float ty   = sy + sh - size - sw * 0.04f;
            gfx_2d_text(buf, tx + 1.0f, ty + 1.0f, size, kShadow);
            gfx_2d_text(buf, tx, ty, size, kWhite);
        }
    }

    frame(sx, sy, sw, sh, sw * 0.06f, border_rgba);
}

} // namespace

void HotbarLayout::slot_rect(int i, float &out_x, float &out_y, float &out_w, float &out_h) const
{
    if (i < 0 || i >= slots) {
        out_x = out_y = out_w = out_h = 0.0f;
        return;
    }
    out_x = x + (float)i * (slot + pad);
    out_y = y;
    out_w = slot;
    out_h = slot;
}

HotbarLayout draw_hotbar(const Inventory &bag, const ItemRegistry &items,
                         const NodeRegistry &nodes, const IconSource &icons,
                         int screen_w, int screen_h)
{
    return draw_hotbar(bag.slots().data(), bag.size(), bag.selected(), items, nodes, icons,
                       screen_w, screen_h);
}

HotbarLayout draw_hotbar(const ItemStack *slots, int count, int selected,
                         const ItemRegistry &items, const NodeRegistry &nodes,
                         const IconSource &icons, int screen_w, int screen_h)
{
    HotbarLayout out;

    if (slots == nullptr) return out;
    const int n = count < Inventory::kHotbar ? count : Inventory::kHotbar;
    if (n <= 0 || screen_w <= 0 || screen_h <= 0) return out;

    const float side  = (float)screen_h * kSlotOfHeight;
    const float pad   = side * kPadOfSlot;
    const float total = (float)n * side + (float)(n - 1) * pad;

    out.slots = n;
    out.slot  = side;
    out.pad   = pad;
    out.x     = ((float)screen_w - total) * 0.5f;
    out.y     = (float)screen_h - side - (float)screen_h * kBottomMargin;

    // One backdrop behind the whole row rather than one per slot: the bar reads
    // as a bar, and a slot that happens to be empty does not look like a hole
    // in the screen.
    const float edge = pad;
    gfx_2d_rect(out.x - edge, out.y - edge, total + edge * 2.0f, side + edge * 2.0f, kBackdrop);

    for (int i = 0; i < n; ++i) {
        float sx, sy, sw, sh;
        out.slot_rect(i, sx, sy, sw, sh);
        draw_slot(sx, sy, sw, sh, slots[i], items, nodes, icons,
                  i == selected ? kSelected : kBorder);
    }

    // What the hand is on, named above the bar. The name and not the id: an id
    // is meaningless to whoever is reading it, which is the same argument
    // core.node_name makes.
    const ItemStack &held = slots[(selected >= 0 && selected < n) ? selected : 0];
    if (!held.empty()) {
        const std::string &name = items.get(held.item).name;
        const float        size = side * 0.30f;
        const float        tw   = gfx_2d_text_width(name.c_str(), size);
        const float        tx   = ((float)screen_w - tw) * 0.5f;
        const float        ty   = out.y - edge - size - side * 0.12f;
        gfx_2d_text(name.c_str(), tx + 1.0f, ty + 1.0f, size, kShadow);
        gfx_2d_text(name.c_str(), tx, ty, size, kWhite);
    }

    return out;
}

void PanelLayout::slot_rect(int i, float &out_x, float &out_y, float &out_w, float &out_h) const
{
    if (i < 0 || i >= slots || cols <= 0) {
        out_x = out_y = out_w = out_h = 0.0f;
        return;
    }
    if (i == loose) {
        out_x = loose_x;
        out_y = loose_y;
        out_w = slot;
        out_h = slot;
        return;
    }
    if (worn_from >= 0 && i >= worn_from) {
        // Where on the figure, in slots across it and rows down the grid: head,
        // chest, legs, feet, back, then the arms -- the wearer's left on the
        // right, the figure facing out of the screen.
        static constexpr float kAcross[] = {2.0f, 2.0f, 2.0f, 2.0f, 0.5f, 3.5f, 0.5f};
        static constexpr int   kDown[]   = {0, 1, 2, 3, 1, 0, 0};
        const int k = i - worn_from;
        if (k >= (int)(sizeof kDown / sizeof kDown[0]) || (k >= Inventory::kWorn && k - Inventory::kWorn >= arms)) {
            out_x = out_y = out_w = out_h = 0.0f;   // shut: nothing there to hit
            return;
        }
        out_x = figure + kAcross[k] * slot - slot * 0.5f;
        out_y = y + (float)kDown[k] * (slot + pad);
        out_w = slot;
        out_h = slot;
        return;
    }
    int row = i / cols;
    float drop = 0.0f;
    if (hotbar_gap > 0.0f) {
        row  = row == 0 ? rows - 1 : row - 1;
        drop = row == rows - 1 ? hotbar_gap : 0.0f;
    }
    out_x = x + (float)(i % cols) * (slot + pad);
    out_y = y + (float)row * (slot + pad) + drop;
    out_w = slot;
    out_h = slot;
}

int PanelLayout::slot_at(float px, float py) const
{
    for (int i = 0; i < slots; ++i) {
        float sx, sy, sw, sh;
        slot_rect(i, sx, sy, sw, sh);
        if (px >= sx && px < sx + sw && py >= sy && py < sy + sh) return i;
    }
    return -1;
}

void Notice::say(const std::string &what, double now)
{
    // An empty sentence is not a notice. The session layer answers an empty
    // hand with an empty string on purpose (SessionAction::edit_refusal), and
    // "you were not holding anything" is not worth a line on the screen.
    if (what.empty()) return;

    text    = lang::tr(what);
    said_at = now;
}

float Notice::alpha(double now) const
{
    // No check for empty text, and that is deliberate rather than an omission:
    // there is no path to a recent said_at with nothing said, because say()
    // refuses an empty sentence. A second guard here would be a second
    // implementation of one rule, and each would hide the other being wrong --
    // which is what sabotaging them one at a time showed.
    const double age = now - said_at;
    if (age < 0.0) return 0.0f;              // a clock that went backwards
    if (age <= kNoticeHold) return 1.0f;
    if (age >= kNoticeHold + kNoticeFade) return 0.0f;

    return (float)(1.0 - (age - kNoticeHold) / kNoticeFade);
}

void draw_notice(const std::string &text, float alpha, int screen_w, int screen_h)
{
    if (alpha <= 0.0f || text.empty()) return;

    const float shorter = (float)(screen_w < screen_h ? screen_w : screen_h);
    const float size    = shorter * 0.035f;
    const float tw      = gfx_2d_text_width(text.c_str(), size);

    // Above the bar, which is where a player's eyes already are when they have
    // just clicked. The offset is the bar's own height plus a gap, computed
    // from the same fractions the bar uses rather than from a number that would
    // drift away from it.
    const float slot = shorter * kSlotOfHeight;
    const float y    = (float)screen_h - shorter * kBottomMargin - slot * 1.20f - size * 2.2f;
    const float x    = ((float)screen_w - tw) * 0.5f;

    const float pad = size * 0.45f;
    const float bg[4]     = {0.05f, 0.05f, 0.06f, 0.72f * alpha};
    const float shadow[4] = {0.00f, 0.00f, 0.00f, 0.85f * alpha};
    const float ink[4]    = {1.00f, 0.86f, 0.55f, 1.00f * alpha};

    gfx_2d_rect(x - pad, y - pad * 0.5f, tw + pad * 2.0f, size + pad, bg);
    gfx_2d_text(text.c_str(), x + 1.0f, y + 1.0f, size, shadow);
    gfx_2d_text(text.c_str(), x, y, size, ink);
}

void Chat::add(const std::string &text, double now)
{
    lines.push_back(ChatLine{lang::tr(text), now});
    if (lines.size() > kChatKeep) lines.erase(lines.begin());
}

void draw_chat(const Chat &c, double now, int screen_w, int screen_h)
{
    const float shorter = (float)(screen_w < screen_h ? screen_w : screen_h);
    const float size    = shorter * 0.024f;
    const float row     = size * 1.25f;
    const float pad     = size * 0.3f;
    const float x       = size * 0.5f;

    // Left of the hotbar and the bars over it, as draw_hotbar lays them out;
    // on a screen too narrow for that, over them.
    const float side  = (float)screen_h * kSlotOfHeight;
    const float bar   = (float)Inventory::kHotbar * side * (1.0f + kPadOfSlot);
    const float room  = ((float)screen_w - bar) * 0.5f - x - pad * 3.0f;
    const float width = std::min(size * 32.0f, room >= size * 12.0f ? room : (float)screen_w - x * 2.0f);
    float       y       = (float)screen_h - size * 2.0f;   // top of the input row

    if (c.open) {
        const std::string shown = c.line + "_";
        const float bg[4]  = {0.0f, 0.0f, 0.0f, 0.55f};
        const float ink[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        gfx_2d_rect(x - pad, y - pad * 0.5f, (float)screen_w - x * 2.0f + pad * 2.0f, row, bg);
        // The end of a long line, which is where the typing is.
        const char *from = shown.c_str();
        while (*from && gfx_2d_text_width(from, size) > (float)screen_w - x * 2.0f)
            while (((uint8_t)*++from & 0xC0) == 0x80) {}   // a whole character at a time
        gfx_2d_text(from, x, y, size, ink);
    }
    y -= row * 1.5f;

    // Wrapped at spaces into rows no wider than `width`, newest at the bottom.
    int rows_left = c.open ? kChatShown * 2 : kChatShown;
    for (auto it = c.lines.rbegin(); it != c.lines.rend() && rows_left > 0; ++it) {
        const double age = now - it->at;
        float alpha      = 1.0f;
        if (!c.open) {
            if (age >= kChatHold + kChatFade) break;   // older ones are older still
            if (age > kChatHold) alpha = (float)(1.0 - (age - kChatHold) / kChatFade);
        }

        std::vector<std::string> rows;
        std::string              cur;
        size_t                   i = 0;
        const std::string       &t = it->text;
        while (i < t.size()) {
            size_t word_end = t.find(' ', i);
            if (word_end == std::string::npos) word_end = t.size();
            const std::string word = t.substr(i, word_end - i);
            const std::string next = cur.empty() ? word : cur + " " + word;
            if (!cur.empty() && gfx_2d_text_width(next.c_str(), size) > width) {
                rows.push_back(cur);
                cur = word;
            } else {
                cur = next;
            }
            i = word_end + 1;
        }
        rows.push_back(cur);

        const float bg[4]     = {0.0f, 0.0f, 0.0f, 0.45f * alpha};
        const float shadow[4] = {0.0f, 0.0f, 0.0f, 0.9f * alpha};
        const float ink[4]    = {1.0f, 1.0f, 1.0f, alpha};
        for (auto r = rows.rbegin(); r != rows.rend() && rows_left > 0; ++r, --rows_left) {
            gfx_2d_rect(x - pad, y - pad * 0.5f, width + pad * 2.0f, row, bg);
            gfx_2d_text(r->c_str(), x + 1.0f, y + 1.0f, size, shadow);
            gfx_2d_text(r->c_str(), x, y, size, ink);
            y -= row;
        }
    }
}

void draw_dig_progress(float t, int screen_w, int screen_h)
{
    if (t <= 0.0f) return;
    if (t > 1.0f) t = 1.0f;

    const float shorter = (float)(screen_w < screen_h ? screen_w : screen_h);
    const float w       = shorter * 0.16f;
    const float h       = shorter * 0.012f;
    const float x       = ((float)screen_w - w) * 0.5f;
    const float y       = (float)screen_h * 0.5f + shorter * 0.045f;

    const float back[4] = {0.05f, 0.05f, 0.06f, 0.65f};
    const float edge[4] = {0.00f, 0.00f, 0.00f, 0.80f};
    const float fill[4] = {0.95f, 0.90f, 0.70f, 0.95f};

    gfx_2d_rect(x - 2.0f, y - 2.0f, w + 4.0f, h + 4.0f, edge);
    gfx_2d_rect(x, y, w, h, back);
    gfx_2d_rect(x, y, w * t, h, fill);
}

void draw_crosshair(int screen_w, int screen_h)
{
    const float shorter = (float)(screen_w < screen_h ? screen_w : screen_h);
    const float arm     = std::max(5.0f, std::floor(shorter * 0.012f));
    const float thick   = 2.0f;
    const float cx      = std::floor((float)screen_w * 0.5f);
    const float cy      = std::floor((float)screen_h * 0.5f);

    // Dark under light, so it reads on snow and on night sky alike.
    const float edge[4] = {0.0f, 0.0f, 0.0f, 0.55f};
    const float fill[4] = {1.0f, 1.0f, 1.0f, 0.85f};

    gfx_2d_rect(cx - arm - 1.0f, cy - thick * 0.5f - 1.0f, arm * 2.0f + 2.0f, thick + 2.0f, edge);
    gfx_2d_rect(cx - thick * 0.5f - 1.0f, cy - arm - 1.0f, thick + 2.0f, arm * 2.0f + 2.0f, edge);
    gfx_2d_rect(cx - arm, cy - thick * 0.5f, arm * 2.0f, thick, fill);
    gfx_2d_rect(cx - thick * 0.5f, cy - arm, thick, arm * 2.0f, fill);
}

std::string name_under(const PanelLayout &at, const ItemStack *slots, int count, float mx,
                       float my, const ItemRegistry &items)
{
    const int i = at.slot_at(mx, my);
    if (i < 0 || i >= count || slots == nullptr) return std::string();

    // Belt and braces: kNoItem's own name is empty, so an empty square would
    // come back as an empty string through the line below anyway. Said out
    // loud because a sabotage that deleted this line stayed GREEN, and the
    // reason is that the two are the same answer -- not that nothing was
    // watching.
    if (slots[i].empty()) return std::string();

    // Shown as the creative panel shows it: "Iron pick", not "tools:iron_pick".
    return item_display_name(items.get(slots[i].item).name);
}

void draw_item_tooltip(const std::string &said, float mx, float my, int screen_w, int screen_h)
{
    if (said.empty()) return;
    const std::string text = lang::tr(said);

    const float shorter = (float)(screen_w < screen_h ? screen_w : screen_h);
    const float size    = (float)screen_h * 0.026f;
    const float tw      = gfx_2d_text_width(text.c_str(), size);
    const float pad     = size * 0.40f;
    const float w       = tw + pad * 2.0f;
    const float h       = size + pad * 1.6f;

    // Up and to the right of the cursor, which is where the hand is not: a box
    // drawn under the pointer covers the square being asked about.
    float x = mx + shorter * 0.012f;
    float y = my - h - shorter * 0.008f;

    // ...and back onto the screen at the edges, because the far corner of a
    // panel is a real place to put the cursor.
    if (x + w > (float)screen_w) x = mx - w - shorter * 0.012f;
    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = my + shorter * 0.012f;
    if (y + h > (float)screen_h) y = (float)screen_h - h;

    const float bg[4]     = {0.05f, 0.05f, 0.06f, 0.88f};
    const float edge[4]   = {0.00f, 0.00f, 0.00f, 0.85f};
    const float shadow[4] = {0.00f, 0.00f, 0.00f, 0.85f};
    const float ink[4]    = {0.96f, 0.94f, 0.88f, 1.00f};

    gfx_2d_rect(x, y, w, h, bg);
    frame(x, y, w, h, size * 0.10f, edge);
    gfx_2d_text(text.c_str(), x + pad + 1.0f, y + pad * 0.8f + 1.0f, size, shadow);
    gfx_2d_text(text.c_str(), x + pad, y + pad * 0.8f, size, ink);
}

// The figure the worn squares sit on: a backdrop the grid's height, and a
// body in flat blocks behind the squares -- head, torso, arms, legs and feet --
// whose edges show round them, so a square reads as the part it is on. With
// arms open, the pack's arms rise from the shoulders to their squares.
static void draw_figure(const PanelLayout &at, float h, float edge)
{
    static constexpr float kBody[4]  = {0.30f, 0.32f, 0.37f, 0.95f};
    static constexpr float kShade[4] = {0.22f, 0.23f, 0.27f, 0.95f};
    static constexpr float kRod[4]   = {0.55f, 0.58f, 0.62f, 1.00f};
    static constexpr float kJoint[4] = {1.00f, 0.55f, 0.05f, 1.00f};

    const float u   = at.slot;
    const float row = at.slot + at.pad;
    const float x0  = at.figure;
    const float mid = x0 + 2.0f * u;
    const float y0  = at.y;
    gfx_2d_rect(x0 - edge, y0 - edge, 4.0f * u + edge * 2.0f, h + edge * 2.0f, kBackdrop);

    const auto box = [&](float cx, float top, float w, float hh, const float *c) {
        gfx_2d_rect(cx - w * 0.5f, top, w, hh, c);
    };
    // Rows: 0 the head, 1 the chest, 2 the legs, 3 the feet.
    box(mid, y0 - u * 0.06f, u * 1.12f, u * 1.04f, kBody);                       // head
    box(mid, y0 + row - u * 0.12f, u * 0.40f, u * 0.14f, kShade);               // neck
    box(mid, y0 + row - u * 0.02f, u * 1.50f, u * 1.32f, kBody);                // torso
    for (float s : {-1.0f, 1.0f}) {
        box(mid + s * u * 0.88f, y0 + row, u * 0.26f, row + u * 0.60f, kBody);  // arm
        box(mid + s * u * 0.88f, y0 + row * 2.0f + u * 0.62f, u * 0.32f, u * 0.24f, kShade);   // hand
        box(mid + s * u * 0.27f, y0 + row * 2.0f, u * 0.46f, row + u * 0.80f, kBody);   // leg
        box(mid + s * u * 0.30f, y0 + row * 3.0f + u * 0.72f, u * 0.56f, u * 0.30f, kShade);   // foot
    }
    if (at.arms <= 0) return;

    // The pack's arms: up from each shoulder, then out to the claw's square,
    // a joint at the elbow. Arm k's square is slot_rect's, whichever side.
    const float t = u * 0.10f;
    for (int k = 0; k < at.arms; ++k) {
        float sx, sy, sw, sh;
        at.slot_rect(at.worn_from + Inventory::kWorn + k, sx, sy, sw, sh);
        const float cx    = sx + sw * 0.5f;
        const float side  = cx > mid ? 1.0f : -1.0f;
        const float sh_x  = mid + side * u * 0.60f;           // the shoulder
        const float sh_y  = y0 + row;
        const float elb_y = sy + sh * 0.5f;                   // level with the claw
        gfx_2d_rect(sh_x - t * 0.5f, elb_y, t, sh_y - elb_y, kRod);
        const float from = side > 0 ? sh_x : sx + sw;
        const float to   = side > 0 ? sx : sh_x;
        gfx_2d_rect(from, elb_y - t * 0.5f, to - from, t, kRod);
        gfx_2d_rect(sh_x - t, elb_y - t, t * 2.0f, t * 2.0f, kJoint);
        gfx_2d_rect(sh_x - t, sh_y - t * 0.5f, t * 2.0f, t * 1.5f, kJoint);
    }
}

PanelLayout draw_bag(const ItemStack *slots, int count, int hand, int dragged,
                     const ItemRegistry &items, const NodeRegistry &nodes,
                     const IconSource &icons, int screen_w, int screen_h)
{
    PanelLayout out;

    if (slots == nullptr || count <= 0 || screen_w <= 0 || screen_h <= 0) return out;

    // Nine across, because the hotbar is nine and the bottom row of the panel
    // IS the hotbar -- a bag whose rows do not line up with the bar is a bag
    // where the slot a player just clicked is not the slot they are about to
    // hold.
    // What is worn is not part of the grid: past the 36, a column of its own.
    const int grid = count > Inventory::kDefaultSlots ? Inventory::kDefaultSlots : count;
    if (grid < count) out.worn_from = grid;
    const int back = grid + kWornBack - 1;
    if (out.worn_from >= 0 && back < count && !slots[back].empty())
        out.arms = std::min<int>(items.get(slots[back].item).arms, Inventory::kArms);

    out.cols  = Inventory::kHotbar;
    out.slots = count;
    out.rows  = (grid + out.cols - 1) / out.cols;
    out.slot  = (float)screen_h * kPanelSlotOfHeight;
    out.pad   = out.slot * kPadOfSlot;
    if (out.rows > 1) out.hotbar_gap = out.pad * 3.0f;

    const float w = (float)out.cols * out.slot + (float)(out.cols - 1) * out.pad;
    const float h = (float)out.rows * out.slot + (float)(out.rows - 1) * out.pad + out.hotbar_gap;

    // The figure is four slots wide, a slot's gap left of the grid, and the
    // two are centred together.
    const float fig_w = out.worn_from >= 0 ? out.slot * 4.0f + out.slot * 0.6f : 0.0f;
    out.x      = ((float)screen_w - w - fig_w) * 0.5f + fig_w;
    out.y      = ((float)screen_h - h) * 0.5f;
    out.figure = out.x - fig_w;

    // A sheet over the whole screen first. Not decoration: it is what says the
    // world behind is not taking clicks any more, and without it a player
    // aiming at a block through an open bag is a player who is about to be
    // surprised.
    const float dim[4] = {0.0f, 0.0f, 0.0f, 0.55f};
    gfx_2d_rect(0.0f, 0.0f, (float)screen_w, (float)screen_h, dim);

    const float edge = out.pad * 2.0f;
    gfx_2d_rect(out.x - edge, out.y - edge, w + edge * 2.0f, h + edge * 2.0f, kBackdrop);
    if (out.worn_from >= 0) draw_figure(out, h, edge);

    for (int i = 0; i < count; ++i) {
        float sx, sy, sw, sh;
        out.slot_rect(i, sx, sy, sw, sh);
        if (sw <= 0.0f) continue;   // an arm the back does not open

        // Two marks, because they are two different facts: the hand is where
        // the next block comes from, and the drag mark is where the stack under
        // the cursor came from. A player who cannot tell them apart cannot tell
        // what is about to happen.
        const float *mark = (i == dragged) ? kPicked : (i == hand ? kSelected : kBorder);

        // The square a drag lifted from is drawn empty: the stack is under the
        // cursor and it is one stack. The square itself stays -- its fill and
        // its mark -- because a hole in the grid would look like the bag lost a
        // slot rather than that a stack is in flight.
        const ItemStack empty;
        draw_slot(sx, sy, sw, sh, i == dragged ? empty : slots[i], items, nodes, icons, mark);
    }

    return out;
}

PanelLayout draw_chest(const ItemStack *slots, int count, int dragged,
                       const ItemRegistry &items, const NodeRegistry &nodes,
                       const IconSource &icons, const PanelLayout &bag, const uint8_t *roles,
                       float progress, float scroll, float scroll_size, const char *search,
                       bool typing)
{
    // Raw, fuel, product (SlotRole 1..3); a Store square is left as it is.
    static constexpr float kRole[4][4] = {{0, 0, 0, 0},
                                          {0.35f, 0.62f, 0.95f, 1.0f},
                                          {0.95f, 0.55f, 0.18f, 1.0f},
                                          {0.40f, 0.85f, 0.35f, 1.0f}};
    static const char *const kRoleName[4] = {"", "raw", "fuel", "product"};
    PanelLayout out;

    if (slots == nullptr || count <= 0 || bag.slot <= 0.0f) return out;

    out.cols  = Inventory::kHotbar;
    out.slots = count;
    if (roles != nullptr && roles[count - 1] == kRoleTrash) out.loose = count - 1;
    const int grid = out.loose >= 0 ? count - 1 : count;
    out.rows  = (grid + out.cols - 1) / out.cols;
    out.slot  = bag.slot;
    out.pad   = bag.pad;

    // Smaller squares when it does not fit above the bag: a double chest is
    // six rows, and at the bag's size its top three were off the screen. The
    // edge of the backdrop and the gap below are counted, so it ends on the
    // screen's top edge at the worst.
    const float room = bag.y - out.pad * 2.0f;
    const float need = (float)out.rows * out.slot + (float)(out.rows - 1) * out.pad + out.slot;
    if (need > room && room > 0.0f) {
        out.slot *= room / need;
        out.pad *= room / need;
    }

    const float h = (float)out.rows * out.slot + (float)(out.rows - 1) * out.pad;
    const float w = (float)out.cols * out.slot + (float)(out.cols - 1) * out.pad;

    // Aligned with the bag rather than centred on the screen: the two grids
    // are nine columns each, so aligning them makes the columns line up, and a
    // chest of four columns centred over a bag of nine would not. Centred over
    // it when its squares had to be made smaller, where no column lines up.
    const float bag_w = (float)bag.cols * bag.slot + (float)(bag.cols - 1) * bag.pad;
    out.x = bag.x + (bag_w - w) * 0.5f;

    // A gap of one slot between the two, so a drag that ends between them ends
    // on neither -- which is Nothing, and Nothing is better than landing on
    // whichever grid happened to be nearer.
    out.y = bag.y - h - out.slot;

    const float edge = out.pad * 2.0f;
    const float bar  = scroll_size > 0.0f ? out.slot * 0.3f + out.pad * 2.0f : 0.0f;
    gfx_2d_rect(out.x - edge, out.y - edge, w + bar + edge * 2.0f, h + edge * 2.0f, kBackdrop);

    // Where in the list the rows on show are: a trough right of the grid and a
    // thumb as long as the share on show, never shorter than half a square.
    if (scroll_size > 0.0f) {
        constexpr float kTrough[4] = {0.10f, 0.10f, 0.12f, 0.95f};
        const float tx = out.x + w + out.pad * 2.0f;
        const float tw = out.slot * 0.3f;
        const float th = std::max(h * scroll_size, out.slot * 0.5f);
        gfx_2d_rect(tx, out.y, tw, h, kTrough);
        gfx_2d_rect(tx, out.y + std::min(h * scroll, h - th), tw, th, kBorder);
    }

    // The search box, left of the top row: what is typed, or a grey word saying
    // what the box is for; the tail of a line too long for it.
    if (search != nullptr) {
        constexpr float kHint[4] = {0.55f, 0.55f, 0.58f, 1.0f};
        out.field_w = out.slot * 4.0f;
        out.field_h = out.slot * 0.7f;
        out.field_x = out.x - edge - out.slot * 0.5f - out.field_w;
        out.field_y = out.y;
        gfx_2d_rect(out.field_x - edge, out.field_y - edge, out.field_w + edge * 2.0f,
                    out.field_h + edge * 2.0f, kBackdrop);
        gfx_2d_rect(out.field_x, out.field_y, out.field_w, out.field_h, kSlotFill);
        frame(out.field_x, out.field_y, out.field_w, out.field_h, out.slot * 0.05f,
              typing ? kSelected : kBorder);

        const float ts  = out.field_h * 0.55f;
        const float tx  = out.field_x + ts * 0.4f;
        const float ty  = out.field_y + (out.field_h - ts) * 0.5f;
        std::string line = std::string(search) + (typing ? "_" : "");
        while (!line.empty() && gfx_2d_text_width(line.c_str(), ts) > out.field_w - ts * 0.8f)
            line.erase(0, 1);
        if (line.empty() && !typing) gfx_2d_text(lang::tr("search"), tx, ty, ts, kHint);
        else gfx_2d_text(line.c_str(), tx, ty, ts, kWhite);
    }

    // The bin, outside: right of the backdrop, level with the bottom row.
    if (out.loose >= 0) {
        out.loose_x = out.x + w + bar + edge + out.slot * 0.5f;
        out.loose_y = out.y + h - out.slot;
        gfx_2d_rect(out.loose_x - edge, out.loose_y - edge, out.slot + edge * 2.0f,
                    out.slot + edge * 2.0f, kBackdrop);
    }

    for (int i = 0; i < count; ++i) {
        float sx, sy, sw, sh;
        out.slot_rect(i, sx, sy, sw, sh);

        // No hand mark here: the hotbar is the bag's and a chest slot is never
        // the one a block gets placed from.
        const float *mark = (i == dragged) ? kPicked : kBorder;

        const ItemStack empty;
        draw_slot(sx, sy, sw, sh, i == dragged ? empty : slots[i], items, nodes, icons, mark);
        if (roles != nullptr && roles[i] > 0 && roles[i] < 4)
            gfx_2d_rect(sx + sw * 0.08f, sy + sh * 0.86f, sw * 0.84f, sh * 0.08f, kRole[roles[i]]);
        // A bin: a red cross on the square, which is what it does.
        if (roles != nullptr && roles[i] == kRoleTrash) {
            constexpr float kBin[4]  = {0.90f, 0.25f, 0.22f, 1.0f};
            constexpr float kBack[4] = {0.45f, 0.08f, 0.08f, 0.55f};
            gfx_2d_rect(sx + sw * 0.06f, sy + sh * 0.06f, sw * 0.88f, sh * 0.88f, kBack);
            const float ts = sh * 0.5f;
            const float tw = gfx_2d_text_width("X", ts);
            gfx_2d_text("X", sx + (sw - tw) * 0.5f, sy + (sh - ts) * 0.5f, ts, kBin);
        }
    }

    // What the colours mean, over the grid: only the roles it has.
    if (roles != nullptr) {
        bool has[4] = {};
        for (int i = 0; i < count; ++i)
            if (roles[i] < 4) has[roles[i]] = true;
        const float size = out.slot * 0.28f;
        float       tx   = out.x;
        const float ty   = out.y - edge - size * 1.4f;
        for (int r = 1; r < 4; ++r) {
            if (!has[r]) continue;
            gfx_2d_rect(tx, ty + size * 0.3f, size * 0.8f, size * 0.5f, kRole[r]);
            tx += size * 1.1f;
            gfx_2d_text(kRoleName[r], tx + 1.0f, ty + 1.0f, size, kShadow);
            gfx_2d_text(kRoleName[r], tx, ty, size, kWhite);
            tx += gfx_2d_text_width(kRoleName[r], size) + size;
        }
    }

    // How far along it is: a bar in the gap under the squares, filling from
    // the left, the product's green when it is full.
    if (progress >= 0.0f) {
        constexpr float kTrack[4] = {0.10f, 0.10f, 0.12f, 0.95f};
        constexpr float kFill[4]  = {0.95f, 0.62f, 0.18f, 1.00f};
        constexpr float kDone[4]  = {0.40f, 0.85f, 0.35f, 1.00f};
        const float bh = out.slot * 0.22f;
        const float by = out.y + h + edge + out.slot * 0.2f;
        gfx_2d_rect(out.x - 1.0f, by - 1.0f, w + 2.0f, bh + 2.0f, kBorder);
        gfx_2d_rect(out.x, by, w, bh, kTrack);
        const float p = progress > 1.0f ? 1.0f : progress;
        gfx_2d_rect(out.x, by, w * p, bh, p >= 1.0f ? kDone : kFill);
        char pct[8];
        std::snprintf(pct, sizeof pct, "%d%%", (int)(p * 100.0f + 0.5f));
        const float ts = bh * 0.9f;
        const float tw = gfx_2d_text_width(pct, ts);
        gfx_2d_text(pct, out.x + (w - tw) * 0.5f + 1.0f, by + (bh - ts) * 0.5f + 1.0f, ts, kShadow);
        gfx_2d_text(pct, out.x + (w - tw) * 0.5f, by + (bh - ts) * 0.5f, ts, kWhite);
    }

    return out;
}

CraftLayout draw_craft(const ItemStack *slots, int w, int h, const ItemStack &result, int dragged,
                       const ItemRegistry &items, const NodeRegistry &nodes,
                       const IconSource &icons, const PanelLayout &bag)
{
    CraftLayout out;

    if (slots == nullptr || w <= 0 || h <= 0 || bag.slot <= 0.0f) return out;

    out.grid.cols  = w;
    out.grid.rows  = h;
    out.grid.slots = w * h;
    out.grid.slot  = bag.slot;
    out.grid.pad   = bag.pad;

    const float gh = (float)h * out.grid.slot + (float)(h - 1) * out.grid.pad;
    const float gw = (float)w * out.grid.slot + (float)(w - 1) * out.grid.pad;

    out.grid.x = bag.x;

    // The same gap above the bag a chest leaves, so that a drag which ends
    // between the two ends on neither -- Nothing is better than landing on
    // whichever grid happened to be nearer.
    out.grid.y = bag.y - gh - out.grid.slot;

    // The result to the right, one slot's width away and level with the middle
    // of the grid. Its own layout of one square: it is not a square OF the
    // grid, and a hit test that could return it as "slot four of a two-by-two"
    // would be a hit test whose answer means two things.
    out.result.cols  = 1;
    out.result.rows  = 1;
    out.result.slots = 1;
    out.result.slot  = out.grid.slot;
    out.result.pad   = out.grid.pad;
    out.result.x     = out.grid.x + gw + out.grid.slot * 1.5f;
    out.result.y     = out.grid.y + (gh - out.grid.slot) * 0.5f;

    const float edge  = out.grid.pad * 2.0f;
    const float right = out.result.x + out.result.slot;
    gfx_2d_rect(out.grid.x - edge, out.grid.y - edge, right - out.grid.x + edge * 2.0f,
                gh + edge * 2.0f, kBackdrop);

    for (int i = 0; i < out.grid.slots; ++i) {
        float sx, sy, sw, sh;
        out.grid.slot_rect(i, sx, sy, sw, sh);

        const float      *mark = (i == dragged) ? kPicked : kBorder;
        const ItemStack   empty;
        draw_slot(sx, sy, sw, sh, i == dragged ? empty : slots[i], items, nodes, icons, mark);
    }

    // An arrow between the two, so the square on the right reads as an OUTCOME
    // and not as a fifth place to put something. Two rectangles: a shaft and a
    // head made of a shorter, taller one, because there is no triangle in
    // gfx_2d_* and a picture is not worth a primitive.
    const float ax = out.grid.x + gw + out.grid.slot * 0.35f;
    const float ay = out.result.y + out.result.slot * 0.5f;
    gfx_2d_rect(ax, ay - out.grid.slot * 0.04f, out.grid.slot * 0.55f, out.grid.slot * 0.08f,
                kBorder);
    gfx_2d_rect(ax + out.grid.slot * 0.40f, ay - out.grid.slot * 0.12f, out.grid.slot * 0.15f,
                out.grid.slot * 0.24f, kBorder);

    {
        float sx, sy, sw, sh;
        out.result.slot_rect(0, sx, sy, sw, sh);
        draw_slot(sx, sy, sw, sh, result, items, nodes, icons,
                  result.empty() ? kBorder : kSelected);
    }

    return out;
}

void draw_drag_ghost(const ItemStack &stack, float cx, float cy, float side,
                     const ItemRegistry &items, const NodeRegistry &nodes,
                     const IconSource &icons, bool picked)
{
    if (stack.empty() || side <= 0.0f) return;

    // Centred on the cursor rather than hanging off it: the point of the drag
    // is that the cursor IS the stack, and a stack drawn to one side makes a
    // player aim with a corner they cannot see.
    const float sx = cx - side * 0.5f;
    const float sy = cy - side * 0.5f;

    // No border colour of its own -- kPicked, the same mark a click leaves --
    // because a drag and a pick are the same choice made two ways, and giving
    // them two looks would be inventing a third state.
    draw_slot(sx, sy, side, side, stack, items, nodes, icons, picked ? kPicked : kBorder);
}

BagAction bag_click(BagPoint press, BagPoint release, bool press_has_item)
{
    BagAction a;

    // A press that started off the panel is not the bag's, however it ends.
    if (press.slot < 0) return a;

    // Let go outside the panel and the stack leaves the bag. The world is where
    // it goes, and the caller is what knows how to put it there -- this decides
    // WHAT the gesture was, not what happens next.
    if (release.slot < 0) {
        if (!press_has_item) return a;

        // Only out of the bag. A stack dragged out of a CHEST and dropped on
        // the floor would be a player emptying somebody else's chest onto the
        // ground with one gesture, and the chest may not even be theirs. Taking
        // it into the bag first is one extra drag and makes it their stack
        // before it is anybody's problem. The same for a craft grid, where the
        // answer is also "take it into the bag first", and for the result
        // square, where there is nothing to take until it has been paid for.
        if (press.grid != Grid::Bag) return a;

        a.kind = BagAction::Drop;
        a.from = press.slot;
        return a;
    }

    // Down and up on the same square is a click, and a click moves nothing. It
    // is not "a drag of length zero that happens to be refused": it is the
    // gesture that used to arm a two-click move, and it is deliberately inert
    // now, so that a player who half-remembers the old way gets nothing rather
    // than something they did not mean.
    //
    // The same square means the same GRID too: slot 3 of the chest and slot 3
    // of the bag are two squares that happen to share a number.
    if (press.slot == release.slot && press.grid == release.grid) return a;

    // Nothing goes INTO the result square. It is not a slot, it is an answer,
    // and a gesture that ended on it is a gesture that ended nowhere.
    if (release.grid == Grid::Result) return a;

    // A drag carries what it STARTED on. An empty square carries nothing, and
    // it must not fall back to some other square: a drag that quietly moved
    // something the player was not touching is how a stack goes missing.
    if (!press_has_item) return a;

    a.kind      = BagAction::Move;
    a.from      = press.slot;
    a.to        = release.slot;
    a.from_grid = press.grid;
    a.to_grid   = release.grid;
    return a;
}

BagAction bag_tap(BagPoint press, BagPoint at, bool press_has_item)
{
    BagAction a;

    // No drag in progress is nothing to put down. The right button on its own
    // is not a gesture here: with no stack in the cursor there is nothing for
    // it to pick up, and a button that did something different depending on
    // whether the other one was down would be two gestures on one key.
    if (press.slot < 0 || !press_has_item) return a;

    // Off the panel is not a square. Dropping one item into the world is the
    // left button's business, and only at the END of a drag -- a right button
    // that also threw things would let a stack leave the bag one item at a
    // time by accident, over the world the player cannot see behind the panel.
    if (at.slot < 0) return a;

    // Out of the result square, never. An answer is paid for out of the grid
    // all at once or not at all -- see craft_take -- so there is no single item
    // of it to put anywhere.
    if (press.grid == Grid::Result) return a;

    // And nothing goes INTO the result square, which is the same rule
    // bag_click states: it is not a slot, it is an answer.
    if (at.grid == Grid::Result) return a;

    // Onto the square it came from is a no-op spelled the long way round.
    if (press.slot == at.slot && press.grid == at.grid) return a;

    a.kind      = BagAction::Move;
    a.from      = press.slot;
    a.to        = at.slot;
    a.from_grid = press.grid;
    a.to_grid   = at.grid;
    a.count     = 1;
    return a;
}

BagAction bag_shift_click(BagPoint press, BagPoint release, bool press_has_item)
{
    BagAction a;

    // A press that began off the panel is not the bag's, however it ends, and
    // an empty square has nothing to send anywhere.
    if (press.slot < 0 || !press_has_item) return a;

    // Down and up on the SAME square, in the same grid. Anything else is a
    // drag, and a drag means what bag_click says it means.
    if (release.slot != press.slot || release.grid != press.grid) return a;

    a.kind      = BagAction::Quick;
    a.from      = press.slot;
    a.from_grid = press.grid;
    return a;
}

void HealthLayout::pip_rect(int i, float &out_x, float &out_y, float &out_w, float &out_h) const
{
    if (i < 0 || i >= pips) {
        out_x = out_y = out_w = out_h = 0.0f;
        return;
    }

    const float gap  = h * 0.25f;
    const float step = (w + gap) / (float)pips;

    out_x = x + (float)i * step;
    out_y = y;
    out_w = step - gap;
    out_h = h;
}

HealthLayout draw_health(int health, int max, int screen_w, int screen_h)
{
    HealthLayout out;

    // Nobody has said yet. Not the same as empty, and drawing it as empty would
    // tell everybody who just joined that they are about to die.
    if (max <= 0 || screen_w <= 0 || screen_h <= 0) return out;

    if (health < 0) health = 0;
    if (health > max) health = max;

    const float side = (float)screen_h * kSlotOfHeight;
    const float pad  = side * kPadOfSlot;

    // One segment per two points, which is the ten hearts a player of this kind
    // of game already counts in. An odd maximum gets a last segment worth one,
    // and it fills at half -- rounding it away would lose a point off the end of
    // the bar for every odd maximum a mod ever sets.
    out.pips = (max + 1) / 2;
    out.w    = (float)Inventory::kHotbar * side + (float)(Inventory::kHotbar - 1) * pad;
    out.h    = side * 0.22f;
    out.x    = ((float)screen_w - out.w) * 0.5f;
    out.y    = (float)screen_h - side - (float)screen_h * kBottomMargin - pad * 2.0f - out.h;

    gfx_2d_rect(out.x - pad, out.y - pad, out.w + pad * 2.0f, out.h + pad * 2.0f, kBackdrop);

    for (int i = 0; i < out.pips; ++i) {
        float px, py, wide, tall;
        out.pip_rect(i, px, py, wide, tall);

        // How much of THIS segment is filled: the two points it stands for,
        // minus what is missing. Worked out per segment rather than by
        // comparing i against a half-count, so that a half-filled one is drawn
        // half filled instead of rounded to one side.
        const int left = health - i * 2;
        const float fill = left <= 0 ? 0.0f : (left >= 2 ? 1.0f : 0.5f);

        gfx_2d_rect(px, py, wide, tall, kEmptyHeart);
        if (fill > 0.0f) gfx_2d_rect(px, py, wide * fill, tall, kHeart);
    }

    return out;
}

void draw_armor(int armor, const HealthLayout &health)
{
    if (armor <= 0 || health.pips <= 0) return;
    if (armor > 20) armor = 20;

    // The health bar's own segments, one bar higher: ten of two points.
    const float pad = health.h * 0.5f;
    HealthLayout at = health;
    at.pips         = 10;
    at.y            = health.y - health.h - pad * 2.0f;
    constexpr float kPlate[4] = {0.72f, 0.78f, 0.86f, 1.0f};
    constexpr float kNone[4]  = {0.18f, 0.20f, 0.24f, 1.0f};

    gfx_2d_rect(at.x - pad, at.y - pad, at.w + pad * 2.0f, at.h + pad * 2.0f, kBackdrop);
    for (int i = 0; i < at.pips; ++i) {
        float px, py, wide, tall;
        at.pip_rect(i, px, py, wide, tall);
        const int   left = armor - i * 2;
        const float fill = left <= 0 ? 0.0f : (left >= 2 ? 1.0f : 0.5f);
        gfx_2d_rect(px, py, wide, tall, kNone);
        if (fill > 0.0f) gfx_2d_rect(px, py, wide * fill, tall, kPlate);
    }
}

uint16_t half_of(uint16_t count) { return (uint16_t)((count + 1) / 2); }

BagAction bag_half(BagPoint press, BagPoint release, uint16_t press_count)
{
    BagAction a;

    if (press.slot < 0 || press_count == 0) return a;

    // Off the panel is not a square. Throwing half a stack on the floor is a
    // gesture nobody asked for, and the left button already means the floor.
    if (release.slot < 0) return a;

    // Out of the answer square, never: a craft is paid for all at once, so
    // there is no half of one to carry -- the same rule bag_tap states.
    if (press.grid == Grid::Result) return a;
    if (release.grid == Grid::Result) return a;

    // Down and up on the same square is a click, and a right click on one
    // square has nowhere to put the half it picked up.
    if (press.slot == release.slot && press.grid == release.grid) return a;

    a.kind      = BagAction::Move;
    a.from      = press.slot;
    a.to        = release.slot;
    a.from_grid = press.grid;
    a.to_grid   = release.grid;

    a.count     = half_of(press_count);
    return a;
}

bool half_drag_starts(Grid g) { return g != Grid::Result; }

CursorWish panel_cursor(bool panel_up, bool bag_open, bool grabbed, bool took)
{
    CursorWish w;
    w.took = took;

    if (panel_up) {
        // A panel takes the mouse, and remembers that it was the one that did.
        if (grabbed) {
            w.change = true;
            w.grab   = false;
            w.took   = true;
        }
        return w;
    }

    // No panel. Only whatever TOOK the cursor gives it back, and only if
    // nothing else is holding it -- a bag open behind the panel keeps the mouse,
    // and grabbing here would take it off the bag mid-drag.
    if (!took) return w;

    w.took = false;
    if (bag_open) return w;

    w.change = true;
    w.grab   = true;
    return w;
}

int hotbar_scroll(int selected, float notches, int count)
{
    if (count <= 0) return selected;

    // Rounded, because a trackpad reports fractions and a tenth of a notch is
    // not a slot. Truncating instead would make a slow drag on a trackpad move
    // nothing at all, however far it went.
    const int steps = (int)(notches < 0.0f ? notches - 0.5f : notches + 0.5f);
    if (steps == 0) return selected < 0 ? 0 : (selected >= count ? count - 1 : selected);

    // Away from the player goes left. A modulo that can go negative is the one
    // thing to get right here: -1 % 9 is -1 in C, and a hand on slot -1 is a
    // hand on nothing.
    int at = selected - steps;
    at %= count;
    if (at < 0) at += count;
    return at;
}

std::string item_display_name(const std::string &name)
{
    const size_t colon = name.find(':');
    std::string  out   = colon == std::string::npos ? name : name.substr(colon + 1);
    for (char &c : out)
        if (c == '_') c = ' ';
    if (!out.empty() && out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 'a' + 'A');
    return out;
}

namespace {

// Words laid out by somebody who measured them in English (a server's page,
// src/core/ui/page.h), translated here: a translation wider than `room` is
// drawn smaller, down to two thirds, and past that in the English it was
// measured in, which fits by construction.
const char *fit_text(const std::string &words, float room, float &size)
{
    const char *text = lang::tr(words);
    const float tw   = gfx_2d_text_width(text, size);
    if (tw <= room || text == words.c_str() || room <= 0.0f) return text;
    const float smaller = size * room / tw;
    if (smaller >= size * (2.0f / 3.0f)) {
        size = smaller;
        return text;
    }
    return words.c_str();
}

} // namespace

void draw_mod_panel(const Panel &p, const PanelPlacement &at, int hover, int screen_w,
                    int screen_h, int typing, const char *typed, const ItemRegistry *items,
                    const NodeRegistry *nodes, const IconSource *icons)
{
    if (at.cell <= 0.0f || screen_w <= 0 || screen_h <= 0) return;

    // The same sheet over the world the bag uses, and for the same reason: it
    // is what says the world behind is not taking clicks any more.
    const float dim[4] = {0.0f, 0.0f, 0.0f, 0.55f};
    gfx_2d_rect(0.0f, 0.0f, (float)screen_w, (float)screen_h, dim);

    const float edge = at.cell * 0.2f;
    gfx_2d_rect(at.x - edge, at.y - edge, p.w * at.cell + edge * 2.0f,
                p.h * at.cell + edge * 2.0f, kBackdrop);

    for (size_t i = 0; i < p.elements.size(); ++i) {
        const Element &e = p.elements[i];

        float ex, ey, ew, eh;
        at.rect_of(e, ex, ey, ew, eh);

        switch (e.kind) {
        case ElemKind::Box: {
            const float rgba[4] = {(float)e.r / 255.0f, (float)e.g / 255.0f,
                                   (float)e.b / 255.0f, 1.0f};
            gfx_2d_rect(ex, ey, ew, eh, rgba);
            break;
        }

        case ElemKind::Item:
            if (items != nullptr && nodes != nullptr && icons != nullptr) {
                ItemStack one;
                one.item  = items->find(e.text);
                one.count = one.item == kNoItem ? 0 : 1;
                draw_slot(ex, ey, ew, eh, one, *items, *nodes, *icons,
                          (int)i == hover ? kSelected : kBorder);
                break;
            }
            [[fallthrough]];

        case ElemKind::Button: {
            gfx_2d_rect(ex, ey, ew, eh, kSlotFill);
            frame(ex, ey, ew, eh, at.cell * 0.06f + 1.0f,
                  (int)i == hover ? kSelected : kBorder);

            // Centred, which needs the text measured rather than guessed: a
            // caption placed by an estimate drifts off its button the moment
            // somebody writes a longer word.
            float       size = at.cell * 0.34f;
            const char *text = fit_text(e.text, ew - at.cell * 0.2f, size);
            const float tw   = gfx_2d_text_width(text, size);
            gfx_2d_text(text, ex + (ew - tw) * 0.5f, ey + (eh - size) * 0.5f, size, kWhite);
            break;
        }

        // Darker than a button and written from the left, as a line to type in
        // is everywhere; the one with the keyboard is outlined and has a caret.
        case ElemKind::Field: {
            const float well[4] = {0.06f, 0.06f, 0.08f, 1.0f};
            gfx_2d_rect(ex, ey, ew, eh, well);
            const bool here = (int)i == typing;
            frame(ex, ey, ew, eh, at.cell * 0.06f + 1.0f,
                  here || (int)i == hover ? kSelected : kBorder);

            const float       size = at.cell * 0.34f;
            const std::string line = here && typed ? std::string(typed) + "_" : e.text;
            gfx_2d_text(line.c_str(), ex + at.cell * 0.15f, ey + (eh - size) * 0.5f, size, kWhite);
            break;
        }

        // Its well; the model turning in it is drawn after the panel, in 3D
        // (draw_panel_models in main.cpp).
        case ElemKind::Model: {
            const float well[4] = {0.17f, 0.19f, 0.24f, 1.0f};
            gfx_2d_rect(ex, ey, ew, eh, well);
            frame(ex, ey, ew, eh, at.cell * 0.06f + 1.0f, kBorder);
            break;
        }

        case ElemKind::Label:
        default: {
            // To the panel's edge: a page's line was cut there in English.
            float       size = at.cell * 0.34f;
            const char *text = fit_text(e.text, at.x + p.w * at.cell - ex, size);
            gfx_2d_text(text, ex, ey, size, kWhite);
            break;
        }
        }
    }
}

} // namespace cnp::ui
