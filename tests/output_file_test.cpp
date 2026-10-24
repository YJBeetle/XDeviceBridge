#include "util/OutputFile.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("xdb-output-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(root);
    struct Cleanup { fs::path p; ~Cleanup() { std::error_code e; fs::remove_all(p,e); } } cleanup{root};
    const auto target = root / "existing";
    { std::ofstream stream(target); stream << "keep"; }
    std::string error;
    if (xdb::util::create_output_file(target.string(), error) || error.empty()) return 1;
    { std::ifstream stream(target); std::string text; stream >> text; if (text != "keep") return 2; }
    std::error_code ec;
    fs::create_symlink(target, root / "link", ec);
    if (!ec && xdb::util::create_output_file((root / "link").string(), error)) return 3;
    if (xdb::util::create_output_file("relative", error)) return 4;
    std::atomic<int> winners{0};
    const auto race = (root / "race").string();
    auto attempt = [&] { std::string message; if (auto* file = xdb::util::create_output_file(race,message)) {
        ++winners; std::fputs("new",file); std::fclose(file);
    }};
    std::thread a(attempt), b(attempt); a.join(); b.join();
    if (winners != 1) return 5;
    const auto unicode = root / fs::path(std::u8string(u8"录制-✓.mkv"));
    const auto utf8 = unicode.u8string();
    auto* file = xdb::util::create_output_file(std::string(utf8.begin(),utf8.end()),error);
    if (!file) return 6;
    std::fputs("abcdef",file);
    if (xdb::util::seek_output_file(file,2,SEEK_SET) != 2) return 7;
    std::fputs("Z",file); std::fclose(file);
    std::ifstream stream(unicode); std::string text; stream >> text;
    if (text != "abZdef") return 8;
    std::cout << "Existing output, symlink, concurrent create and Unicode seek checks passed\n";
}
