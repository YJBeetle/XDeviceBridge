#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace xdb::ddi::detail {
// Reads a bounded regular file. Throws on invalid paths or incomplete reads;
// callers still verify the selected asset's size and cryptographic digest.
std::string read_file(const std::filesystem::path &path, uint64_t limit);
} // namespace xdb::ddi::detail
