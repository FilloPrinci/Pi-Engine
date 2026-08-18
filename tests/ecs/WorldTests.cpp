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
