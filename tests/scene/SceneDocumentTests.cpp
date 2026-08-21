#include "engine/scene/Prefab.h"
#include "engine/scene/Scene.h"
#include "engine/scene/SceneDocument.h"

#include "engine/ecs/World.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

using engine::asset::AssetGuid;
using engine::ecs::ColliderComponent;
using engine::ecs::Entity;
using engine::ecs::LightComponent;
using engine::ecs::MeshComponent;
using engine::ecs::RigidbodyComponent;
using engine::ecs::TransformComponent;
using engine::ecs::World;
using engine::scene::EntityDesc;
using engine::scene::ExtractEntityDescs;
using engine::scene::LoadScene;
using engine::scene::ParseSceneDocument;
using engine::scene::Prefab;
using engine::scene::SaveScene;
using engine::scene::SpawnEntities;
using engine::scene::WriteSceneDocument;

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
            "material": {"guid": "fedcba9876543210fedcba9876543210"},
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
    REQUIRE(first.hasMaterial);
    AssetGuid expectedMaterialGuid;
    REQUIRE(engine::asset::TryParseAssetGuid("fedcba9876543210fedcba9876543210",
                                             expectedMaterialGuid));
    CHECK(first.materialGuid == expectedMaterialGuid);
    REQUIRE(first.hasCollider);
    CHECK(first.colliderShape == ColliderComponent::ShapeType::Box);
    CHECK(first.colliderHalfExtents == glm::vec3(0.5f));
    REQUIRE(first.hasRigidbody);
    CHECK_FALSE(first.rigidbodyIsStatic);
    CHECK(first.rigidbodyMass == doctest::Approx(2.5f));

    const EntityDesc& second = entities[1];
    CHECK_FALSE(second.hasMesh);
    CHECK_FALSE(second.hasMaterial);
    CHECK_FALSE(second.hasRigidbody);
    REQUIRE(second.hasCollider);
    CHECK(second.colliderShape == ColliderComponent::ShapeType::Sphere);
    CHECK(second.colliderRadius == doctest::Approx(1.5f));
    CHECK(second.colliderIsTrigger);
}

TEST_CASE("ParseSceneDocument reads a \"scripts\" array of names") {
    ScopedTempFile file("scene_document_scripted.tmp.json", R"({
        "entities": [
            {
                "transform": {"position": [0.0, 0.0, 0.0]},
                "scripts": ["RotateScript", "SecondScript"]
            },
            {
                "transform": {"position": [1.0, 0.0, 0.0]}
            }
        ]
    })");

    std::vector<EntityDesc> entities;
    REQUIRE(ParseSceneDocument(file.m_path, entities));
    REQUIRE(entities.size() == 2);
    REQUIRE(entities[0].scriptNames.size() == 2);
    CHECK(entities[0].scriptNames[0] == "RotateScript");
    CHECK(entities[0].scriptNames[1] == "SecondScript");
    CHECK(entities[1].scriptNames.empty()); // no "scripts" block at all -- not an error
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

    AssetGuid materialGuid;
    REQUIRE(engine::asset::TryParseAssetGuid("fedcba9876543210fedcba9876543210", materialGuid));

    std::vector<EntityDesc> descs(1);
    descs[0].position = glm::vec3(1.0f, 0.0f, 0.0f);
    descs[0].hasMesh = true;
    descs[0].hasMaterial = true;
    descs[0].materialGuid = materialGuid;
    descs[0].hasCollider = true;
    descs[0].hasRigidbody = true;
    descs[0].rigidbodyIsStatic = false; // distinct from RigidbodyComponent's own default
                                        // (true), so this test actually exercises the
                                        // propagation, not just its presence.

    const std::vector<Entity> spawned = SpawnEntities(world, descs, glm::vec3(0.0f), nullptr);
    REQUIRE(spawned.size() == 1);

    const Entity entity = spawned[0];
    CHECK(world.IsAlive(entity));
    REQUIRE(world.HasTransform(entity));
    CHECK(world.GetTransform(entity)->position == glm::vec3(1.0f, 0.0f, 0.0f));
    CHECK(world.HasMesh(entity));
    CHECK(world.GetMesh(entity)->materialGuid == materialGuid);
    CHECK(world.HasCollider(entity));
    // No PhysicsWorld given -- the ECS component is still added (queued/degraded rather
    // than fatal, see SpawnEntities' own comment), just with no live Jolt body.
    REQUIRE(world.HasRigidbody(entity));
    CHECK(world.GetRigidbody(entity)->bodyId == RigidbodyComponent::kInvalidBodyId);
    CHECK_FALSE(world.GetRigidbody(entity)->isStatic);
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

