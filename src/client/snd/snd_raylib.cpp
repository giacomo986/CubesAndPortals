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
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// A loop is not the Sound played again when it ends: that waits for the next
// frame to notice, a hole of a frame each time round (over 200 ms at 9 fps).
// It is a Music stream over the same file's bytes, which the mixer refills and
// wraps itself, made the first time the sound is looped.
struct Loaded {
    Sound                      sound{};
    std::vector<unsigned char> bytes;   // the file, for the loop's stream
    std::string                ext;     // ".ogg" or ".wav"
    Music                      music{};
    bool                       has_music = false;
};

// Frames in each half of a loop's stream buffer: half a second at 48 kHz, so a
// frame as slow as 2 fps still refills it before it runs dry. Only memory: the
// volume and the pan are applied as it is mixed, so they change at once.
constexpr int kLoopBufferFrames = 24000;

std::unordered_map<uint32_t, Loaded> g_sounds;
uint32_t                             g_next  = 1;
bool                                 g_ready = false;
float                                g_volume = 1.0f;

// --record-audio: the mix, interleaved stereo, appended on the audio thread.
std::mutex         g_rec_mutex;
std::vector<float> g_rec;
bool               g_recording = false;

void record_mix(void *buffer, unsigned int frames)
{
    const float *in = static_cast<const float *>(buffer);
    std::lock_guard<std::mutex> lock(g_rec_mutex);
    g_rec.insert(g_rec.end(), in, in + (size_t)frames * 2);
}

void put_u32(std::FILE *f, uint32_t v)
{
    const unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16),
                                (unsigned char)(v >> 24)};
    std::fwrite(b, 1, 4, f);
}

void put_u16(std::FILE *f, uint16_t v)
{
    const unsigned char b[2] = {(unsigned char)v, (unsigned char)(v >> 8)};
    std::fwrite(b, 1, 2, f);
}

// A mono 16-bit WAV file in memory, for a sound uploaded as samples.
std::vector<unsigned char> wav_bytes(const int16_t *samples, int count, int rate)
{
    std::vector<unsigned char> b;
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((unsigned char)(v >> (8 * i))); };
    auto u16 = [&](uint16_t v) { b.push_back((unsigned char)v); b.push_back((unsigned char)(v >> 8)); };
    auto tag = [&](const char *t) { b.insert(b.end(), t, t + 4); };
    const uint32_t bytes = (uint32_t)count * 2;
    tag("RIFF"); u32(36 + bytes); tag("WAVE"); tag("fmt "); u32(16);
    u16(1); u16(1); u32((uint32_t)rate); u32((uint32_t)rate * 2); u16(2); u16(16);
    tag("data"); u32(bytes);
    const size_t at = b.size();
    b.resize(at + bytes);
    std::memcpy(b.data() + at, samples, bytes);
    return b;
}

void unload(Loaded &l)
{
    if (l.has_music) UnloadMusicStream(l.music);
    l.has_music = false;
    UnloadSound(l.sound);
}

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
    if (g_recording) {
        DetachAudioMixedProcessor(record_mix);
        g_recording = false;
    }
    for (auto &kv : g_sounds) unload(kv.second);
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
    Loaded &l = g_sounds[out.id];
    l.sound   = s;
    l.bytes   = wav_bytes(samples, count, rate);
    l.ext     = ".wav";
    return out;
}

snd_sound snd_load(const char *path)
{
    snd_sound out{0};
    if (!g_ready || path == nullptr || !FileExists(path)) return out;

    int            size = 0;
    unsigned char *data = LoadFileData(path, &size);
    if (data == nullptr) return out;
    out = snd_load_memory(GetFileExtension(path), data, size);
    UnloadFileData(data);
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
    Loaded &l = g_sounds[out.id];
    l.sound   = s;
    l.bytes.assign(data, data + size);
    l.ext = ext;
    return out;
}

void snd_free(snd_sound s)
{
    const auto it = g_sounds.find(s.id);
    if (it == g_sounds.end()) return;
    unload(it->second);
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

void snd_loop(snd_sound s, float gain, float pan, bool on)
{
    if (!g_ready) return;
    const auto it = g_sounds.find(s.id);
    if (it == g_sounds.end()) return;

    Loaded &l = it->second;
    if (!on) {
        if (l.has_music && IsMusicStreamPlaying(l.music)) StopMusicStream(l.music);
        return;
    }
    if (!l.has_music) {
        if (l.bytes.empty()) return;
        SetAudioStreamBufferSizeDefault(kLoopBufferFrames);
        l.music = LoadMusicStreamFromMemory(l.ext.c_str(), l.bytes.data(), (int)l.bytes.size());
        SetAudioStreamBufferSizeDefault(0);
        if (l.music.frameCount == 0) {
            l.bytes.clear();   // will not decode as a stream: not tried again every frame
            return;
        }
        l.music.looping = true;
        l.has_music     = true;
    }
    SetMusicVolume(l.music, gain > 1.0f ? 1.0f : gain);
    SetMusicPan(l.music, pan < -1.0f ? -1.0f : (pan > 1.0f ? 1.0f : pan));
    if (!IsMusicStreamPlaying(l.music)) PlayMusicStream(l.music);
    UpdateMusicStream(l.music);
}

void snd_set_volume(float v)
{
    g_volume = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    if (g_ready) SetMasterVolume(g_volume);
}

int snd_loaded(void) { return (int)g_sounds.size(); }

bool snd_record_start(void)
{
    if (!g_ready || g_recording) return g_recording;
    {
        std::lock_guard<std::mutex> lock(g_rec_mutex);
        g_rec.clear();
    }
    AttachAudioMixedProcessor(record_mix);
    g_recording = true;
    return true;
}

float snd_record_seconds(void)
{
    std::lock_guard<std::mutex> lock(g_rec_mutex);
    return (float)g_rec.size() / 2.0f / (float)SND_RECORD_RATE;
}

float snd_record_save(const char *path)
{
    if (!g_recording) return -1.0f;
    DetachAudioMixedProcessor(record_mix);
    g_recording = false;
    std::vector<float> mix;
    {
        std::lock_guard<std::mutex> lock(g_rec_mutex);
        mix.swap(g_rec);
    }
    std::FILE *f = std::fopen(path, "wb");
    if (!f) return -1.0f;
    const uint32_t bytes = (uint32_t)(mix.size() * 2);
    std::fwrite("RIFF", 1, 4, f);
    put_u32(f, 36 + bytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    put_u32(f, 16);
    put_u16(f, 1);                              // PCM
    put_u16(f, 2);                              // stereo
    put_u32(f, SND_RECORD_RATE);
    put_u32(f, SND_RECORD_RATE * 2 * 2);        // bytes a second
    put_u16(f, 4);                              // bytes a frame
    put_u16(f, 16);
    std::fwrite("data", 1, 4, f);
    put_u32(f, bytes);
    for (const float v : mix) {
        const float   c = v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
        const int16_t s = (int16_t)(c * 32767.0f);
        put_u16(f, (uint16_t)s);
    }
    const bool ok = std::fclose(f) == 0;
    return ok ? (float)mix.size() / 2.0f / (float)SND_RECORD_RATE : -1.0f;
}

} // extern "C"
