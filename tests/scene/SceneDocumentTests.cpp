#include "engine/scene/Prefab.h"
#include "engine/scene/Scene.h"
#include "engine/scene/SceneDocument.h"

#include "engine/ecs/World.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <fstream>

using engine::asset::AssetGuid;
using engine::ecs::ColliderComponent;
using engine::ecs::Entity;
using engine::ecs::MeshComponent;
using engine::ecs::RigidbodyComponent;
using engine::ecs::TransformComponent;
using engine::ecs::World;
using engine::scene::EntityDesc;
using engine::scene::LoadScene;
using engine::scene::ParseSceneDocument;
using engine::scene::Prefab;
using engine::scene::SpawnEntities;

namespace {

struct ScopedTempFile {
    explicit ScopedTempFile(const char* path, const char* contents) : m_path(path) {
        std::ofstream out(path);
        out << contents;
    }
    ~ScopedTempFile() { std::remove(m_path); }
    const char* m_path;
};

constexpr const char* kTwoEntityScene = R"({
    "entities": [
        {
            "transform": {"position": [1.0, 2.0, 3.0], "scale": [2.0, 2.0, 2.0]},
            "mesh": {"guid": "0123456789abcdef0123456789abcdef", "boundsRadius": 0.87},
            "collider": {"shape": "box", "halfExtents": [0.5, 0.5, 0.5]},
            "rigidbody": {"isStatic": false, "mass": 2.5}
        },
        {
            "collider": {"shape": "sphere", "radius": 1.5, "isTrigger": true}
        }
    ]
})";

} // namespace

TEST_CASE("ParseSceneDocument reads transform, mesh, collider, and rigidbody fields") {
    ScopedTempFile file("scene_document_two_entities.tmp.json", kTwoEntityScene);

    std::vector<EntityDesc> entities;
    REQUIRE(ParseSceneDocument(file.m_path, entities));
    REQUIRE(entities.size() == 2);

    const EntityDesc& first = entities[0];
    CHECK(first.position == glm::vec3(1.0f, 2.0f, 3.0f));
    CHECK(first.scale == glm::vec3(2.0f, 2.0f, 2.0f));
    REQUIRE(first.hasMesh);
    AssetGuid expectedGuid;
    REQUIRE(engine::asset::TryParseAssetGuid("0123456789abcdef0123456789abcdef", expectedGuid));
    CHECK(first.meshGuid == expectedGuid);
    CHECK(first.meshBoundsRadius == doctest::Approx(0.87f));
    REQUIRE(first.hasCollider);
    CHECK(first.colliderShape == ColliderComponent::ShapeType::Box);
    CHECK(first.colliderHalfExtents == glm::vec3(0.5f));
    REQUIRE(first.hasRigidbody);
    CHECK_FALSE(first.rigidbodyIsStatic);
    CHECK(first.rigidbodyMass == doctest::Approx(2.5f));

    const EntityDesc& second = entities[1];
    CHECK_FALSE(second.hasMesh);
    CHECK_FALSE(second.hasRigidbody);
    REQUIRE(second.hasCollider);
    CHECK(second.colliderShape == ColliderComponent::ShapeType::Sphere);
    CHECK(second.colliderRadius == doctest::Approx(1.5f));
    CHECK(second.colliderIsTrigger);
}

TEST_CASE("ParseSceneDocument rejects a missing file") {
    std::vector<EntityDesc> entities;
    CHECK_FALSE(ParseSceneDocument("this_scene_does_not_exist.json", entities));
}

TEST_CASE("ParseSceneDocument rejects malformed JSON") {
    ScopedTempFile file("scene_document_garbage.tmp.json", "{ not valid json");
    std::vector<EntityDesc> entities;
    CHECK_FALSE(ParseSceneDocument(file.m_path, entities));
}

TEST_CASE("ParseSceneDocument rejects a document with no \"entities\" array") {
    ScopedTempFile file("scene_document_no_entities.tmp.json", R"({"foo": "bar"})");
    std::vector<EntityDesc> entities;
    CHECK_FALSE(ParseSceneDocument(file.m_path, entities));
}

TEST_CASE("SpawnEntities creates ECS components matching each EntityDesc, no PhysicsWorld") {
    World world;

    std::vector<EntityDesc> descs(1);
    descs[0].position = glm::vec3(1.0f, 0.0f, 0.0f);
    descs[0].hasMesh = true;
    descs[0].hasCollider = true;
    descs[0].hasRigidbody = true;

    const std::vector<Entity> spawned = SpawnEntities(world, descs, glm::vec3(0.0f), nullptr);
    REQUIRE(spawned.size() == 1);

    const Entity entity = spawned[0];
    CHECK(world.IsAlive(entity));
    REQUIRE(world.HasTransform(entity));
    CHECK(world.GetTransform(entity)->position == glm::vec3(1.0f, 0.0f, 0.0f));
    CHECK(world.HasMesh(entity));
    CHECK(world.HasCollider(entity));
    // No PhysicsWorld given -- the ECS component is still added (queued/degraded rather
    // than fatal, see SpawnEntities' own comment), just with no live Jolt body.
    REQUIRE(world.HasRigidbody(entity));
    CHECK(world.GetRigidbody(entity)->bodyId == RigidbodyComponent::kInvalidBodyId);
}

TEST_CASE("SpawnEntities applies positionOffset on top of each EntityDesc's own position") {
    World world;
    std::vector<EntityDesc> descs(1);
    descs[0].position = glm::vec3(1.0f, 0.0f, 0.0f);

    const std::vector<Entity> spawned =
        SpawnEntities(world, descs, glm::vec3(10.0f, 0.0f, 0.0f), nullptr);
    REQUIRE(spawned.size() == 1);
    CHECK(world.GetTransform(spawned[0])->position == glm::vec3(11.0f, 0.0f, 0.0f));
}

TEST_CASE("LoadScene spawns every entity described in the file") {
    ScopedTempFile file("scene_load.tmp.json", kTwoEntityScene);
    World world;
    REQUIRE(LoadScene(file.m_path, world));
    CHECK(world.Meshes().Size() == 1); // only the first entity has a "mesh" block
    CHECK(world.Colliders().Size() == 2);
}

TEST_CASE("Prefab::Instantiate can be called more than once, each at its own position") {
    ScopedTempFile file("prefab_cube.tmp.json", R"({
        "entities": [ { "transform": {"position": [0.0, 0.0, 0.0]}, "mesh": {"guid": "0123456789abcdef0123456789abcdef"} } ]
    })");

    Prefab prefab;
    REQUIRE(prefab.Load(file.m_path));
    CHECK(prefab.IsLoaded());

    World world;
    const std::vector<Entity> first = prefab.Instantiate(world, glm::vec3(5.0f, 0.0f, 0.0f));
    const std::vector<Entity> second = prefab.Instantiate(world, glm::vec3(-5.0f, 0.0f, 0.0f));

    REQUIRE(first.size() == 1);
    REQUIRE(second.size() == 1);
    CHECK(first[0] != second[0]); // two distinct entities, not the same one reused
    CHECK(world.GetTransform(first[0])->position == glm::vec3(5.0f, 0.0f, 0.0f));
    CHECK(world.GetTransform(second[0])->position == glm::vec3(-5.0f, 0.0f, 0.0f));
    CHECK(world.Meshes().Size() == 2);
}

TEST_CASE("A Prefab that failed to load is not IsLoaded()") {
    Prefab prefab;
    CHECK_FALSE(prefab.Load("this_prefab_does_not_exist.json"));
    CHECK_FALSE(prefab.IsLoaded());
}
