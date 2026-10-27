#include "core/ui/page.h"

#include <algorithm>
#include <sstream>

namespace cnp::ui {

namespace {

constexpr float kMargin = 0.4f;    // the page's edge to its text
constexpr float kTop    = 1.2f;    // under the title bar
constexpr float kFoot   = 1.0f;    // over the screen buttons
constexpr float kText   = 0.55f;   // a line of text
constexpr float kButton = 0.8f;    // a button's height
constexpr float kSpace  = 0.2f;    // between two things on one line

// One thing on a row, with what a click on it does when it is a link.
struct Piece {
    Element     e;
    std::string target;
    bool        field = false;
};

struct Row {
    float              h = kText;
    std::vector<Piece> pieces;
};

// The pixel widths of raylib's default font, codepoints 32 to 255
// (charsWidth in third_party/raylib/src/rtext.c); anything else draws as '?'.
constexpr uint8_t kGlyphW[224] = {
    3, 1, 4, 6, 5, 7, 6, 2, 3, 3, 5, 5, 2, 4, 1, 7, 5, 2, 5, 5, 5, 5, 5, 5, 5, 5, 1, 1, 3, 4, 3, 6,
    7, 6, 6, 6, 6, 6, 6, 6, 6, 3, 5, 6, 5, 7, 6, 6, 6, 6, 6, 6, 7, 6, 7, 7, 6, 6, 6, 2, 7, 2, 3, 5,
    2, 5, 5, 5, 5, 5, 4, 5, 5, 1, 2, 5, 2, 5, 5, 5, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 5, 3, 1, 3, 4, 4,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 5, 5, 5, 7, 1, 5, 3, 7, 3, 5, 4, 1, 7, 4, 3, 5, 3, 3, 2, 5, 6, 1, 2, 2, 3, 5, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 7, 6, 6, 6, 6, 6, 3, 3, 3, 3, 7, 6, 6, 6, 6, 6, 6, 5, 6, 6, 6, 6, 6, 6, 4, 6,
    5, 5, 5, 5, 5, 5, 9, 5, 5, 5, 5, 5, 2, 2, 3, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 5};

// One glyph's pixels, ten to a font of size 10, and its length in bytes.
int glyph(const std::string &s, size_t at, size_t &len)
{
    const unsigned char c = (unsigned char)s[at];
    len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    if (at + len > s.size()) len = s.size() - at;
    uint32_t cp = len == 1 ? c : c & (0x3F >> (len - 1));
    for (size_t i = 1; i < len; ++i) cp = cp << 6 | ((unsigned char)s[at + i] & 0x3F);
    return cp >= 32 && cp < 256 ? kGlyphW[cp - 32] : kGlyphW['?' - 32];
}

// What gfx_2d_text draws: the glyphs scaled from size 10, a tenth of the size
// between two of them.
constexpr float kPx = 0.034f;

float text_w(const std::string &s) { return page_text_w(s); }

// How many bytes of `s` fit in `w`.
size_t fits(const std::string &s, float w)
{
    float  px = -1.0f;
    size_t at = 0;
    for (size_t len; at < s.size(); at += len) {
        const float next = px + (float)glyph(s, at, len) + 1.0f;
        if (next * kPx > w) break;
        px = next;
    }
    return at;
}

std::string fit(const std::string &s, float w)
{
    if (s.size() <= kMaxPanelText && text_w(s) <= w) return s;
    return s.substr(0, std::min(kMaxPanelText - 1, fits(s, w - text_w("~")))) + "~";
}

// A space after a word: its glyph and the gap.
const float kSpaceW = 4.0f * kPx;

Piece label(const std::string &text, float x, float w)
{
    Piece p;
    p.e.kind = ElemKind::Label;
    p.e.text = fit(text, w);
    p.e.x    = x;
    p.e.h    = kText;
    return p;
}

Piece box(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b)
{
    Piece p;
    p.e.kind = ElemKind::Box;
    p.e.x = x, p.e.y = y, p.e.w = w, p.e.h = h;
    p.e.r = r, p.e.g = g, p.e.b = b;
    return p;
}

Piece button(const std::string &text, const std::string &target, float x, float w)
{
    Piece p;
    p.e.kind   = ElemKind::Button;
    p.e.text   = fit(text, w - 0.3f);
    p.e.x      = x;
    p.e.w      = w;
    p.e.h      = kButton;
    p.target   = target;
    return p;
}

// A line cut into its text and its [links](targets).
struct Seg {
    bool        link = false;
    std::string text, target;
};

std::vector<Seg> segments(const std::string &line)
{
    std::vector<Seg> out;
    size_t           at = 0;
    while (at < line.size()) {
        const size_t open = line.find('[', at);
        const size_t mid  = open == std::string::npos ? open : line.find("](", open);
        const size_t end  = mid == std::string::npos ? mid : line.find(')', mid + 2);
        if (end == std::string::npos) {
            out.push_back({false, line.substr(at), ""});
            break;
        }
        if (open > at) out.push_back({false, line.substr(at, open - at), ""});
        out.push_back({true, line.substr(open + 1, mid - open - 1), line.substr(mid + 2, end - mid - 2)});
        at = end + 1;
    }
    return out;
}

std::vector<std::string> words(const std::string &s)
{
    std::vector<std::string> out;
    std::istringstream       in(s);
    for (std::string w; in >> w;) out.push_back(w);
    return out;
}

bool has_button(const Row &r)
{
    for (const Piece &p : r.pieces)
        if (p.e.kind == ElemKind::Button || p.e.kind == ElemKind::Field) return true;
    return false;
}

// A paragraph from x0, `w` wide, wrapped into as many rows as it takes.
void flow(std::vector<Row> &rows, const std::string &line, float x0, float w)
{
    Row         cur;
    float       x = x0;
    std::string run;
    float       run_x = x0;
    const float limit = x0 + w;

    const auto flush_run = [&] {
        if (!run.empty()) cur.pieces.push_back(label(run, run_x, limit - run_x));
        run.clear();
    };
    const auto newline = [&] {
        flush_run();
        cur.h = has_button(cur) ? kButton + 0.15f : kText;
        rows.push_back(std::move(cur));
        cur = Row{};
        x   = x0;
    };

    for (const Seg &s : segments(line)) {
        if (s.link) {
            const float bw = std::min(text_w(s.text) + 0.6f, w);
            if (x + bw > limit && x > x0) newline();
            flush_run();
            cur.pieces.push_back(button(s.text, s.target, x, bw));
            x += bw + kSpace;
            continue;
        }
        for (std::string word : words(s.text)) {
            // A word longer than the line is cut where the line ends.
            while (text_w(word) > w) {
                if (x > x0) newline();
                const size_t most = std::max<size_t>(1, fits(word, w));
                run_x = x;
                run   = word.substr(0, most);
                word  = word.substr(most);
                newline();
            }
            std::string joined = run.empty() ? word : run + " " + word;
            if ((run.empty() ? x : run_x) + text_w(joined) > limit && x > x0) {
                newline();
                joined = word;
            }
            if (run.empty()) run_x = x;
            run = joined;
            x   = run_x + text_w(run) + kSpaceW;
        }
    }
    if (!cur.pieces.empty() || !run.empty()) newline();
}

std::vector<std::string> table_cells(const std::string &line)
{
    std::vector<std::string> cells;
    std::string              cell;
    for (size_t i = line.find('|') + 1; i < line.size(); ++i) {
        if (line[i] != '|') {
            cell += line[i];
            continue;
        }
        cells.push_back(cell);
        cell.clear();
    }
    if (cell.find_first_not_of(' ') != std::string::npos) cells.push_back(cell);

    // |---|---| under a header, as a README writes it: nothing to draw.
    bool rule = true;
    for (const std::string &c : cells)
        if (c.find_first_not_of("-: ") != std::string::npos) rule = false;
    if (rule) cells.clear();
    return cells;
}

std::string trimmed(std::string t)
{
    t.erase(0, t.find_first_not_of(' '));
    while (!t.empty() && t.back() == ' ') t.pop_back();
    return t;
}

// How wide a cell wants to be: its text and its buttons side by side.
float cell_w(const std::string &cell)
{
    float w = 0.0f;
    for (const Seg &s : segments(cell))
        w += s.link ? text_w(s.text) + 0.6f + kSpace * 0.5f
                    : (trimmed(s.text).empty() ? 0.0f : text_w(trimmed(s.text)) + kSpace);
    return w + kSpace;
}

// A run of table lines, its columns as wide as their widest cell wants: what
// is left over is shared out, and a table too wide is squeezed evenly.
void table(std::vector<Row> &rows, const std::vector<std::string> &lines, float x0, float w)
{
    std::vector<std::vector<std::string>> grid;
    std::vector<float>                    want;
    for (const std::string &l : lines) {
        std::vector<std::string> cells = table_cells(l);
        if (cells.empty()) continue;
        if (want.size() < cells.size()) want.resize(cells.size(), 0.0f);
        for (size_t i = 0; i < cells.size(); ++i) want[i] = std::max(want[i], cell_w(cells[i]));
        grid.push_back(std::move(cells));
    }
    float total = 0.0f;
    for (const float f : want) total += f;
    if (total <= 0.0f) return;
    std::vector<float> cw(want.size());
    for (size_t i = 0; i < want.size(); ++i)
        cw[i] = total <= w ? want[i] + (w - total) / (float)want.size() : want[i] * w / total;

    for (const std::vector<std::string> &cells : grid) {
        Row   row;
        float cx = x0;
        for (size_t i = 0; i < cells.size(); ++i) {
            float       x   = cx;
            const float end = cx + cw[i] - kSpace * 0.5f;
            cx += cw[i];
            for (const Seg &s : segments(cells[i])) {
                if (x >= end) break;
                if (s.link) {
                    const float bw = std::min(text_w(s.text) + 0.6f, end - x);
                    row.pieces.push_back(button(s.text, s.target, x, bw));
                    x += bw + kSpace * 0.5f;
                } else if (const std::string t = trimmed(s.text); !t.empty()) {
                    row.pieces.push_back(label(t, x, end - x));
                    x += text_w(t) + kSpace;
                }
            }
        }
        row.h = has_button(row) ? kButton + 0.1f : kText + 0.05f;
        rows.push_back(std::move(row));
    }
}

std::vector<Row> parse(const std::string &markup)
{
    const float      x0 = kMargin, w = kPageWidth - kMargin * 2.0f;
    std::vector<Row> rows;
    std::istringstream in(markup);
    std::vector<std::string> tab;   // the table being read, until a line that is not one
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t lead = line.find_first_not_of(' ');
        if (lead != std::string::npos && line[lead] == '|') {
            tab.push_back(line.substr(lead));
            continue;
        }
        if (!tab.empty()) {
            table(rows, tab, x0, w);
            tab.clear();
        }
        if (lead == std::string::npos) {
            rows.push_back(Row{0.3f, {}});
            continue;
        }
        line.erase(0, lead);

        if (line.rfind("## ", 0) == 0) {
            Row r{0.8f, {}};
            r.pieces.push_back(label(line.substr(3), x0, w));
            r.pieces.push_back(box(x0, 0.6f, w, 0.05f, 110, 120, 150));
            rows.push_back(std::move(r));
        } else if (line.rfind("# ", 0) == 0) {
            Row r{0.95f, {}};
            r.pieces.push_back(box(x0 - 0.15f, 0.0f, w + 0.3f, 0.75f, 50, 70, 110));
            Piece l = label(line.substr(2), x0 + 0.1f, w - 0.2f);
            l.e.y   = 0.2f;
            r.pieces.push_back(std::move(l));
            rows.push_back(std::move(r));
        } else if (line == "---") {
            Row r{0.45f, {}};
            r.pieces.push_back(box(x0, 0.2f, w, 0.05f, 90, 95, 110));
            rows.push_back(std::move(r));
        } else if (line.rfind("?[", 0) == 0 && line.find("](") != std::string::npos &&
                   line.back() == ')') {
            const size_t mid  = line.find("](");
            const std::string hint = line.substr(2, mid - 2);
            const float  hw   = std::min(text_w(hint) + 0.3f, w * 0.45f);
            Row          r{kButton + 0.15f, {}};
            if (!hint.empty()) r.pieces.push_back(label(hint, x0, hw));
            Piece f;
            f.e.kind = ElemKind::Field;
            f.e.x    = x0 + (hint.empty() ? 0.0f : hw);
            f.e.w    = w - (hint.empty() ? 0.0f : hw);
            f.e.h    = kButton;
            f.target = line.substr(mid + 2, line.size() - mid - 3);
            f.field  = true;
            r.pieces.push_back(std::move(f));
            rows.push_back(std::move(r));
        } else if (line[0] == '>') {
            const size_t first = rows.size();
            flow(rows, line.substr(1), x0 + 0.3f, w - 0.6f);
            for (size_t i = first; i < rows.size(); ++i)
                rows[i].pieces.insert(rows[i].pieces.begin(),
                                      box(x0, 0.0f, w, rows[i].h, 38, 40, 48));
        } else {
            flow(rows, line, x0, w);
        }
    }
    if (!tab.empty()) table(rows, tab, x0, w);
    return rows;
}

} // namespace

