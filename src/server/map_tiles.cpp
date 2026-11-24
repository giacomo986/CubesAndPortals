// --map-tiles DIR: the world file seen from above, as PNG tiles and a page to
// look at them. Read from the file alone, so it runs with the server off; with
// the server on, it runs on a copy made by /backup.

#include "map_tiles.h"

#include "core/media/png.h"
#include "core/registry/node_registry.h"
#include "core/storage/world_db.h"
#include "core/voxel/chunk.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

namespace cnp {

namespace {

constexpr int kTileColumns = 8;                          // columns a side
constexpr int kTileNodes   = kTileColumns * kChunkSize;  // 256 pixels a side

int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

} // namespace

int write_map_tiles(const WorldDb &db, const NodeRegistry &reg, const std::string &dir)
{
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        std::printf("map: cannot make %s: %s\n", dir.c_str(), ec.message().c_str());
        return 1;
    }

    std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> tiles;
    for (const auto &c : db.stored_columns())
        tiles[{floor_div(c.first, kTileColumns), floor_div(c.second, kTileColumns)}].push_back(c);
    if (tiles.empty()) {
        std::printf("map: the world file has no chunks stored\n");
        return 1;
    }

    int minx = INT_MAX, minz = INT_MAX, maxx = INT_MIN, maxz = INT_MIN;
    for (const auto &[tile, columns] : tiles) {
        const int tx = tile.first, tz = tile.second;
        std::vector<int>     height((size_t)kTileNodes * kTileNodes, INT_MIN);
        std::vector<uint8_t> px((size_t)kTileNodes * kTileNodes * 4, 0);

        for (const auto &[cx, cz] : columns) {
            std::string err;
            auto chunks = db.load_column(cx, cz, err);
            if (!err.empty()) std::printf("map: %s\n", err.c_str());
            std::sort(chunks.begin(), chunks.end(),
                      [](const auto &a, const auto &b) { return a->pos().y > b->pos().y; });
            const int ox = (cx - tx * kTileColumns) * kChunkSize;
            const int oz = (cz - tz * kTileColumns) * kChunkSize;
            for (int z = 0; z < kChunkSize; ++z)
                for (int x = 0; x < kChunkSize; ++x) {
                    const size_t i = (size_t)(oz + z) * kTileNodes + (size_t)(ox + x);
                    for (const auto &ch : chunks) {
                        int y = kChunkSize - 1;
                        for (; y >= 0; --y) {
                            const BlockId b = ch->get(x, y, z);
                            if (b != kAir && (reg.is_solid(b) || reg.is_liquid(b))) break;
                        }
                        if (y < 0) continue;
                        const NodeDef &d = reg.get(ch->get(x, y, z));
                        height[i] = ch->pos().y * kChunkSize + y;
                        px[i * 4 + 0] = d.colour[0];
                        px[i * 4 + 1] = d.colour[1];
                        px[i * 4 + 2] = d.colour[2];
                        px[i * 4 + 3] = 255;
                        break;
                    }
                }
        }

        // Light from the north-west: a cell higher than the one before it is
        // brighter, lower is darker.
        for (int z = 1; z < kTileNodes; ++z)
            for (int x = 1; x < kTileNodes; ++x) {
                const size_t i = (size_t)z * kTileNodes + x;
                const int    h0 = height[i - kTileNodes - 1];
                if (height[i] == INT_MIN || h0 == INT_MIN) continue;
                const float s = std::clamp(1.0f + 0.1f * (float)(height[i] - h0), 0.7f, 1.3f);
                for (int k = 0; k < 3; ++k)
                    px[i * 4 + k] = (uint8_t)std::min(255.0f, (float)px[i * 4 + k] * s);
            }

        std::string err;
        const std::string name = dir + "/" + std::to_string(tx) + "_" + std::to_string(tz) + ".png";
        if (!png_write_rgba(name, px.data(), kTileNodes, kTileNodes, err)) {
            std::printf("map: %s\n", err.c_str());
            return 1;
        }
        minx = std::min(minx, tx), maxx = std::max(maxx, tx);
        minz = std::min(minz, tz), maxz = std::max(maxz, tz);
    }

    // The page: the tiles side by side, scroll to move, + and - to zoom, and
    // the block coordinates under the pointer (what /rollback and /inspect want).
    std::ofstream f(dir + "/index.html");
    f << "<!doctype html><meta charset=utf-8><title>Map</title><style>"
         "body{margin:0;background:#222;color:#eee;font:14px sans-serif}"
         "#m{position:relative;transform-origin:0 0}"
         "#m img{position:absolute;image-rendering:pixelated;width:" << kTileNodes << "px}"
         "#c{position:fixed;top:8px;left:8px;background:#000a;padding:4px 8px;z-index:1}"
         "</style><div id=c>+ / - zoom &middot; <span id=xy></span></div><div id=m style='width:"
      << (maxx - minx + 1) * kTileNodes << "px;height:" << (maxz - minz + 1) * kTileNodes << "px'>";
    for (const auto &[tile, columns] : tiles)
        f << "<img src='" << tile.first << "_" << tile.second << ".png' style='left:"
          << (tile.first - minx) * kTileNodes << "px;top:" << (tile.second - minz) * kTileNodes
          << "px'>";
    f << "</div><script>let z=1;const m=document.getElementById('m');"
         "addEventListener('keydown',e=>{if(e.key=='+')z*=2;if(e.key=='-')z/=2;"
         "m.style.transform='scale('+z+')'});"
         "m.addEventListener('mousemove',e=>{const r=m.getBoundingClientRect();"
         "document.getElementById('xy').textContent='x '+Math.floor((e.clientX-r.left)/z+"
      << minx * kTileNodes << ")+'  z '+Math.floor((e.clientY-r.top)/z+" << minz * kTileNodes
      << ")});</script>\n";

    std::printf("map: %zu tile(s) of %d x %d nodes in %s (open index.html)\n", tiles.size(),
                kTileNodes, kTileNodes, dir.c_str());
    return 0;
}

} // namespace cnp
