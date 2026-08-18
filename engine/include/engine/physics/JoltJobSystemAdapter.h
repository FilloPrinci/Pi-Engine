#pragma once

#include "engine/jobs/JobSystem.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemWithBarrier.h>

namespace engine::physics {

// Injects Jolt's jobs into our own engine::jobs::JobSystem instead of a second,
// independent thread pool (docs/01 section 9.3, CLAUDE.md rule 2) -- on a Cortex-A72 with
// no SMT, two schedulers competing for the same 4 cores would be pure waste. This is the
// one file in the engine that has to include Jolt's headers just to name the base class;
// everything else physics-related (physics/PhysicsWorld.h) hides Jolt behind
// unique_ptr<IncompleteType> members so the rest of the engine never needs Jolt on its
// include path.
//
// JPH::JobSystemWithBarrier already implements all of the Barrier bookkeeping (see its
// own header comment) -- only GetMaxConcurrency/CreateJob/FreeJob/QueueJob(s) are ours to
// provide, and QueueJob(s) is the only one that actually touches our JobSystem; the rest
// is bookkeeping around a plain `new`/`delete` Job allocation (docs/01 section 5: "no
// premature custom allocators" applies here too -- Jolt's own FixedSizeFreeList<Job> is a
// perf-motivated alternative, not a correctness requirement).
class JoltJobSystemAdapter final : public JPH::JobSystemWithBarrier {
public:
    explicit JoltJobSystemAdapter(jobs::JobSystem& jobSystem);
    ~JoltJobSystemAdapter() override = default;

    JoltJobSystemAdapter(const JoltJobSystemAdapter&) = delete;
    JoltJobSystemAdapter& operator=(const JoltJobSystemAdapter&) = delete;

    int GetMaxConcurrency() const override;
    JobHandle CreateJob(const char* name, JPH::ColorArg color, const JobFunction& jobFunction,
                        JPH::uint32 numDependencies = 0) override;

protected:
    void QueueJob(Job* job) override;
    void QueueJobs(Job** jobs, JPH::uint numJobs) override;
    void FreeJob(Job* job) override;

private:
    jobs::JobSystem& m_jobSystem;
};

} // namespace engine::physics