PageLayout layout_page(const std::string &name, const std::string &owner,
                       const std::string &title, const std::string &markup, int screen)
{
    PageLayout out;
    out.panel.name  = name;
    out.panel.owner = owner;
    out.panel.w     = kPageWidth;
    out.panel.h     = kPageHeight;

    // Screens: rows in order, as many as fit between the title and the foot.
    const std::vector<Row> rows  = parse(markup);
    const float            room  = kPageHeight - kTop - kFoot;
    std::vector<size_t>    start = {0};
    float                  used  = 0.0f;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (used + rows[i].h > room && used > 0.0f) {
            start.push_back(i);
            used = 0.0f;
        }
        used += rows[i].h;
    }
    out.screens = (int)start.size();
    out.screen  = std::clamp(screen, 0, out.screens - 1);

    std::vector<Element> &el = out.panel.elements;
    const auto add = [&](Piece p, float y) {
        if (!p.target.empty()) {
            p.e.id = "pg:" + std::to_string(out.links.size());
            out.links.push_back({p.e.id, p.target, p.field});
        }
        p.e.y += y;
        el.push_back(std::move(p.e));
    };

    add(box(0.0f, 0.0f, kPageWidth, 1.0f, 35, 45, 70), 0.0f);
    {
        Piece t = label(title, kMargin, kPageWidth - kMargin - 1.2f);
        t.e.y   = 0.3f;
        add(std::move(t), 0.0f);
        Piece x = button("x", "", kPageWidth - 1.0f, 0.8f);
        x.e.id  = kPageClose;
        x.e.y   = 0.1f;
        add(std::move(x), 0.0f);
    }

    // Room left for the foot's three, whatever the content.
    const size_t budget = kMaxElements - 4;
    const size_t last   = (size_t)out.screen + 1 < start.size() ? start[(size_t)out.screen + 1]
                                                                : rows.size();
    float y = kTop;
    for (size_t i = start[(size_t)out.screen]; i < last; ++i) {
        if (el.size() + rows[i].pieces.size() > budget) {
            Piece cut = label("(cut: too much on one screen)", kMargin, kPageWidth);
            add(std::move(cut), y);
            break;
        }
        for (const Piece &p : rows[i].pieces) {
            Piece q = p;
            // Text beside buttons sits in their middle.
            if (q.e.kind == ElemKind::Label && rows[i].h >= kButton && q.e.y == 0.0f) q.e.y = 0.22f;
            add(std::move(q), y);
        }
        y += rows[i].h;
    }

    if (out.screens > 1) {
        const float fy = kPageHeight - 0.9f;
        if (out.screen > 0) {
            Piece b = button("< back", "", kMargin, 2.2f);
            b.e.id  = kPagePrev;
            add(std::move(b), fy);
        }
        Piece n = label(std::to_string(out.screen + 1) + " / " + std::to_string(out.screens),
                        kPageWidth * 0.5f - 0.6f, 2.0f);
        add(std::move(n), fy + 0.22f);
        if (out.screen + 1 < out.screens) {
            Piece b = button("next >", "", kPageWidth - kMargin - 2.2f, 2.2f);
            b.e.id  = kPageNext;
            add(std::move(b), fy);
        }
    }
    return out;
}

float page_text_w(const std::string &text)
{
    float px = -1.0f;
    for (size_t at = 0, len; at < text.size(); at += len) px += (float)glyph(text, at, len) + 1.0f;
    return std::max(0.0f, px) * kPx;
}

std::string page_text(const std::string &text)
{
    std::string out = text;
    for (char &c : out) {
        switch (c) {
        case '[': c = '{'; break;
        case ']': c = '}'; break;
        case '(': c = '<'; break;
        case ')': c = '>'; break;
        case '|': c = '!'; break;
        default:  break;
        }
    }
    return out;
}

const PageLink *page_link(const PageLayout &page, const std::string &id)
{
    for (const PageLink &l : page.links)
        if (l.id == id) return &l;
    return nullptr;
}

} // namespace cnp::ui
