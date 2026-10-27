#pragma once

// What this build calls itself.
//
// Deliberately not what any compatibility decision is made on. A version string
// is written by a person and can be wrong, stale, or the same on two builds
// that differ in the way that matters; it goes in logs and in the join line so
// a human reading them knows what they are looking at.
//
// The decisions are made on two numbers that are derived rather than declared:
// the protocol version (net/protocol.h), which changes when the wire format
// does, and the content hash (registry/core_nodes.h), which changes when the
// block table does. If those two agree, a mismatched version string is
// cosmetic. If either disagrees, a matching version string is a lie.

namespace cnp {

// MAJOR.MINOR.PATCH-alpha.N while the game is in alpha (docs/BUILDING.md,
// "Versioni"): N goes up with each alpha handed out, MINOR when an alpha
// breaks saved worlds or the protocol. The packages take their file names
// from this line and a release is tagged v<this>.
inline constexpr const char *kEngineVersion = "0.9.0-alpha.4";

// Where the releases are: the client asks the second once on the title screen
// whether there is a newer one (core/update_check.h) and opens the first to
// download it. A fork changes these two lines.
inline constexpr const char *kReleasesPage = "https://github.com/giacomo986/CubesAndPortals/releases";
inline constexpr const char *kReleasesApi  = "https://api.github.com/repos/giacomo986/CubesAndPortals/releases?per_page=1";

} // namespace cnp
