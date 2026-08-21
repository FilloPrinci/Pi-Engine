#include "engine/scene/SceneDocument.h"

#include "engine/asset/AssetGuid.h"
#include "engine/ecs/World.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <unordered_map>

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

    if (json.contains("material")) {
        const auto& material = json.at("material");
        if (material.contains("guid") && material["guid"].is_string() &&
            asset::TryParseAssetGuid(material["guid"].get<std::string>(), desc.materialGuid)) {
            desc.hasMaterial = true;
        }
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

    if (json.contains("scripts")) {
        const auto& scripts = json.at("scripts");
        if (scripts.is_array()) {
            for (const auto& scriptNameJson : scripts) {
                if (scriptNameJson.is_string()) {
                    desc.scriptNames.push_back(scriptNameJson.get<std::string>());
                }
            }
        }
    }

    if (json.contains("parent") && json["parent"].is_number_integer()) {
        desc.parentIndex = json["parent"].get<int>();
    }

    return desc;
}

nlohmann::json WriteVec3(const glm::vec3& v) {
    return nlohmann::json::array({v.x, v.y, v.z});
}

nlohmann::json WriteQuat(const glm::quat& q) {
    // [x, y, z, w] -- inverse of ReadQuat()'s (w, x, y, z) glm::quat construction above.
    return nlohmann::json::array({q.x, q.y, q.z, q.w});
}

