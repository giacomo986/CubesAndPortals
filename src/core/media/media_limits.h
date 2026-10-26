#pragma once

#include <cstddef>

// How big a media file may be, and how many of them a world may have.
//
// Its own header because three layers need the same two numbers and none of
// them should include the others to get them: the wire parser refuses an
// absurd length before it allocates, the server's content set refuses to hold
// one, and the client's cache refuses to read one back off disk. A cap that
// lived in one of those and was re-guessed in the other two is a cap with three
// values.

namespace cnp {

// Media are textures. A 32x32 tile out of the stored-deflate encoder is about
// 4.2 KB, and real art is a few hundred bytes to a few tens of KB. A megabyte
// is far above anything legitimate and far below what would make one message a
// memory problem.
inline constexpr size_t kMaxMediaBytes = 1024u * 1024u;

// One per block, and a world with four thousand distinct blocks is well past
// anything this engine has generated.
inline constexpr size_t kMaxMediaItems = 4096;

} // namespace cnp
