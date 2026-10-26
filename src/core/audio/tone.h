#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The sounds this engine has, until it has real ones.
//
// The same bargain the block textures make: a NodeDef carries a colour and the
// engine bakes a placeholder from it, so a mod can add a block before anybody
// has drawn one. This is that for audio -- a sound is a frequency, a length and
// whether it is pitched or not, and the samples are generated. Nothing here
// opens an audio device or knows what one is; the client's snd_* backend takes
// the buffer.
//
// Generated rather than shipped for one more reason: a .wav in the repository
// is a file two clients can disagree about, and the media path that would fix
// that (content-addressed, streamed, cached) exists for textures and does not
// cover sounds yet. A generated sound is the same on both machines by
// construction.

namespace cnp {

// Mono, signed 16-bit. 22050 because a placeholder does not need more and half
// the samples is half the memory in a table the client keeps resident.
inline constexpr int kSampleRate = 22050;

// How loud a baked sound is at its peak, out of 32767. Short of full scale on
// purpose: several of these can be playing at once and a mixer that clips is a
// mixer nobody can turn down.
inline constexpr int16_t kBakePeak = 9000;

struct SoundDef {
    std::string name;              // namespaced, like a node: "core:break"

    float hz      = 440.0f;        // where the pitch starts
    float hz_end  = 0.0f;          // and where it ends; 0 means it does not move
    float seconds = 0.15f;
    bool  noise   = false;         // pitchless, for the sounds that are not a note
};

// The samples. Deterministic: the same definition gives the same bytes on every
// machine, which is the only reason two clients can be said to hear the same
// thing.
std::vector<int16_t> bake_sound(const SoundDef &def);

} // namespace cnp