nlohmann::json WriteEntity(const EntityDesc& desc) {
    nlohmann::json entityJson;

    nlohmann::json transform;
    transform["position"] = WriteVec3(desc.position);
    transform["rotation"] = WriteQuat(desc.rotation);
    transform["scale"] = WriteVec3(desc.scale);
    entityJson["transform"] = transform;

    if (desc.hasMesh) {
        nlohmann::json mesh;
        mesh["guid"] = asset::ToString(desc.meshGuid);
        mesh["boundsRadius"] = desc.meshBoundsRadius;
        entityJson["mesh"] = mesh;
    }

    if (desc.hasMaterial) {
        nlohmann::json material;
        material["guid"] = asset::ToString(desc.materialGuid);
        entityJson["material"] = material;
    }

    if (desc.hasCollider) {
        nlohmann::json collider;
        collider["shape"] =
            desc.colliderShape == ecs::ColliderComponent::ShapeType::Sphere ? "sphere" : "box";
        collider["halfExtents"] = WriteVec3(desc.colliderHalfExtents);
        collider["radius"] = desc.colliderRadius;
        collider["isTrigger"] = desc.colliderIsTrigger;
        entityJson["collider"] = collider;
    }

    if (desc.hasRigidbody) {
        nlohmann::json rigidbody;
        rigidbody["isStatic"] = desc.rigidbodyIsStatic;
        rigidbody["mass"] = desc.rigidbodyMass;
        entityJson["rigidbody"] = rigidbody;
    }

    if (!desc.scriptNames.empty()) {
        entityJson["scripts"] = desc.scriptNames;
    }

    if (desc.parentIndex >= 0) {
        entityJson["parent"] = desc.parentIndex;
    }

    return entityJson;
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

bool WriteSceneDocument(const char* path, const std::vector<EntityDesc>& entities) {
    nlohmann::json document;
    document["_comment"] = "Written by the Pi-Engine Editor's Save action -- see "
                           "engine::scene::WriteSceneDocument.";

    nlohmann::json entitiesJson = nlohmann::json::array();
    for (const EntityDesc& desc : entities) {
        entitiesJson.push_back(WriteEntity(desc));
    }
    document["entities"] = entitiesJson;

    std::ofstream out(path);
    if (!out.is_open()) {
        std::fprintf(stderr, "WriteSceneDocument: failed to open \"%s\" for writing\n", path);
        return false;
    }
    out << document.dump(2);
    if (!out) {
        std::fprintf(stderr, "WriteSceneDocument: failed while writing \"%s\"\n", path);
        return false;
    }
    return true;
}

std::vector<EntityDesc> ExtractEntityDescs(const ecs::World& world) {
    std::vector<EntityDesc> entities;
    const auto& transformEntities = world.Transforms().Entities();
    const auto& transformData = world.Transforms().Data();
    entities.reserve(transformEntities.size());

    // Entity -> its own position in this output list, so a parent Entity can be translated
    // back into the EntityDesc::parentIndex SpawnEntities() will resolve it from later (see
    // this function's own header comment for why no separate id needs to survive the round
    // trip). Keyed by Entity::index alone, not the full Entity (index+generation) -- every
    // key here comes from `transformEntities`, i.e. entities alive *this instant*, so two
    // different generations of the same index can never both be present to collide; the
    // generation is exactly the part a plain index-based map would get wrong if it could.
    std::unordered_map<std::uint32_t, int> indexOf;
    indexOf.reserve(transformEntities.size());
    for (std::size_t i = 0; i < transformEntities.size(); ++i) {
        indexOf.emplace(transformEntities[i].index, static_cast<int>(i));
    }

    for (std::size_t i = 0; i < transformEntities.size(); ++i) {
        const ecs::Entity entity = transformEntities[i];
        EntityDesc desc;
        desc.position = transformData[i].position;
        desc.rotation = transformData[i].rotation;
        desc.scale = transformData[i].scale;

        if (transformData[i].parent != ecs::kInvalidEntity && world.IsAlive(transformData[i].parent)) {
            const auto parentIt = indexOf.find(transformData[i].parent.index);
            // Always found in practice once IsAlive() above is true (every alive entity
            // with a Transform is in indexOf) -- the fallback just avoids ever writing a
            // bogus index if that assumption is somehow violated.
            desc.parentIndex = parentIt != indexOf.end() ? parentIt->second : -1;
        }

        if (const ecs::MeshComponent* mesh = world.GetMesh(entity)) {
            desc.hasMesh = true;
            desc.meshGuid = mesh->meshGuid;
            desc.meshBoundsRadius = mesh->boundsRadius;

            if (mesh->materialGuid != asset::kInvalidAssetGuid) {
                desc.hasMaterial = true;
                desc.materialGuid = mesh->materialGuid;
            }
        }

        if (const ecs::ColliderComponent* collider = world.GetCollider(entity)) {
            desc.hasCollider = true;
            desc.colliderShape = collider->shapeType;
            desc.colliderHalfExtents = collider->halfExtents;
            desc.colliderRadius = collider->radius;
            desc.colliderIsTrigger = collider->isTrigger;
        }

        if (const ecs::RigidbodyComponent* rigidbody = world.GetRigidbody(entity)) {
            // isStatic now lives on the live component itself (post-Editor-E8,
            // RigidbodyComponent.h's own comment) -- fully recoverable here, unlike the
            // gap this exact spot used to document (a Rigidbody entity silently dropped on
            // save because isStatic was read once and discarded).
            desc.hasRigidbody = true;
            desc.rigidbodyIsStatic = rigidbody->isStatic;
            desc.rigidbodyMass = rigidbody->mass;
        }

        entities.push_back(desc);
    }

    return entities;
}

std::vector<ecs::Entity> SpawnEntities(ecs::World& world, const std::vector<EntityDesc>& entities,
                                       const glm::vec3& positionOffset,
                                       const CreatePhysicsBodyFn& createPhysicsBody,
                                       const AttachScriptFn& attachScript) {
    std::vector<ecs::Entity> spawned;
    spawned.reserve(entities.size());

    for (const EntityDesc& desc : entities) {
        const ecs::Entity entity = world.CreateEntity();

        ecs::TransformComponent transform;
        // positionOffset only ever applies to a root entity -- a non-root's position is
        // relative to its parent, not world space, see this function's own header comment.
        transform.position = desc.position + (desc.parentIndex < 0 ? positionOffset : glm::vec3(0.0f));
        transform.rotation = desc.rotation;
        transform.scale = desc.scale;
        world.AddTransform(entity, transform);

        if (desc.hasMesh) {
            ecs::MeshComponent mesh;
            mesh.meshGuid = desc.meshGuid;
            mesh.boundsRadius = desc.meshBoundsRadius;
            if (desc.hasMaterial) {
                mesh.materialGuid = desc.materialGuid;
            }
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
            rigidbody.isStatic = desc.rigidbodyIsStatic;
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

        if (!desc.scriptNames.empty() && attachScript) {
            for (const std::string& scriptName : desc.scriptNames) {
                attachScript(world, entity, scriptName);
            }
        }

        spawned.push_back(entity);
    }

    // Second pass: wire up TransformComponent::parent now that every entity in this batch
    // exists (a forward reference, e.g. entity 0 parented to entity 3, needs entity 3 to
    // already have been created -- see this function's own header comment).
    for (std::size_t i = 0; i < entities.size(); ++i) {
        const int parentIndex = entities[i].parentIndex;
        if (parentIndex < 0) {
            continue;
        }
        if (static_cast<std::size_t>(parentIndex) >= spawned.size() ||
            static_cast<std::size_t>(parentIndex) == i) {
            std::fprintf(stderr,
                         "SpawnEntities: entity %zu has an invalid parent index %d (valid "
                         "range: 0..%zu, excluding itself) -- treating it as a root entity\n",
                         i, parentIndex, spawned.size() - 1);
            continue;
        }
        if (ecs::TransformComponent* transform = world.GetTransform(spawned[i])) {
            transform->parent = spawned[static_cast<std::size_t>(parentIndex)];
        }
    }

    return spawned;
}

} // namespace engine::scene
