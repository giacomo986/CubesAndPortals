// raylib implementation of snd.h.
//
// The second translation unit in the client allowed to include raylib.h, and
// for the same reason as the first: everything above it is written against a
// handle and two floats.
//
// raylib's pan happens to use the same convention this engine does -- -1 left,
// 0 centre, +1 right -- so there is no conversion here to get backwards. Said
// out loud because that is exactly the kind of line that is wrong for a year.

#include "client/snd/snd.h"

#include <raylib.h>

#include <cstdio>
#include <unordered_map>
#include <vector>

namespace {

struct Loaded {
    Sound sound{};
};

std::unordered_map<uint32_t, Loaded> g_sounds;
uint32_t                             g_next  = 1;
bool                                 g_ready = false;
float                                g_volume = 1.0f;

} // namespace

extern "C" {

bool snd_init(void)
{
    if (g_ready) return true;

    InitAudioDevice();
    g_ready = IsAudioDeviceReady();
    if (g_ready) SetMasterVolume(g_volume);

    // Said once, plainly. A game that is quiet because the machine has no audio
    // device and a game that is quiet because of a defect look identical, and
    // this line is the difference.
    if (!g_ready) std::printf("audio: no device, so this run has no sound\n");
    return g_ready;
}

void snd_shutdown(void)
{
    if (!g_ready) return;
    for (auto &kv : g_sounds) UnloadSound(kv.second.sound);
    g_sounds.clear();
    CloseAudioDevice();
    g_ready = false;
}

bool snd_ready(void) { return g_ready; }

snd_sound snd_upload(const int16_t *samples, int count, int rate)
{
    snd_sound out{0};
    if (!g_ready || samples == nullptr || count <= 0) return out;

    Wave w{};
    w.frameCount = (unsigned int)count;
    w.sampleRate = (unsigned int)rate;
    w.sampleSize = 16;
    w.channels   = 1;

    // raylib takes ownership of nothing here: LoadSoundFromWave copies into the
    // audio buffer, and the vector below dies at the end of this function.
    std::vector<int16_t> copy(samples, samples + count);
    w.data = copy.data();

    const Sound s = LoadSoundFromWave(w);
    if (s.frameCount == 0) return out;

    out.id = g_next++;
    g_sounds[out.id] = Loaded{s};
    return out;
}

snd_sound snd_load(const char *path)
{
    snd_sound out{0};
    if (!g_ready || path == nullptr || !FileExists(path)) return out;

    const Sound s = LoadSound(path);
    if (s.frameCount == 0) return out;

    out.id = g_next++;
    g_sounds[out.id] = Loaded{s};
    return out;
}

snd_sound snd_load_memory(const char *ext, const unsigned char *data, int size)
{
    snd_sound out{0};
    if (!g_ready || ext == nullptr || data == nullptr || size <= 0) return out;
    const Wave w = LoadWaveFromMemory(ext, data, size);
    if (w.frameCount == 0) return out;
    const Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    if (s.frameCount == 0) return out;

    out.id = g_next++;
    g_sounds[out.id] = Loaded{s};
    return out;
}

void snd_free(snd_sound s)
{
    const auto it = g_sounds.find(s.id);
    if (it == g_sounds.end()) return;
    UnloadSound(it->second.sound);
    g_sounds.erase(it);
}

void snd_play(snd_sound s, float gain, float pan)
{
    if (!g_ready || gain <= 0.0f) return;

    const auto it = g_sounds.find(s.id);
    if (it == g_sounds.end()) return;

    SetSoundVolume(it->second.sound, gain > 1.0f ? 1.0f : gain);
    SetSoundPan(it->second.sound, pan < -1.0f ? -1.0f : (pan > 1.0f ? 1.0f : pan));
    PlaySound(it->second.sound);
}

void snd_loop(snd_sound s, float gain, bool on)
{
    if (!g_ready) return;
    const auto it = g_sounds.find(s.id);
    if (it == g_sounds.end()) return;

    Sound &snd = it->second.sound;
    if (!on) {
        if (IsSoundPlaying(snd)) StopSound(snd);
        return;
    }
    SetSoundVolume(snd, gain > 1.0f ? 1.0f : gain);
    SetSoundPan(snd, 0.0f);
    if (!IsSoundPlaying(snd)) PlaySound(snd);
}

void snd_set_volume(float v)
{
    g_volume = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    if (g_ready) SetMasterVolume(g_volume);
}

int snd_loaded(void) { return (int)g_sounds.size(); }

} // extern "C"
