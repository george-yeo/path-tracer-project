#include "utils/Paths.hpp"

#include <format>
#include <fstream>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifndef PT_SOURCE_ASSET_DIR
#define PT_SOURCE_ASSET_DIR "assets"
#endif

namespace pt::paths {

std::filesystem::path executableDir() {
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            break;
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    std::error_code error;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
        return exe.parent_path();
    }
#endif
    return std::filesystem::current_path();
}

std::filesystem::path shader(std::string_view name) {
    return executableDir() / "shaders" / name;
}

std::filesystem::path asset(std::string_view name) {
    auto nextToExe = executableDir() / "assets" / name;
    if (std::filesystem::exists(nextToExe)) {
        return nextToExe;
    }
    return std::filesystem::path(PT_SOURCE_ASSET_DIR) / name;
}

std::vector<uint32_t> readSpirv(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error(std::format("Failed to open shader '{}'", path.string()));
    }

    auto size = static_cast<size_t>(file.tellg());
    if (size == 0 || size % sizeof(uint32_t) != 0) {
        throw std::runtime_error(std::format("'{}' is not a valid SPIR-V binary", path.string()));
    }

    std::vector<uint32_t> words(size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(words.data()), static_cast<std::streamsize>(size));
    return words;
}

} // namespace pt::paths
