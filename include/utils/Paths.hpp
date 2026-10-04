#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace pt::paths {

// Directory containing the running executable. Shaders are built next to it, so the app works
// regardless of the working directory it is launched from (IDE, shell, double-click).
std::filesystem::path executableDir();

// <exe dir>/shaders/<name>
std::filesystem::path shader(std::string_view name);

// <exe dir>/assets/<name> if present, otherwise <source dir>/assets/<name>.
std::filesystem::path asset(std::string_view name);

// Reads a SPIR-V binary into 32-bit words (the alignment vkCreateShaderModule requires).
std::vector<uint32_t> readSpirv(const std::filesystem::path& path);

} // namespace pt::paths
