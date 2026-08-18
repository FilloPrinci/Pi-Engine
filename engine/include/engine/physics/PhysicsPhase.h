#pragma once

namespace engine::physics {

class PhysicsWorld;

// Fixed-timestep accumulator (docs/01 section 9.4: "physics runs at a fixed step,
// decoupled from the variable rendering framerate -- this also gives a predictable
// amount of work to distribute among the workers at each step"). Hooked in as the Physics
// phase in Application's loop (CLAUDE.md section 4), between Script and Post-Physics.
//
// Owns only the timing/accumulator state -- PhysicsWorld::Step() is where the actual
// synchronization barrier lives (that call blocking until it returns *is* the barrier,
// see PhysicsWorld.h's comment), this class just decides how many times to call it.
class PhysicsPhase {
public:
    explicit PhysicsPhase(float fixedTimestepSeconds = 1.0f / 60.0f);

    // Accumulates `deltaSeconds` and runs as many fixed-size PhysicsWorld::Step() calls as
    // have accrued. Call once per frame, after the Script phase and before Post-Physics.
    void Update(PhysicsWorld& physicsWorld, float deltaSeconds);

private:
    // Caps how many fixed steps a single Update() call will run -- without this, a long
    // stall (e.g. a breakpoint, a slow frame from window resize) would otherwise queue up
    // an ever-growing backlog of steps to "catch up", stalling every subsequent frame even
    // harder (the classic accumulator "spiral of death"). Past this cap, the excess
    // accumulated time is dropped instead of simulated.
    static constexpr int kMaxStepsPerUpdate = 4;

    float m_fixedTimestepSeconds;
    float m_accumulatedSeconds = 0.0f;
};

} // namespace engine::physics
