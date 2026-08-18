#include "engine/physics/PhysicsWorld.h"

#include "engine/ecs/World.h"
#include "engine/physics/JoltJobSystemAdapter.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/RegisterTypes.h>

#include <atomic>
#include <cstdio>

namespace engine::physics {

namespace {

// Two object layers is all M4 needs: a static, immovable ground and dynamic bodies that
// fall onto it (docs/02 section 4 exit criterion). Trigger volumes/character layers are
// later extensions (docs/01 section 9.6).
constexpr JPH::ObjectLayer kNonMovingLayer = 0;
constexpr JPH::ObjectLayer kMovingLayer = 1;
constexpr JPH::uint kNumObjectLayers = 2;
constexpr JPH::uint kNumBroadPhaseLayers = 2;

// Jolt's Factory/type registry is process-global state (JPH::Factory::sInstance), not
// per-PhysicsWorld -- reference-counted so multiple PhysicsWorld instances (e.g. across
// tests) register/unregister it exactly once each way.
std::atomic<int> g_joltRefCount{0};

JPH::Vec3 ToJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
JPH::Quat ToJolt(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
glm::vec3 FromJolt(JPH::Vec3Arg v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
glm::quat FromJolt(JPH::QuatArg q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

} // namespace

PhysicsWorld::PhysicsWorld() = default;

PhysicsWorld::~PhysicsWorld() {
    Shutdown();
}

bool PhysicsWorld::Init(jobs::JobSystem& jobSystem) {
    if (g_joltRefCount.fetch_add(1, std::memory_order_relaxed) == 0) {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }

    m_jobSystemAdapter = std::make_unique<JoltJobSystemAdapter>(jobSystem);
    // 10 MB scratch for the solver's per-step temporary allocations -- plenty for the
    // handful of bodies M4's sample creates; revisit only if a later milestone's scene
    // size actually exhausts it (JPH::TempAllocatorImpl asserts loudly if it does).
    m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

    m_broadPhaseLayerInterface =
        std::make_unique<JPH::BroadPhaseLayerInterfaceTable>(kNumObjectLayers, kNumBroadPhaseLayers);
    m_broadPhaseLayerInterface->MapObjectToBroadPhaseLayer(kNonMovingLayer, JPH::BroadPhaseLayer(0));
    m_broadPhaseLayerInterface->MapObjectToBroadPhaseLayer(kMovingLayer, JPH::BroadPhaseLayer(1));

    m_objectLayerPairFilter = std::make_unique<JPH::ObjectLayerPairFilterTable>(kNumObjectLayers);
    m_objectLayerPairFilter->EnableCollision(kNonMovingLayer, kMovingLayer);
    m_objectLayerPairFilter->EnableCollision(kMovingLayer, kMovingLayer);
    // NonMoving vs NonMoving stays disabled (the table's default): static bodies never
    // need to collide with each other.

    m_objectVsBroadPhaseLayerFilter = std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(
        *m_broadPhaseLayerInterface, kNumBroadPhaseLayers, *m_objectLayerPairFilter, kNumObjectLayers);

    m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    // Body/pair/constraint limits sized for a handful of entities (M4's actual scene) --
    // Jolt allocates these up front, so this is a fixed one-time cost, not a per-frame
    // bandwidth concern (CLAUDE.md section 3's checklist is about the GPU/memory-bandwidth
    // budget, not this kind of small, one-shot CPU-side allocation).
    m_physicsSystem->Init(/*maxBodies=*/1024, /*numBodyMutexes=*/0, /*maxBodyPairs=*/1024,
                          /*maxContactConstraints=*/1024, *m_broadPhaseLayerInterface,
                          *m_objectVsBroadPhaseLayerFilter, *m_objectLayerPairFilter);
    m_physicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
    return true;
}

void PhysicsWorld::Shutdown() {
    if (!m_physicsSystem) {
        return; // idempotent -- not initialized, or already shut down
    }

    m_physicsSystem.reset();
    m_objectVsBroadPhaseLayerFilter.reset();
    m_objectLayerPairFilter.reset();
    m_broadPhaseLayerInterface.reset();
    m_tempAllocator.reset();
    m_jobSystemAdapter.reset();

    if (g_joltRefCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

bool PhysicsWorld::CreateBody(ecs::World& world, ecs::Entity entity, bool isStatic) {
    const ecs::TransformComponent* transform = world.GetTransform(entity);
    const ecs::ColliderComponent* collider = world.GetCollider(entity);
    ecs::RigidbodyComponent* rigidbody = world.GetRigidbody(entity);
    if (transform == nullptr || collider == nullptr || rigidbody == nullptr) {
        std::fprintf(stderr, "PhysicsWorld::CreateBody: entity is missing Transform/Collider/"
                              "Rigidbody -- add all three before calling this\n");
        return false;
    }

    JPH::ShapeSettings::ShapeResult shapeResult;
    if (collider->shapeType == ecs::ColliderComponent::ShapeType::Box) {
        shapeResult = JPH::BoxShapeSettings(ToJolt(collider->halfExtents)).Create();
    } else {
        shapeResult = JPH::SphereShapeSettings(collider->radius).Create();
    }
    if (shapeResult.HasError()) {
        std::fprintf(stderr, "PhysicsWorld::CreateBody: shape creation failed: %s\n",
                      shapeResult.GetError().c_str());
        return false;
    }

    const JPH::EMotionType motionType = isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;
    const JPH::ObjectLayer objectLayer = isStatic ? kNonMovingLayer : kMovingLayer;
    JPH::BodyCreationSettings bodySettings(shapeResult.Get(), ToJolt(transform->position),
                                           ToJolt(transform->rotation), motionType, objectLayer);
    if (!isStatic) {
        bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        bodySettings.mMassPropertiesOverride.mMass = rigidbody->mass;
    }

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();
    JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    if (body == nullptr) {
        std::fprintf(stderr, "PhysicsWorld::CreateBody: CreateBody failed (max body count "
                              "reached?)\n");
        return false;
    }
    bodyInterface.AddBody(body->GetID(), isStatic ? JPH::EActivation::DontActivate
                                                   : JPH::EActivation::Activate);

    rigidbody->bodyId = body->GetID().GetIndexAndSequenceNumber();
    return true;
}

void PhysicsWorld::Step(float fixedDeltaSeconds) {
    constexpr int kCollisionSteps = 1; // one sub-step per fixed tick is enough at M4's scale
    m_physicsSystem->Update(fixedDeltaSeconds, kCollisionSteps, m_tempAllocator.get(),
                            m_jobSystemAdapter.get());
}

void PhysicsWorld::SyncTransforms(ecs::World& world) {
    // Read-only, no lock needed: this runs after Step() has returned, i.e. after the
    // synchronization barrier -- nothing else touches body state at this point (docs/01
    // section 9.4).
    const JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterfaceNoLock();

    auto& rigidbodies = world.Rigidbodies().Data();
    const auto& entities = world.Rigidbodies().Entities();
    for (std::size_t i = 0; i < rigidbodies.size(); ++i) {
        if (rigidbodies[i].bodyId == ecs::RigidbodyComponent::kInvalidBodyId) {
            continue;
        }
        ecs::TransformComponent* transform = world.GetTransform(entities[i]);
        if (transform == nullptr) {
            continue;
        }

        JPH::RVec3 position;
        JPH::Quat rotation;
        bodyInterface.GetPositionAndRotation(JPH::BodyID(rigidbodies[i].bodyId), position, rotation);
        transform->position = FromJolt(position);
        transform->rotation = FromJolt(rotation);
    }
}

void PhysicsWorld::SetGravity(const glm::vec3& gravity) {
    m_physicsSystem->SetGravity(ToJolt(gravity));
}

} // namespace engine::physics
