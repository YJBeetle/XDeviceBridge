#pragma once
#include <cstdio>
#include <cstdint>
#include <string>
namespace xdb::util {
// Creates a new absolute local file atomically; existing files/symlinks are refused.
// The caller owns the returned stream and must fclose it.
std::FILE *create_output_file(const std::string &path, std::string &error);
int64_t seek_output_file(std::FILE *file, int64_t offset, int whence);
}
