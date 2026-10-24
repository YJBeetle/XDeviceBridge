#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace xdb::util {
// Callers check available bytes and keep variable widths within 0..8.
constexpr uint64_t read_be(const uint8_t *bytes, unsigned width) {
    uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i) value = (value << 8) | bytes[i];
    return value;
}
constexpr uint64_t read_le(const uint8_t *bytes, unsigned width) {
    uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i) value |= uint64_t(bytes[i]) << (8 * i);
    return value;
}
constexpr void write_be(uint8_t *bytes, uint64_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i) bytes[width - i - 1] = uint8_t(value >> (8 * i));
}
constexpr void write_le(uint8_t *bytes, uint64_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i) bytes[i] = uint8_t(value >> (8 * i));
}
inline void append_be(std::vector<uint8_t> &out, uint64_t value, unsigned width) {
    const auto start = out.size(); out.resize(start + width);
    write_be(out.data() + start, value, width);
}
inline void append_le(std::vector<uint8_t> &out, uint64_t value, unsigned width) {
    const auto start = out.size(); out.resize(start + width);
    write_le(out.data() + start, value, width);
}
constexpr uint16_t read_be16(const uint8_t *p) { return uint16_t(read_be(p, 2)); }
constexpr uint32_t read_be32(const uint8_t *p) { return uint32_t(read_be(p, 4)); }
constexpr uint32_t read_le32(const uint8_t *p) { return uint32_t(read_le(p, 4)); }
constexpr uint64_t read_le64(const uint8_t *p) { return read_le(p, 8); }
constexpr void write_be16(uint8_t *p, uint16_t v) { write_be(p, v, 2); }
constexpr void write_be32(uint8_t *p, uint32_t v) { write_be(p, v, 4); }
constexpr void write_le32(uint8_t *p, uint32_t v) { write_le(p, v, 4); }
inline void append_be16(std::vector<uint8_t> &out, uint16_t v) { append_be(out, v, 2); }
inline void append_be32(std::vector<uint8_t> &out, uint32_t v) { append_be(out, v, 4); }
inline void append_le32(std::vector<uint8_t> &out, uint32_t v) { append_le(out, v, 4); }
inline void append_le64(std::vector<uint8_t> &out, uint64_t v) { append_le(out, v, 8); }
} // namespace xdb::util
