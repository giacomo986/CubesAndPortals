#!/bin/sh
# Runs a command with its sound going to a sink nobody hears.
#
# A client recording what it plays (--record-audio) has to open a real audio
# device, and on a desktop that is the speakers: a test run would play its
# shots and footsteps into the room. This makes a PulseAudio / PipeWire null
# sink, points the command at it (PULSE_SINK) and removes the sink afterwards,
# whatever the command did.
#
#   tools/null_sink.sh build/src/client/cnp_client --connect ... --record-audio out.wav
#
# Without pactl or a sound server it runs the command as it is and says so: the
# client then finds no device and records nothing, which it also says.

set -u
NAME="cnp_null_$$"
MODULE=""
if command -v pactl > /dev/null 2>&1; then
    MODULE=$(pactl load-module module-null-sink sink_name="$NAME" \
                 sink_properties=device.description="$NAME" 2>/dev/null) || MODULE=""
fi
if [ -n "$MODULE" ]; then
    trap 'pactl unload-module "$MODULE" 2>/dev/null' EXIT INT TERM
    PULSE_SINK="$NAME" "$@"
    exit $?
fi
echo "null_sink: no sound server to make a sink on; running as it is" >&2
"$@"
