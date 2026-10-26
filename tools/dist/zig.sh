#!/bin/sh
# One wrapper for every tool CMake wants a single path for: the name it is
# called by (zig-cc, zig-c++, zig-ar, ...) picks the zig subcommand.
tool="${0##*/zig-}"
case "$tool" in
    cc|c++) exec zig "$tool" -target "${CNP_ZIG_TARGET:-x86_64-windows-gnu}" "$@" ;;
    *)      exec zig "$tool" "$@" ;;
esac
