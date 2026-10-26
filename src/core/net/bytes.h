#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Reading and writing the bytes that go on the wire.
//
// Every byte a ByteReader touches came from somewhere else -- another process,
// possibly on another machine, possibly not running our code at all. So the
// rules here are not style:
//
//   - **Explicit little endian.** Byte by byte, never a memcpy of a struct and
//     never a cast to a wider type. A big-endian machine produces the same
//     bytes, and no padding ever reaches the wire.
//   - **Every read is bounds checked**, and the first overrun poisons the
//     reader: reads after it return zero and change nothing. That means the
//     parser above can read a whole message and check `ok()` once at the end
//     instead of checking after every field, which is the check people forget.
//   - **No allocation sized by a number that has not been validated.** A count
//     read off the wire is compared against a cap before it is used as a loop
//     bound, so a two-byte lie cannot cost 65535 iterations or a 4 GB reserve.
//   - **Trailing bytes are an error.** A message that parses but has bytes left
//     over is a message we did not understand, and `done()` says so.

namespace cnp {

// A string on the wire is a u16 length and that many bytes. The cap is here so
// no code path has to invent one: names, mod ids and error strings are all
// short, and anything claiming otherwise is either broken or hostile.
inline constexpr size_t kMaxWireString = 1024;

class ByteWriter {
public:
    void u8(uint8_t v) { m_data.push_back(v); }

    void u16(uint16_t v)
    {
        m_data.push_back((uint8_t)(v & 0xFF));
        m_data.push_back((uint8_t)((v >> 8) & 0xFF));
    }

    void u32(uint32_t v)
    {
        for (int i = 0; i < 4; ++i) m_data.push_back((uint8_t)((v >> (8 * i)) & 0xFF));
    }

    void u64(uint64_t v)
    {
        for (int i = 0; i < 8; ++i) m_data.push_back((uint8_t)((v >> (8 * i)) & 0xFF));
    }

    void i32(int32_t v) { u32((uint32_t)v); }

    void f32(float v);

    // Writes at most kMaxWireString bytes; a longer string is truncated rather
    // than silently producing a length the reader will refuse. Callers that
    // care validate before they get here.
    void str(const std::string &s);

    void bytes(const void *p, size_t n);

    const std::vector<uint8_t> &data() const { return m_data; }
    size_t                      size() const { return m_data.size(); }
    void                        clear() { m_data.clear(); }

private:
    std::vector<uint8_t> m_data;
};

class ByteReader {
public:
    ByteReader(const void *data, size_t size)
        : m_data((const uint8_t *)data), m_size(size)
    {
    }

    explicit ByteReader(const std::vector<uint8_t> &v) : ByteReader(v.data(), v.size()) {}

    // A reader never owns its bytes, so it must not be handed a temporary: the
    // vector dies at the end of the declaration and every read after that is a
    // read of freed memory. Deleted rather than left to be found, because what
    // it produces is not a crash -- it is a decode that refuses, which in a test
    // whose point is that a bad message IS refused looks exactly like a pass.
    // Three checks in protocol_test were green for that reason before this line
    // existed. Encode into a named buffer and read that.
    explicit ByteReader(std::vector<uint8_t> &&) = delete;

    uint8_t  u8();
    uint16_t u16();
    uint32_t u32();
    uint64_t u64();
    int32_t  i32() { return (int32_t)u32(); }
    float    f32();

    // `max` is the caller's cap, which is what makes an absurd length cheap to
    // refuse: nothing is allocated before the length has been compared to it.
    bool str(std::string &out, size_t max = kMaxWireString);

    bool bytes(void *out, size_t n);

    // A count field, refused before it is ever used as a loop bound.
    bool count(size_t &out, size_t max);

    bool   ok() const { return m_ok; }
    bool   done() const { return m_ok && m_at == m_size; }   // no trailing bytes
    size_t remaining() const { return m_ok ? m_size - m_at : 0; }

private:
    bool take(size_t n);

    const uint8_t *m_data = nullptr;
    size_t         m_size = 0;
    size_t         m_at   = 0;
    bool           m_ok   = true;
};

} // namespace cnp
