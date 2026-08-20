#include "engine/ecs/World.h"

#include <doctest/doctest.h>

using engine::ecs::Entity;
using engine::ecs::MeshComponent;
using engine::ecs::TransformComponent;
using engine::ecs::World;

TEST_CASE("CreateEntity returns distinct, alive entities") {
    World world;
    const Entity a = world.CreateEntity();
    const Entity b = world.CreateEntity();

    CHECK(a != b);
    CHECK(world.IsAlive(a));
    CHECK(world.IsAlive(b));
}

TEST_CASE("DestroyEntity invalidates the handle even after the index is recycled") {
    World world;
    const Entity first = world.CreateEntity();
    world.DestroyEntity(first);
    CHECK_FALSE(world.IsAlive(first));

    // A new entity is very likely to reuse `first`'s now-free index -- the old handle
    // must stay invalid (this is exactly what the generation counter is for).
    const Entity second = world.CreateEntity();
    CHECK(world.IsAlive(second));
    if (second.index == first.index) {
        CHECK(second.generation != first.generation);
    }
    CHECK_FALSE(world.IsAlive(first));
}

TEST_CASE("Add/Get/Has/Remove round-trip for TransformComponent") {
    World world;
    const Entity entity = world.CreateEntity();

    CHECK_FALSE(world.HasTransform(entity));
    CHECK(world.GetTransform(entity) == nullptr);

    TransformComponent transform;
    transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    world.AddTransform(entity, transform);

    CHECK(world.HasTransform(entity));
    REQUIRE(world.GetTransform(entity) != nullptr);
    CHECK(world.GetTransform(entity)->position == glm::vec3(1.0f, 2.0f, 3.0f));

    world.RemoveTransform(entity);
    CHECK_FALSE(world.HasTransform(entity));
    CHECK(world.GetTransform(entity) == nullptr);
}

TEST_CASE("Removing a component keeps the dense array contiguous (swap-and-pop)") {
    World world;
    Entity entities[3];
    for (int i = 0; i < 3; ++i) {
        entities[i] = world.CreateEntity();
        MeshComponent mesh;
        mesh.boundsRadius = static_cast<float>(i) + 1.0f; // 1, 2, 3 -- distinguishable
        world.AddMesh(entities[i], mesh);
    }
    REQUIRE(world.Meshes().Size() == 3);

    // Remove the middle one: swap-and-pop moves the last entry into its slot.
    world.RemoveMesh(entities[1]);
    CHECK(world.Meshes().Size() == 2);
    CHECK_FALSE(world.HasMesh(entities[1]));

    // The other two entities' components must still be reachable and correct.
    REQUIRE(world.GetMesh(entities[0]) != nullptr);
    CHECK(world.GetMesh(entities[0])->boundsRadius == doctest::Approx(1.0f));
    REQUIRE(world.GetMesh(entities[2]) != nullptr);
    CHECK(world.GetMesh(entities[2])->boundsRadius == doctest::Approx(3.0f));

    // Dense array/entity list must stay index-parallel after the swap.
    for (std::size_t i = 0; i < world.Meshes().Size(); ++i) {
        const engine::ecs::Entity& denseEntity = world.Meshes().Entities()[i];
        CHECK(world.GetMesh(denseEntity) == &world.Meshes().Data()[i]);
    }
}

TEST_CASE("DestroyEntity removes its components too") {
    World world;
    const Entity entity = world.CreateEntity();
    world.AddTransform(entity, {});
    world.AddMesh(entity, {});
    world.AddRigidbody(entity, {});
    world.AddCollider(entity, {});
    REQUIRE(world.HasTransform(entity));
    REQUIRE(world.HasMesh(entity));
    REQUIRE(world.HasRigidbody(entity));
    REQUIRE(world.HasCollider(entity));

    world.DestroyEntity(entity);

    CHECK_FALSE(world.HasTransform(entity));
    CHECK_FALSE(world.HasMesh(entity));
    CHECK_FALSE(world.HasRigidbody(entity));
    CHECK_FALSE(world.HasCollider(entity));
}