TEST_CASE("SpawnEntities calls AttachScriptFn once per scriptName, in order") {
    World world;
    std::vector<EntityDesc> descs(1);
    descs[0].scriptNames = {"First", "Second"};

    std::vector<std::string> attachedNames;
    auto attachScript = [&](World&, Entity, const std::string& name) {
        attachedNames.push_back(name);
    };
    const std::vector<Entity> spawned =
        SpawnEntities(world, descs, glm::vec3(0.0f), {}, attachScript);
    REQUIRE(spawned.size() == 1);
    REQUIRE(attachedNames.size() == 2);
    CHECK(attachedNames[0] == "First");
    CHECK(attachedNames[1] == "Second");
}

TEST_CASE("SpawnEntities silently skips scriptNames when no AttachScriptFn is given") {
    World world;
    std::vector<EntityDesc> descs(1);
    descs[0].scriptNames = {"SomeScript"};

    // Must not crash/assert -- an empty AttachScriptFn is the documented default (a
    // read-only Scene View never wires one up, see SceneDocument.h's own comment).
    const std::vector<Entity> spawned = SpawnEntities(world, descs, glm::vec3(0.0f));
    REQUIRE(spawned.size() == 1);
    CHECK(world.IsAlive(spawned[0]));
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

TEST_CASE("ExtractEntityDescs/WriteSceneDocument round-trip preserves transform/mesh/collider/"
          "rigidbody") {
    ScopedTempFile sourceFile("scene_extract_source.tmp.json", kTwoEntityScene);
    World world;
    REQUIRE(LoadScene(sourceFile.m_path, world));

    const std::vector<EntityDesc> extracted = ExtractEntityDescs(world);
    REQUIRE(extracted.size() == 2);

    ScopedTempFile outputFile("scene_extract_output.tmp.json", "");
    REQUIRE(WriteSceneDocument(outputFile.m_path, extracted));

    std::vector<EntityDesc> reloaded;
    REQUIRE(ParseSceneDocument(outputFile.m_path, reloaded));
    REQUIRE(reloaded.size() == 2);

    const EntityDesc& first = reloaded[0];
    CHECK(first.position == glm::vec3(1.0f, 2.0f, 3.0f));
    CHECK(first.scale == glm::vec3(2.0f, 2.0f, 2.0f));
    REQUIRE(first.hasMesh);
    CHECK(first.meshBoundsRadius == doctest::Approx(0.87f));
    REQUIRE(first.hasMaterial);
    AssetGuid expectedMaterialGuid;
    REQUIRE(engine::asset::TryParseAssetGuid("fedcba9876543210fedcba9876543210",
                                             expectedMaterialGuid));
    CHECK(first.materialGuid == expectedMaterialGuid);
    REQUIRE(first.hasCollider);
    CHECK(first.colliderShape == ColliderComponent::ShapeType::Box);
    CHECK(first.colliderHalfExtents == glm::vec3(0.5f));
    // RigidbodyComponent::isStatic now lives on the live component (post-Editor-E8,
    // RigidbodyComponent.h's own comment) -- fully recoverable, unlike the gap this test
    // used to document.
    REQUIRE(first.hasRigidbody);
    CHECK_FALSE(first.rigidbodyIsStatic);
    CHECK(first.rigidbodyMass == doctest::Approx(2.5f));

    const EntityDesc& second = reloaded[1];
    CHECK_FALSE(second.hasMesh);
    CHECK_FALSE(second.hasMaterial);
    REQUIRE(second.hasCollider);
    CHECK(second.colliderShape == ColliderComponent::ShapeType::Sphere);
    CHECK(second.colliderRadius == doctest::Approx(1.5f));
    CHECK(second.colliderIsTrigger);
}

TEST_CASE("SaveScene persists a live edit made after LoadScene") {
    ScopedTempFile file("scene_save_roundtrip.tmp.json", kTwoEntityScene);

    World world;
    REQUIRE(LoadScene(file.m_path, world));

    TransformComponent* transform = world.GetTransform(world.Transforms().Entities()[0]);
    REQUIRE(transform != nullptr);
    transform->position = glm::vec3(42.0f, 0.0f, 0.0f); // simulates an Inspector edit (E3)

    REQUIRE(SaveScene(file.m_path, world));

    std::vector<EntityDesc> reloaded;
    REQUIRE(ParseSceneDocument(file.m_path, reloaded));
    REQUIRE(reloaded.size() == 2);
    CHECK(reloaded[0].position == glm::vec3(42.0f, 0.0f, 0.0f));
}

TEST_CASE("WriteSceneDocument round-trips a \"scripts\" array") {
    EntityDesc desc;
    desc.scriptNames = {"RotateScript"};

    ScopedTempFile file("scene_document_scripts_roundtrip.tmp.json", "");
    REQUIRE(WriteSceneDocument(file.m_path, {desc}));

    std::vector<EntityDesc> reloaded;
    REQUIRE(ParseSceneDocument(file.m_path, reloaded));
    REQUIRE(reloaded.size() == 1);
    REQUIRE(reloaded[0].scriptNames.size() == 1);
    CHECK(reloaded[0].scriptNames[0] == "RotateScript");
}

TEST_CASE("WriteSceneDocument omits \"scripts\" entirely when empty (no empty array noise)") {
    EntityDesc desc; // scriptNames left default-empty
    ScopedTempFile file("scene_document_no_scripts.tmp.json", "");
    REQUIRE(WriteSceneDocument(file.m_path, {desc}));

    std::ifstream in(file.m_path);
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(contents.find("scripts") == std::string::npos);
}

TEST_CASE("ExtractEntityDescs cannot recover scriptNames -- dropped on a Save round-trip, "
          "the one remaining gap of this shape (RigidbodyComponent::isStatic used to be "
          "another, now fixed)") {
    ScopedTempFile sourceFile("scene_scripts_extract_source.tmp.json", R"({
        "entities": [
            {"transform": {"position": [0.0, 0.0, 0.0]}, "scripts": ["RotateScript"]}
        ]
    })");
    World world;
    // No AttachScriptFn -- matches the Editor's own read-only Scene View, which never
    // attaches scripts either (see SceneDocument.h's ExtractEntityDescs comment).
    REQUIRE(LoadScene(sourceFile.m_path, world));

    const std::vector<EntityDesc> extracted = ExtractEntityDescs(world);
    REQUIRE(extracted.size() == 1);
    CHECK(extracted[0].scriptNames.empty());
}

