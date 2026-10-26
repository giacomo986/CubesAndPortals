#pragma once

#include <stdbool.h>
#include <stdint.h>

// The only door between the client and the audio platform.
//
// The same rule and the same reason as gfx.h: nothing above this header
// includes raylib.h, and nothing below it decides how loud anything is. What
// crosses the boundary is a buffer of samples and, per play, a gain and a pan
// -- both computed in core/audio/attenuation.h, where they can be checked on a
// machine with no sound card.
//
// It is allowed to fail. A build machine, a container, a session with no
// PulseAudio: snd_init returns false, every call after it does nothing, and the
// game is a game without sound rather than a game that will not start. That is
// why the play call has no error to check and no handle to test -- there is
// nothing a caller could usefully do about it.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_sound { uint32_t id; } snd_sound;   // id == 0 is the invalid handle

// Opens the device. False means there is no audio on this machine, which is not
// an error anywhere above here.
bool snd_init(void);
void snd_shutdown(void);
bool snd_ready(void);

// Mono, signed 16-bit. The buffer is copied.
snd_sound snd_upload(const int16_t *samples, int count, int rate);
void      snd_free(snd_sound s);

// A sound file (.ogg or .wav). The invalid handle when it is missing or will not
// decode, which a caller treats like any other sound it cannot have.
snd_sound snd_load(const char *path);
// The same from the bytes of one, `ext` saying which (".ogg", ".wav"): a sound
// a world sent as media.
snd_sound snd_load_memory(const char *ext, const unsigned char *data, int size);

// gain 0..1, pan -1 (left) .. +1 (right). A gain of zero is not played at all
// rather than played silently: a voice held for something nobody can hear is a
// voice the next sound cannot have.
void snd_play(snd_sound s, float gain, float pan);

// A sound held going round and round while `on` is said every frame, and
// stopped the first frame it is not. The clip has to loop cleanly by itself.
void snd_loop(snd_sound s, float gain, bool on);

// The whole game's volume, 0..1 (the options' "Volume"). Kept when said before
// the device is open, and applied when it is.
void snd_set_volume(float v);

// How many sounds are loaded, for a caller that wants to say so.
int snd_loaded(void);

#ifdef __cplusplus
}
#endif