TEST_CASE("Add/Get/Has/Remove round-trip for RigidbodyComponent and ColliderComponent") {
    World world;
    const Entity entity = world.CreateEntity();

    engine::ecs::RigidbodyComponent rigidbody;
    rigidbody.bodyId = 42;
    rigidbody.mass = 2.5f;
    world.AddRigidbody(entity, rigidbody);
    REQUIRE(world.HasRigidbody(entity));
    CHECK(world.GetRigidbody(entity)->bodyId == 42);
    CHECK(world.GetRigidbody(entity)->mass == doctest::Approx(2.5f));

    engine::ecs::ColliderComponent collider;
    collider.shapeType = engine::ecs::ColliderComponent::ShapeType::Sphere;
    collider.radius = 1.5f;
    world.AddCollider(entity, collider);
    REQUIRE(world.HasCollider(entity));
    CHECK(world.GetCollider(entity)->shapeType == engine::ecs::ColliderComponent::ShapeType::Sphere);
    CHECK(world.GetCollider(entity)->radius == doctest::Approx(1.5f));

    world.RemoveRigidbody(entity);
    world.RemoveCollider(entity);
    CHECK_FALSE(world.HasRigidbody(entity));
    CHECK_FALSE(world.HasCollider(entity));
}

// Hierarchy (post-Editor-E8, docs/07-unity-parity-analysis.md).

TEST_CASE("GetWorldMatrix returns the local matrix unchanged for a root entity") {
    World world;
    const Entity entity = world.CreateEntity();
    TransformComponent transform;
    transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    world.AddTransform(entity, transform);

    CHECK(world.GetWorldMatrix(entity) == world.GetTransform(entity)->GetMatrix());
}

TEST_CASE("GetWorldMatrix composes a child's world position through its parent's local "
          "translation") {
    World world;
    const Entity parent = world.CreateEntity();
    TransformComponent parentTransform;
    parentTransform.position = glm::vec3(10.0f, 0.0f, 0.0f);
    world.AddTransform(parent, parentTransform);

    const Entity child = world.CreateEntity();
    TransformComponent childTransform;
    childTransform.position = glm::vec3(1.0f, 0.0f, 0.0f);
    childTransform.parent = parent;
    world.AddTransform(child, childTransform);

    const glm::vec3 childWorldPosition = glm::vec3(world.GetWorldMatrix(child)[3]);
    CHECK(childWorldPosition == glm::vec3(11.0f, 0.0f, 0.0f));
}

TEST_CASE("GetWorldMatrix composes through a multi-level parent chain") {
    World world;
    const Entity grandparent = world.CreateEntity();
    TransformComponent grandparentTransform;
    grandparentTransform.position = glm::vec3(100.0f, 0.0f, 0.0f);
    world.AddTransform(grandparent, grandparentTransform);

    const Entity parent = world.CreateEntity();
    TransformComponent parentTransform;
    parentTransform.position = glm::vec3(10.0f, 0.0f, 0.0f);
    parentTransform.parent = grandparent;
    world.AddTransform(parent, parentTransform);

    const Entity child = world.CreateEntity();
    TransformComponent childTransform;
    childTransform.position = glm::vec3(1.0f, 0.0f, 0.0f);
    childTransform.parent = parent;
    world.AddTransform(child, childTransform);

    const glm::vec3 childWorldPosition = glm::vec3(world.GetWorldMatrix(child)[3]);
    CHECK(childWorldPosition == glm::vec3(111.0f, 0.0f, 0.0f));
}

TEST_CASE("GetWorldMatrix doesn't hang on a cyclic parent chain") {
    World world;
    const Entity a = world.CreateEntity();
    const Entity b = world.CreateEntity();
    world.AddTransform(a, TransformComponent{});
    world.AddTransform(b, TransformComponent{});
    world.GetTransform(a)->parent = b;
    world.GetTransform(b)->parent = a; // a genuine cycle -- must terminate, not loop forever.

    // No crash/hang is the actual assertion here; the exact resulting matrix isn't
    // meaningful for a malformed cycle, just that GetWorldMatrix() returns at all.
    (void)world.GetWorldMatrix(a);
    CHECK(true);
}

TEST_CASE("IsDescendantOf finds an entity anywhere in the parent chain, direct or "
          "transitive") {
    World world;
    const Entity grandparent = world.CreateEntity();
    const Entity parent = world.CreateEntity();
    const Entity child = world.CreateEntity();
    const Entity unrelated = world.CreateEntity();
    world.AddTransform(grandparent, TransformComponent{});
    world.AddTransform(parent, TransformComponent{});
    world.AddTransform(child, TransformComponent{});
    world.AddTransform(unrelated, TransformComponent{});
    world.GetTransform(parent)->parent = grandparent;
    world.GetTransform(child)->parent = parent;

    CHECK(world.IsDescendantOf(child, parent));
    CHECK(world.IsDescendantOf(child, grandparent)); // transitive, not just direct
    CHECK_FALSE(world.IsDescendantOf(child, unrelated));
    CHECK_FALSE(world.IsDescendantOf(parent, child)); // wrong direction
}
