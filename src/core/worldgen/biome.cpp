#include "core/worldgen/biome.h"

#include "core/worldgen/noise.h"

namespace cnp {
namespace {

// Value noise interpolated between four corners does not spend much time near
// 0 or 1 -- the middle of a cell is an average of four uniform numbers, and
// averages crowd the centre. Left alone, the extreme climates would be rare
// enough that a small world never sees a desert. The stretch pushes the tails
// back out; the clamp is what the stretch costs, and it costs it at the ends,
// where the biome is already the nearest one by a wide margin.
float stretch(float n)
{
    float c = 0.5f + (n - 0.5f) * 1.7f;
    if (c < 0.0f) c = 0.0f;
    if (c > 1.0f) c = 1.0f;
    return c;
}

// Different periods as well as different seeds. With the same period the two
// fields share their cell boundaries, and every biome border in the world would
// land on the same grid. A quarter of each field is a finer octave, so a border
// wanders instead of running along the big cells' smooth curves.
constexpr int kTemperaturePeriod = 256;
constexpr int kHumidityPeriod    = 208;

float climate(int x, int z, int period, uint32_t seed)
{
    return 0.88f * value_noise(x, z, period, seed) + 0.12f * value_noise(x, z, period / 4, seed ^ 0x7F4A7C15u);
}

} // namespace

float biome_temperature(int x, int z, uint32_t seed)
{
    return stretch(climate(x, z, kTemperaturePeriod, seed ^ 0xB1013DE1u));
}

float biome_humidity(int x, int z, uint32_t seed)
{
    return stretch(climate(x, z, kHumidityPeriod, seed ^ 0x48D31F07u));
}

BiomeSet::BiomeSet()
{
    BiomeDef plains;
    plains.name = "core:plains";
    plains.temperature = 0.55f; plains.humidity = 0.55f;
    plains.top = "core:grass"; plains.filler = "core:dirt"; plains.filler_depth = 3;
    plains.base = 0.0f; plains.relief = 0.40f;
    add(plains);

    BiomeDef desert;
    desert.name = "core:desert";
    desert.temperature = 0.90f; desert.humidity = 0.15f;
    desert.top = "core:sand"; desert.filler = "core:sand"; desert.filler_depth = 4;
    desert.base = -2.0f; desert.relief = 0.55f;
    add(desert);

    BiomeDef tundra;
    tundra.name = "core:tundra";
    tundra.temperature = 0.10f; tundra.humidity = 0.60f;
    tundra.top = "core:snow"; tundra.filler = "core:dirt"; tundra.filler_depth = 3;
    tundra.base = 1.0f; tundra.relief = 0.70f;
    add(tundra);

    BiomeDef peaks;
    peaks.name = "core:peaks";
    peaks.temperature = 0.30f; peaks.humidity = 0.15f;
    peaks.top = "core:stone"; peaks.filler = "core:stone"; peaks.filler_depth = 4;
    peaks.base = 10.0f; peaks.relief = 1.70f;
    add(peaks);

    // The waters and their shores, handed out by mapgen_biome_at from where
    // the ground lies, never by climate.
    auto water = [&](const char *name, const char *top, const char *filler) {
        BiomeDef b;
        b.name = name; b.top = top; b.filler = filler; b.filler_depth = 3; b.climate = false;
        add(b);
    };
    water("core:ocean", "core:sand", "core:sand");   // deep, far from land
    water("core:sea", "core:sand", "core:sand");     // the shelf along a coast
    water("core:river", "core:sand", "core:dirt");
    water("core:lake", "core:dirt", "core:dirt");    // a hollow inland
    water("core:beach", "core:sand", "core:sand");   // at the water's edge
    water("core:gorge", "core:stone", "core:stone"); // a gorge's walls and its torrent's bed
}

void BiomeSet::add(const BiomeDef &def)
{
    m_biomes.push_back(def);
}

size_t BiomeSet::find(const std::string &name) const
{
    for (size_t i = 0; i < m_biomes.size(); ++i)
        if (m_biomes[i].name == name) return i;
    return m_biomes.size();
}

size_t BiomeSet::pick(float temperature, float humidity) const
{
    size_t best      = 0;
    float  best_dist = -1.0f;

    for (size_t i = 0; i < m_biomes.size(); ++i) {
        if (!m_biomes[i].climate) continue;
        const float dt = temperature - m_biomes[i].temperature;
        const float dh = humidity - m_biomes[i].humidity;
        const float d  = dt * dt + dh * dh;

        // The tie is broken on the name and not on the index, so that adding a
        // biome cannot silently change which of two others owns a column.
        const bool better = (best_dist < 0.0f) || d < best_dist ||
                            (d == best_dist && m_biomes[i].name < m_biomes[best].name);
        if (better) {
            best_dist = d;
            best      = i;
        }
    }
    return best;
}

BiomeShape BiomeSet::shape(float temperature, float humidity) const
{
    // Inverse fourth power of the distance. The square alone is too gentle:
    // standing in the middle of the peaks you would get three quarters of their
    // height and a quarter of everybody else's, so no biome would ever look
    // like itself. The epsilon is what keeps a column standing exactly on a
    // biome's point finite.
    constexpr float kEpsilon = 0.004f;

    float sum_w = 0.0f, sum_base = 0.0f, sum_relief = 0.0f;

    for (const BiomeDef &b : m_biomes) {
        if (!b.climate) continue;
        const float dt = temperature - b.temperature;
        const float dh = humidity - b.humidity;
        const float d2 = dt * dt + dh * dh + kEpsilon;
        const float w  = 1.0f / (d2 * d2);

        sum_w      += w;
        sum_base   += w * b.base;
        sum_relief += w * b.relief;
    }

    BiomeShape s;
    s.base   = sum_base / sum_w;
    s.relief = sum_relief / sum_w;
    return s;
}

} // namespace cnp