TEST_CASE("WriteSceneDocument rejects an unwritable path") {
    const std::vector<EntityDesc> entities(1);
    CHECK_FALSE(WriteSceneDocument("/this/directory/does/not/exist/scene.json", entities));
}

// Hierarchy (post-Editor-E8, docs/07-unity-parity-analysis.md).

TEST_CASE("ParseSceneDocument reads a \"parent\" index, defaulting to -1 (no parent)") {
    ScopedTempFile file("scene_document_parented.tmp.json", R"({
        "entities": [
            {"transform": {"position": [0.0, 0.0, 0.0]}},
            {"transform": {"position": [1.0, 0.0, 0.0]}, "parent": 0}
        ]
    })");

    std::vector<EntityDesc> entities;
    REQUIRE(ParseSceneDocument(file.m_path, entities));
    REQUIRE(entities.size() == 2);
    CHECK(entities[0].parentIndex == -1);
    CHECK(entities[1].parentIndex == 0);
}

TEST_CASE("WriteSceneDocument round-trips a \"parent\" index, omitting it when -1") {
    EntityDesc root;
    EntityDesc child;
    child.parentIndex = 0;

    ScopedTempFile file("scene_document_parent_roundtrip.tmp.json", "");
    REQUIRE(WriteSceneDocument(file.m_path, {root, child}));

    std::vector<EntityDesc> reloaded;
    REQUIRE(ParseSceneDocument(file.m_path, reloaded));
    REQUIRE(reloaded.size() == 2);
    CHECK(reloaded[0].parentIndex == -1);
    CHECK(reloaded[1].parentIndex == 0);

    std::ifstream in(file.m_path);
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // Only the child's "parent" should appear -- the root's -1 is never written at all.
    CHECK(contents.find("\"parent\"") != std::string::npos);
    CHECK(contents.find("\"parent\": 0") != std::string::npos);
}

