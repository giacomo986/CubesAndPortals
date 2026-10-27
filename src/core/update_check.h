#pragma once

// Is there a newer release than this build? Asked once, of the repository's
// releases on GitHub (kReleasesApi in core/version.h), on a thread of its own
// so neither the client's title screen nor a server's tick waits for it. Nothing is sent but the request; an
// answer that does not come, or does not parse, is no update.

#include <string>

namespace cnp::update {

// Starts the question. Once is enough; a second call does nothing.
void start();

// The newer release's tag ("v0.9.0-alpha.3"), or empty: not answered yet, no
// newer release, or no answer at all.
std::string newer();

// Below zero when a is older than b, zero when the same, above when newer.
// "0.9.0-alpha.2" against "v0.9.0-alpha.10" or "0.9.0": the leading v is
// optional, numbers compare as numbers, and a release outranks its alphas.
int compare(const std::string &a, const std::string &b);

} // namespace cnp::update
