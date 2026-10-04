#pragma once

#include "scene/Scene.hpp"

#include <filesystem>
#include <vector>

namespace pt {

// Loads an OBJ file as triangles with the given material. Uses the file's normals when present,
// otherwise computes area-weighted smooth vertex normals. Throws on failure.
std::vector<Triangle> loadObj(const std::filesystem::path& path, uint32_t material);

} // namespace pt
