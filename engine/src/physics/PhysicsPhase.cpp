#include "engine/physics/PhysicsPhase.h"

#include "engine/physics/PhysicsWorld.h"

namespace engine::physics {

PhysicsPhase::PhysicsPhase(float fixedTimestepSeconds) : m_fixedTimestepSeconds(fixedTimestepSeconds) {}

void PhysicsPhase::Update(PhysicsWorld& physicsWorld, ecs::World& world, float deltaSeconds) {
    m_accumulatedSeconds += deltaSeconds;

    int steps = 0;
    while (m_accumulatedSeconds >= m_fixedTimestepSeconds && steps < kMaxStepsPerUpdate) {
        physicsWorld.Step(world, m_fixedTimestepSeconds);
        m_accumulatedSeconds -= m_fixedTimestepSeconds;
        ++steps;
    }

    if (steps == kMaxStepsPerUpdate) {
        m_accumulatedSeconds = 0.0f; // dropped the backlog rather than spiral further behind
    }
}

} // namespace engine::physics
