#pragma once

#include <cstddef>

namespace xdb {
// Bound reassembly before decoder/recorder admission, including streams without an end marker.
inline constexpr std::size_t kMaxHevcNalBytes = 8u * 1024u * 1024u;
inline constexpr std::size_t kMaxHevcAccessUnitBytes = 16u * 1024u * 1024u;
inline constexpr std::size_t kMaxHevcAccessUnitNals = 1024;
inline constexpr std::size_t kMaxHevcParameterBytes = 1024u * 1024u;
} // namespace xdb