TEST_CASE("SpawnEntities wires up TransformComponent::parent from parentIndex, forward "
          "references included") {
    World world;
    std::vector<EntityDesc> descs(2);
    descs[0].parentIndex = 1; // forward reference -- entity 1 doesn't exist yet at this point
    descs[1].parentIndex = -1;

    const std::vector<Entity> spawned = SpawnEntities(world, descs, glm::vec3(0.0f));
    REQUIRE(spawned.size() == 2);
    CHECK(world.GetTransform(spawned[0])->parent == spawned[1]);
    CHECK(world.GetTransform(spawned[1])->parent == engine::ecs::kInvalidEntity);
}

TEST_CASE("SpawnEntities treats an out-of-range or self-referencing parentIndex as \"no "
          "parent\" instead of corrupting the entity") {
    World world;
    std::vector<EntityDesc> descs(2);
    descs[0].parentIndex = 99; // out of range
    descs[1].parentIndex = 1;  // self-reference

    const std::vector<Entity> spawned = SpawnEntities(world, descs, glm::vec3(0.0f));
    REQUIRE(spawned.size() == 2);
    CHECK(world.GetTransform(spawned[0])->parent == engine::ecs::kInvalidEntity);
    CHECK(world.GetTransform(spawned[1])->parent == engine::ecs::kInvalidEntity);
}

TEST_CASE("SpawnEntities only offsets a root entity's position, never a child's "
          "already-relative-to-parent one") {
    World world;
    std::vector<EntityDesc> descs(2);
    descs[0].position = glm::vec3(1.0f, 0.0f, 0.0f); // root
    descs[1].position = glm::vec3(2.0f, 0.0f, 0.0f); // child of descs[0]
    descs[1].parentIndex = 0;

    const std::vector<Entity> spawned =
        SpawnEntities(world, descs, glm::vec3(10.0f, 0.0f, 0.0f));
    CHECK(world.GetTransform(spawned[0])->position == glm::vec3(11.0f, 0.0f, 0.0f));
    // Not 12 -- positionOffset must not double-apply to the child's local position.
    CHECK(world.GetTransform(spawned[1])->position == glm::vec3(2.0f, 0.0f, 0.0f));
}

TEST_CASE("ExtractEntityDescs recovers parentIndex from live TransformComponent::parent "
          "state") {
    ScopedTempFile sourceFile("scene_parent_extract_source.tmp.json", R"({
        "entities": [
            {"transform": {"position": [0.0, 0.0, 0.0]}},
            {"transform": {"position": [1.0, 0.0, 0.0]}, "parent": 0}
        ]
    })");
    World world;
    REQUIRE(LoadScene(sourceFile.m_path, world));

    const std::vector<EntityDesc> extracted = ExtractEntityDescs(world);
    REQUIRE(extracted.size() == 2);
    CHECK(extracted[0].parentIndex == -1);
    CHECK(extracted[1].parentIndex == 0);
}

TEST_CASE("ParseSceneDocument reads a \"light\" block (Point, non-default fields)") {
    ScopedTempFile file("scene_document_light_point.tmp.json", R"({
        "entities": [
            {
                "transform": {"position": [0.0, 5.0, 0.0]},
                "light": {"type": "point", "color": [1.0, 0.5, 0.25], "intensity": 2.0,
                          "range": 15.0, "isStatic": true, "castsShadow": true}
            }
        ]
    })");

    std::vector<EntityDesc> entities;
    REQUIRE(ParseSceneDocument(file.m_path, entities));
    REQUIRE(entities.size() == 1);

    const EntityDesc& desc = entities[0];
    REQUIRE(desc.hasLight);
    CHECK(desc.lightType == LightComponent::Type::Point);
    CHECK(desc.lightColor == glm::vec3(1.0f, 0.5f, 0.25f));
    CHECK(desc.lightIntensity == doctest::Approx(2.0f));
    CHECK(desc.lightRange == doctest::Approx(15.0f));
    CHECK(desc.lightIsStatic);
    CHECK(desc.lightCastsShadow);
}

