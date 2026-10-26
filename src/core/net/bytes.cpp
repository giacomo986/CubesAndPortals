#include "core/net/bytes.h"

#include <cstring>

namespace cnp {

void ByteWriter::f32(float v)
{
    // Through a memcpy, not a cast: type punning through a pointer is
    // undefined, and the bit pattern is what we want on the wire anyway.
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    u32(bits);
}

void ByteWriter::str(const std::string &s)
{
    const size_t n = s.size() > kMaxWireString ? kMaxWireString : s.size();
    u16((uint16_t)n);
    bytes(s.data(), n);
}

void ByteWriter::bytes(const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    m_data.insert(m_data.end(), b, b + n);
}

bool ByteReader::take(size_t n)
{
    if (!m_ok) return false;
    // The addition is done on size_t values that are both already bounded by
    // the buffer, so it cannot wrap: m_at <= m_size always holds.
    if (n > m_size - m_at) {
        m_ok = false;
        return false;
    }
    m_at += n;
    return true;
}

uint8_t ByteReader::u8()
{
    const size_t at = m_at;
    if (!take(1)) return 0;
    return m_data[at];
}

uint16_t ByteReader::u16()
{
    const size_t at = m_at;
    if (!take(2)) return 0;
    return (uint16_t)((uint16_t)m_data[at] | ((uint16_t)m_data[at + 1] << 8));
}

uint32_t ByteReader::u32()
{
    const size_t at = m_at;
    if (!take(4)) return 0;
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= (uint32_t)m_data[at + (size_t)i] << (8 * i);
    return v;
}

uint64_t ByteReader::u64()
{
    const size_t at = m_at;
    if (!take(8)) return 0;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (uint64_t)m_data[at + (size_t)i] << (8 * i);
    return v;
}

float ByteReader::f32()
{
    const uint32_t bits = u32();
    float          v    = 0.0f;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

bool ByteReader::str(std::string &out, size_t max)
{
    out.clear();

    const size_t n = u16();
    if (!m_ok) return false;

    // Checked before the assign, which is the whole point: the length is a
    // claim by the sender until it has been compared against a cap we chose.
    if (n > max || n > kMaxWireString) {
        m_ok = false;
        return false;
    }

    const size_t at = m_at;
    if (!take(n)) return false;
    out.assign((const char *)m_data + at, n);
    return true;
}

bool ByteReader::bytes(void *out, size_t n)
{
    const size_t at = m_at;
    if (!take(n)) return false;
    std::memcpy(out, m_data + at, n);
    return true;
}

bool ByteReader::count(size_t &out, size_t max)
{
    out = 0;

    const size_t n = u16();
    if (!m_ok) return false;
    if (n > max) {
        m_ok = false;
        return false;
    }
    out = n;
    return true;
}

} // namespace cnp
