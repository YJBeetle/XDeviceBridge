#include "util/OutputFile.h"
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
namespace xdb::util {
std::FILE *create_output_file(const std::string &path, std::string &error) {
    error.clear();
    if (path.empty() || path.find('\0') != std::string::npos) {
        error = "Output requires a nonempty local file path without NUL bytes";
        return nullptr;
    }
    const std::filesystem::path native(std::u8string(path.begin(), path.end()));
    if (!native.is_absolute()) {
        error = "Output path must be absolute on the server host";
        return nullptr;
    }
#ifdef _WIN32
    const int fd = _wopen(native.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT,
                         _S_IREAD | _S_IWRITE);
#else
    const int fd = ::open(native.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
#endif
    if (fd < 0) {
        error = "Cannot create a new output file (existing paths are not overwritten): " +
                path + ": " + std::strerror(errno);
        return nullptr;
    }
#ifdef _WIN32
    auto *file = _fdopen(fd, "wb");
    if (!file) _close(fd);
#else
    auto *file = fdopen(fd, "wb");
    if (!file) ::close(fd);
#endif
    if (!file) error = "Cannot open the new output stream: " + std::string(std::strerror(errno));
    return file;
}
int64_t seek_output_file(std::FILE *file, int64_t offset, int whence) {
#ifdef _WIN32
    if (_fseeki64(file, offset, whence) != 0) return -1;
    return _ftelli64(file);
#else
    if (fseeko(file, offset, whence) != 0) return -1;
    return ftello(file);
#endif
}
}
