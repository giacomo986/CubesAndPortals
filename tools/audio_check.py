#!/usr/bin/env python3
"""What a recording of the client's mix (--record-audio) has in it.

"Not heard" was written in TODO.md some thirty times: the timed client opened
no audio device, so nothing a script ran could say whether a sound played.
Now it records the mix, and this reads the recording -- whether a sound
plays, when, how loud, and whether a loop has a seam. Whether it sounds good
is still for a person.

    python3 tools/audio_check.py out.wav                  # the level in time
    python3 tools/audio_check.py out.wav --expect 6.0-7.5 # fails on silence there
    python3 tools/audio_check.py out.wav --silent 0-5     # fails on sound there
    python3 tools/audio_check.py out.wav --seams 8-20     # clicks in a loop

The level is RMS in dBFS over windows of --window seconds (0.1). A window is
sound when it is above --floor dBFS (-50). A seam is one sample's step much
bigger than the steps around it: the jump a loop makes when its end does not
meet its start.
"""

import argparse
import math
import struct
import sys
import wave


def read(path):
    with wave.open(path, "rb") as w:
        if w.getsampwidth() != 2:
            sys.exit(f"{path}: 16-bit WAV only")
        ch, rate, n = w.getnchannels(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    data = struct.unpack(f"<{len(raw) // 2}h", raw)
    # Mono: the louder of the channels each frame, so a sound panned hard to one
    # side is not halved.
    mono = [max(abs(data[i + c]) for c in range(ch)) / 32768.0 for i in range(0, len(data), ch)]
    signed = [data[i] / 32768.0 for i in range(0, len(data), ch)]
    return rate, mono, signed


def db(x):
    return 20.0 * math.log10(x) if x > 1e-9 else -180.0


def span(text):
    a, _, b = text.partition("-")
    return float(a), float(b)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("wav")
    ap.add_argument("--window", type=float, default=0.1)
    ap.add_argument("--floor", type=float, default=-50.0)
    ap.add_argument("--expect", action="append", default=[], metavar="T0-T1",
                    help="fail unless some window in this span is sound")
    ap.add_argument("--silent", action="append", default=[], metavar="T0-T1",
                    help="fail if any window in this span is sound")
    ap.add_argument("--seams", action="append", default=[], metavar="T0-T1",
                    help="list sample steps far above their neighbours in this span")
    ap.add_argument("--quiet", action="store_true", help="no level table, only the checks")
    a = ap.parse_args()

    rate, mono, signed = read(a.wav)
    step = max(1, int(rate * a.window))
    levels = []
    for i in range(0, len(mono), step):
        chunk = mono[i:i + step]
        rms = math.sqrt(sum(v * v for v in chunk) / len(chunk)) if chunk else 0.0
        levels.append((i / rate, db(rms), db(max(chunk) if chunk else 0.0)))

    total = len(mono) / rate
    loud = [t for t, r, _ in levels if r > a.floor]
    print(f"{a.wav}: {total:.1f} s at {rate} Hz; sound in {len(loud)} of {len(levels)} windows"
          f" (above {a.floor:.0f} dBFS); peak {max((p for _, _, p in levels), default=-180):.1f} dBFS")
    if not a.quiet:
        for t, r, p in levels:
            if r > a.floor:
                bar = "#" * max(0, int((r - a.floor) / 2))
                print(f"  {t:7.2f} s  rms {r:6.1f}  peak {p:6.1f}  {bar}")

    failures = 0
    for e in a.expect:
        t0, t1 = span(e)
        hit = [t for t in loud if t0 <= t < t1]
        if hit:
            print(f"  ok    sound between {t0} and {t1} s (first at {hit[0]:.2f})")
        else:
            print(f"  FAIL  no sound between {t0} and {t1} s")
            failures += 1
    for e in a.silent:
        t0, t1 = span(e)
        hit = [t for t in loud if t0 <= t < t1]
        if hit:
            print(f"  FAIL  sound between {t0} and {t1} s (first at {hit[0]:.2f})")
            failures += 1
        else:
            print(f"  ok    silent between {t0} and {t1} s")
    for e in a.seams:
        t0, t1 = span(e)
        i0, i1 = int(t0 * rate), min(len(signed) - 1, int(t1 * rate))
        steps = [abs(signed[i + 1] - signed[i]) for i in range(i0, i1)]
        if not steps:
            continue
        mean = sum(steps) / len(steps)
        big = [(i0 + k) / rate for k, s in enumerate(steps) if s > 0.05 and s > 12 * mean]
        print(f"  seams between {t0} and {t1} s: {len(big)}"
              + (" at " + ", ".join(f"{t:.3f}" for t in big[:10]) if big else ""))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