TEST_CASE("ParseSceneDocument defaults \"light\" to Directional, not static, no shadow") {
    ScopedTempFile file("scene_document_light_directional.tmp.json", R"({
        "entities": [
            {"transform": {"position": [0.0, 0.0, 0.0]}, "light": {}}
        ]
    })");

    std::vector<EntityDesc> entities;
    REQUIRE(ParseSceneDocument(file.m_path, entities));
    REQUIRE(entities.size() == 1);
    REQUIRE(entities[0].hasLight);
    CHECK(entities[0].lightType == LightComponent::Type::Directional);
    CHECK_FALSE(entities[0].lightIsStatic);
    CHECK_FALSE(entities[0].lightCastsShadow);
}

TEST_CASE("An entity with no \"light\" block has hasLight == false") {
    ScopedTempFile file("scene_document_no_light.tmp.json", R"({
        "entities": [{"transform": {"position": [0.0, 0.0, 0.0]}}]
    })");

    std::vector<EntityDesc> entities;
    REQUIRE(ParseSceneDocument(file.m_path, entities));
    REQUIRE(entities.size() == 1);
    CHECK_FALSE(entities[0].hasLight);
}

TEST_CASE("SpawnEntities adds a LightComponent matching a \"light\"-bearing EntityDesc") {
    World world;
    std::vector<EntityDesc> descs(1);
    descs[0].hasLight = true;
    descs[0].lightType = LightComponent::Type::Point;
    descs[0].lightColor = glm::vec3(0.2f, 0.4f, 0.6f);
    descs[0].lightIntensity = 3.0f;
    descs[0].lightRange = 8.0f;
    descs[0].lightIsStatic = true;
    descs[0].lightCastsShadow = true;

    const std::vector<Entity> spawned = SpawnEntities(world, descs, glm::vec3(0.0f));
    REQUIRE(spawned.size() == 1);
    REQUIRE(world.HasLight(spawned[0]));
    const LightComponent* light = world.GetLight(spawned[0]);
    CHECK(light->type == LightComponent::Type::Point);
    CHECK(light->color == glm::vec3(0.2f, 0.4f, 0.6f));
    CHECK(light->intensity == doctest::Approx(3.0f));
    CHECK(light->range == doctest::Approx(8.0f));
    CHECK(light->isStatic);
    CHECK(light->castsShadow);
}

TEST_CASE("ExtractEntityDescs/WriteSceneDocument round-trip preserves a \"light\" block") {
    ScopedTempFile sourceFile("scene_light_extract_source.tmp.json", R"({
        "entities": [
            {
                "transform": {"position": [0.0, 5.0, 0.0]},
                "light": {"type": "point", "color": [1.0, 0.5, 0.25], "intensity": 2.0,
                          "range": 15.0, "isStatic": true, "castsShadow": true}
            }
        ]
    })");
    World world;
    REQUIRE(LoadScene(sourceFile.m_path, world));

    const std::vector<EntityDesc> extracted = ExtractEntityDescs(world);
    REQUIRE(extracted.size() == 1);

    ScopedTempFile outputFile("scene_light_extract_output.tmp.json", "");
    REQUIRE(WriteSceneDocument(outputFile.m_path, extracted));

    std::vector<EntityDesc> reloaded;
    REQUIRE(ParseSceneDocument(outputFile.m_path, reloaded));
    REQUIRE(reloaded.size() == 1);
    REQUIRE(reloaded[0].hasLight);
    CHECK(reloaded[0].lightType == LightComponent::Type::Point);
    CHECK(reloaded[0].lightColor == glm::vec3(1.0f, 0.5f, 0.25f));
    CHECK(reloaded[0].lightIntensity == doctest::Approx(2.0f));
    CHECK(reloaded[0].lightRange == doctest::Approx(15.0f));
    CHECK(reloaded[0].lightIsStatic);
    CHECK(reloaded[0].lightCastsShadow);
}

TEST_CASE("WriteSceneDocument omits \"light\" entirely when the entity has none") {
    std::vector<EntityDesc> descs(1);
    ScopedTempFile file("scene_light_omitted.tmp.json", "");
    REQUIRE(WriteSceneDocument(file.m_path, descs));

    std::ifstream in(file.m_path);
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(contents.find("light") == std::string::npos);
}
