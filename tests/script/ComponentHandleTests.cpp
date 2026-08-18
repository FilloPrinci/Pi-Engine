#include "engine/ecs/World.h"
#include "engine/script/ComponentHandle.h"

#include <doctest/doctest.h>

using engine::ecs::Entity;
using engine::ecs::MeshComponent;
using engine::ecs::TransformComponent;
using engine::ecs::World;
using engine::script::ComponentHandle;

TEST_CASE("ComponentHandle resolves to the live component") {
    World world;
    const Entity entity = world.CreateEntity();
    TransformComponent transform;
    transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    world.AddTransform(entity, transform);

    ComponentHandle<TransformComponent> handle(world, entity);
    REQUIRE(handle.IsValid());
    CHECK(handle->position == glm::vec3(1.0f, 2.0f, 3.0f));

    handle->position = glm::vec3(4.0f, 5.0f, 6.0f);
    CHECK(world.GetTransform(entity)->position == glm::vec3(4.0f, 5.0f, 6.0f));
}

TEST_CASE("ComponentHandle stays correct across a swap-and-pop rearrangement") {
    // This is exactly the scenario ComponentHandle<T> exists for (docs/01 section 6.2): a
    // handle cached once (like a script would in OnStart) must keep pointing at the right
    // component even after ComponentStorage<T>::Remove() moves memory around underneath it.
    World world;
    Entity entities[3];
    for (int i = 0; i < 3; ++i) {
        entities[i] = world.CreateEntity();
        MeshComponent mesh;
        mesh.boundsRadius = static_cast<float>(i) + 1.0f;
        world.AddMesh(entities[i], mesh);
    }

    ComponentHandle<MeshComponent> lastHandle(world, entities[2]);
    REQUIRE(lastHandle->boundsRadius == doctest::Approx(3.0f));

    // Removing the first entity's component swaps the last dense entry (entities[2]'s
    // MeshComponent) into slot 0 -- lastHandle must still resolve to the right data.
    world.RemoveMesh(entities[0]);
    REQUIRE(lastHandle.IsValid());
    CHECK(lastHandle->boundsRadius == doctest::Approx(3.0f));
}

TEST_CASE("ComponentHandle is invalid once the entity is destroyed") {
    World world;
    const Entity entity = world.CreateEntity();
    world.AddTransform(entity, {});

    ComponentHandle<TransformComponent> handle(world, entity);
    REQUIRE(handle.IsValid());

    world.DestroyEntity(entity);
    CHECK_FALSE(handle.IsValid());
}

TEST_CASE("A default-constructed ComponentHandle is never valid") {
    ComponentHandle<TransformComponent> handle;
    CHECK_FALSE(handle.IsValid());
}
