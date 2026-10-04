#include "scene/MeshLoader.hpp"

#include <tinyobjloader/tiny_obj_loader.h>

#include <cstdio>
#include <format>
#include <stdexcept>

namespace pt {

namespace {

glm::vec3 position(const tinyobj::attrib_t& attrib, int index) {
    auto i = static_cast<size_t>(index) * 3;
    return {attrib.vertices[i], attrib.vertices[i + 1], attrib.vertices[i + 2]};
}

glm::vec3 fileNormal(const tinyobj::attrib_t& attrib, int index) {
    auto i = static_cast<size_t>(index) * 3;
    return {attrib.normals[i], attrib.normals[i + 1], attrib.normals[i + 2]};
}

} // namespace

std::vector<Triangle> loadObj(const std::filesystem::path& path, uint32_t material) {
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;
    config.vertex_color = false;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path.string(), config)) {
        throw std::runtime_error(
            std::format("Failed to load '{}': {}", path.string(), reader.Error()));
    }
    // Materials come from the scene preset, so missing .mtl files / usemtl names are expected.
    if (!reader.Warning().empty() && reader.Warning().find("aterial") == std::string::npos) {
        std::fprintf(stderr, "[obj] %s: %s\n", path.string().c_str(), reader.Warning().c_str());
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    // Area-weighted vertex normals: the unnormalised cross product is proportional to area.
    std::vector<glm::vec3> smoothNormals(attrib.vertices.size() / 3, glm::vec3(0.0f));
    for (const auto& shape : shapes) {
        const auto& indices = shape.mesh.indices;
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            glm::vec3 p0 = position(attrib, indices[i].vertex_index);
            glm::vec3 p1 = position(attrib, indices[i + 1].vertex_index);
            glm::vec3 p2 = position(attrib, indices[i + 2].vertex_index);
            glm::vec3 faceNormal = glm::cross(p1 - p0, p2 - p0);
            for (size_t k = 0; k < 3; ++k) {
                smoothNormals[static_cast<size_t>(indices[i + k].vertex_index)] += faceNormal;
            }
        }
    }

    std::vector<Triangle> triangles;
    for (const auto& shape : shapes) {
        const auto& indices = shape.mesh.indices;
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            glm::vec3 p[3];
            glm::vec3 n[3];
            for (size_t k = 0; k < 3; ++k) {
                const auto& index = indices[i + k];
                p[k] = position(attrib, index.vertex_index);
                n[k] = index.normal_index >= 0
                           ? fileNormal(attrib, index.normal_index)
                           : smoothNormals[static_cast<size_t>(index.vertex_index)];
            }

            glm::vec3 faceNormal = glm::cross(p[1] - p[0], p[2] - p[0]);
            if (glm::dot(faceNormal, faceNormal) == 0.0f) {
                continue; // degenerate triangle: zero area, can never be hit
            }
            for (auto& normal : n) {
                float length = glm::length(normal);
                normal = length > 0.0f ? normal / length : glm::normalize(faceNormal);
            }

            triangles.push_back({p[0], p[1], p[2], n[0], n[1], n[2], material});
        }
    }
    return triangles;
}

} // namespace pt
