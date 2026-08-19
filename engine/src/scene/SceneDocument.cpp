#include "engine/scene/SceneDocument.h"

#include "engine/asset/AssetGuid.h"
#include "engine/ecs/World.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>

namespace engine::scene {

namespace {

glm::vec3 ReadVec3(const nlohmann::json& json, const char* key, const glm::vec3& fallback) {
    if (!json.contains(key)) {
        return fallback;
    }
    const auto& array = json.at(key);
    if (!array.is_array() || array.size() != 3) {
        return fallback;
    }
    return glm::vec3(array[0].get<float>(), array[1].get<float>(), array[2].get<float>());
}

glm::quat ReadQuat(const nlohmann::json& json, const char* key, const glm::quat& fallback) {
    if (!json.contains(key)) {
        return fallback;
    }
    const auto& array = json.at(key);
    if (!array.is_array() || array.size() != 4) {
        return fallback;
    }
    // Authored as [x, y, z, w] (the common convention in glTF/most scene formats) --
    // glm::quat's own constructor takes (w, x, y, z), so the order flips here.
    return glm::quat(array[3].get<float>(), array[0].get<float>(), array[1].get<float>(),
                     array[2].get<float>());
}

EntityDesc ParseEntity(const nlohmann::json& json) {
    EntityDesc desc;

    if (json.contains("transform")) {
        const auto& transform = json.at("transform");
        desc.position = ReadVec3(transform, "position", desc.position);
        desc.rotation = ReadQuat(transform, "rotation", desc.rotation);
        desc.scale = ReadVec3(transform, "scale", desc.scale);
    }

    if (json.contains("mesh")) {
        const auto& mesh = json.at("mesh");
        if (mesh.contains("guid") && mesh["guid"].is_string() &&
            asset::TryParseAssetGuid(mesh["guid"].get<std::string>(), desc.meshGuid)) {
            desc.hasMesh = true;
        }
        desc.meshBoundsRadius = mesh.value("boundsRadius", desc.meshBoundsRadius);
    }

    if (json.contains("collider")) {
        const auto& collider = json.at("collider");
        desc.hasCollider = true;
        const std::string shape = collider.value("shape", std::string("box"));
        desc.colliderShape = (shape == "sphere") ? ecs::ColliderComponent::ShapeType::Sphere
                                                  : ecs::ColliderComponent::ShapeType::Box;
        desc.colliderHalfExtents = ReadVec3(collider, "halfExtents", desc.colliderHalfExtents);
        desc.colliderRadius = collider.value("radius", desc.colliderRadius);
        desc.colliderIsTrigger = collider.value("isTrigger", desc.colliderIsTrigger);
    }

    if (json.contains("rigidbody")) {
        const auto& rigidbody = json.at("rigidbody");
        desc.hasRigidbody = true;
        desc.rigidbodyIsStatic = rigidbody.value("isStatic", desc.rigidbodyIsStatic);
        desc.rigidbodyMass = rigidbody.value("mass", desc.rigidbodyMass);
    }

    return desc;
}

} // namespace

bool ParseSceneDocument(const char* path, std::vector<EntityDesc>& outEntities) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::fprintf(stderr, "ParseSceneDocument: failed to open \"%s\"\n", path);
        return false;
    }

    // A malformed/unexpected-shape document throws from inside nlohmann::json (parse
    // errors, wrong-type .get<T>() calls in ParseEntity/ReadVec3/ReadQuat above) --
    // caught here so a bad scene file becomes a clean `return false`, not a crash. Scene
    // loading runs once at a sample's startup, nowhere near hot-path code (CLAUDE.md
    // section 5's no-exceptions rule targets renderer/physics/job system specifically).
    try {
        nlohmann::json document;
        in >> document;

        if (!document.contains("entities") || !document["entities"].is_array()) {
            std::fprintf(stderr, "ParseSceneDocument: \"%s\" has no \"entities\" array\n", path);
            return false;
        }

        outEntities.clear();
        for (const auto& entityJson : document["entities"]) {
            outEntities.push_back(ParseEntity(entityJson));
        }
        return true;
    } catch (const nlohmann::json::exception& e) {
        std::fprintf(stderr, "ParseSceneDocument: \"%s\" is malformed: %s\n", path, e.what());
        return false;
    }
}

std::vector<ecs::Entity> SpawnEntities(ecs::World& world, const std::vector<EntityDesc>& entities,
                                       const glm::vec3& positionOffset,
                                       const CreatePhysicsBodyFn& createPhysicsBody) {
    std::vector<ecs::Entity> spawned;
    spawned.reserve(entities.size());

    for (const EntityDesc& desc : entities) {
        const ecs::Entity entity = world.CreateEntity();

        ecs::TransformComponent transform;
        transform.position = desc.position + positionOffset;
        transform.rotation = desc.rotation;
        transform.scale = desc.scale;
        world.AddTransform(entity, transform);

        if (desc.hasMesh) {
            ecs::MeshComponent mesh;
            mesh.meshGuid = desc.meshGuid;
            mesh.boundsRadius = desc.meshBoundsRadius;
            world.AddMesh(entity, mesh);
        }

        if (desc.hasCollider) {
            ecs::ColliderComponent collider;
            collider.shapeType = desc.colliderShape;
            collider.halfExtents = desc.colliderHalfExtents;
            collider.radius = desc.colliderRadius;
            collider.isTrigger = desc.colliderIsTrigger;
            world.AddCollider(entity, collider);
        }

        if (desc.hasRigidbody) {
            ecs::RigidbodyComponent rigidbody;
            rigidbody.mass = desc.rigidbodyMass;
            world.AddRigidbody(entity, rigidbody);

            if (!createPhysicsBody) {
                std::fprintf(stderr, "SpawnEntities: entity wants a rigidbody but no "
                                     "CreatePhysicsBodyFn was given -- added the ECS "
                                     "component only, no Jolt body\n");
            } else if (!desc.hasCollider) {
                std::fprintf(stderr, "SpawnEntities: entity has a \"rigidbody\" but no "
                                     "\"collider\" -- PhysicsWorld::CreateBody() needs "
                                     "both, no Jolt body created\n");
            } else {
                createPhysicsBody(world, entity, desc.rigidbodyIsStatic);
            }
        }

        spawned.push_back(entity);
    }

    return spawned;
}

} // namespace engine::scene
