#include "engine/renderer/MeshLoader.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <cstdio>

namespace engine::renderer {

bool LoadMesh(const char* path, MeshData& outMesh) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success) {
        std::fprintf(stderr, "MeshLoader: cgltf_parse_file failed for \"%s\" (error %d)\n", path,
                     static_cast<int>(result));
        return false;
    }

    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success) {
        std::fprintf(stderr, "MeshLoader: cgltf_load_buffers failed for \"%s\" (error %d)\n", path,
                     static_cast<int>(result));
        cgltf_free(data);
        return false;
    }

    if (data->meshes_count == 0 || data->meshes[0].primitives_count == 0) {
        std::fprintf(stderr, "MeshLoader: \"%s\" has no mesh primitives\n", path);
        cgltf_free(data);
        return false;
    }

    const cgltf_primitive& primitive = data->meshes[0].primitives[0];

    const cgltf_accessor* positionAccessor = nullptr;
    const cgltf_accessor* normalAccessor = nullptr;
    for (cgltf_size i = 0; i < primitive.attributes_count; ++i) {
        const cgltf_attribute& attribute = primitive.attributes[i];
        if (attribute.type == cgltf_attribute_type_position) {
            positionAccessor = attribute.data;
        } else if (attribute.type == cgltf_attribute_type_normal) {
            normalAccessor = attribute.data;
        }
    }

    if (positionAccessor == nullptr) {
        std::fprintf(stderr, "MeshLoader: \"%s\" primitive has no POSITION attribute\n", path);
        cgltf_free(data);
        return false;
    }

    const cgltf_size vertexCount = positionAccessor->count;
    outMesh.vertices.resize(vertexCount);
    for (cgltf_size i = 0; i < vertexCount; ++i) {
        float pos[3] = {0.0f, 0.0f, 0.0f};
        cgltf_accessor_read_float(positionAccessor, i, pos, 3);
        outMesh.vertices[i].position = glm::vec3(pos[0], pos[1], pos[2]);

        if (normalAccessor != nullptr) {
            float normal[3] = {0.0f, 0.0f, 0.0f};
            cgltf_accessor_read_float(normalAccessor, i, normal, 3);
            outMesh.vertices[i].normal = glm::vec3(normal[0], normal[1], normal[2]);
        } else {
            outMesh.vertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    if (primitive.indices != nullptr) {
        const cgltf_size indexCount = primitive.indices->count;
        outMesh.indices.resize(indexCount);
        for (cgltf_size i = 0; i < indexCount; ++i) {
            outMesh.indices[i] =
                static_cast<std::uint32_t>(cgltf_accessor_read_index(primitive.indices, i));
        }
    } else {
        // No index buffer in the source: synthesize a trivial 0..N-1 list.
        outMesh.indices.resize(vertexCount);
        for (cgltf_size i = 0; i < vertexCount; ++i) {
            outMesh.indices[i] = static_cast<std::uint32_t>(i);
        }
    }

    cgltf_free(data);
    return true;
}

} // namespace engine::renderer
