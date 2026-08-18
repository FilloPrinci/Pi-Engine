#include "engine/physics/JoltJobSystemAdapter.h"

#include <Jolt/Physics/PhysicsSettings.h> // JPH::cMaxPhysicsBarriers

namespace engine::physics {

JoltJobSystemAdapter::JoltJobSystemAdapter(jobs::JobSystem& jobSystem)
    : JPH::JobSystemWithBarrier(JPH::cMaxPhysicsBarriers), m_jobSystem(jobSystem) {}

int JoltJobSystemAdapter::GetMaxConcurrency() const {
    // +1: besides our worker threads, JobSystemWithBarrier::WaitForJobs() also executes
    // queued jobs on the calling thread while it waits (see that class's own comment) --
    // matches JPH::JobSystemThreadPool's own GetMaxConcurrency() (mThreads.size() + 1).
    return static_cast<int>(m_jobSystem.GetWorkerCount()) + 1;
}

JPH::JobSystem::JobHandle JoltJobSystemAdapter::CreateJob(const char* name, JPH::ColorArg color,
                                                          const JobFunction& jobFunction,
                                                          JPH::uint32 numDependencies) {
    Job* job = new Job(name, color, this, jobFunction, numDependencies);

    // Take the first reference before queueing (mirrors JPH::JobSystemThreadPool::
    // CreateJob()): if numDependencies == 0, QueueJob() below can run the job to
    // completion on another thread before this function even returns, and the handle
    // must already be holding a reference by then.
    JobHandle handle(job);
    if (numDependencies == 0) {
        QueueJob(job);
    }
    return handle;
}

void JoltJobSystemAdapter::QueueJob(Job* job) {
    // JobSystem::QueueJob's contract (JobSystem.h): the job is guaranteed to stay alive
    // for the duration of this call, but not after -- take our own reference for the
    // engine::jobs::JobSystem to hold until the job has actually executed.
    job->AddRef();
    m_jobSystem.Submit([job]() {
        job->Execute();
        job->Release();
    });
}

void JoltJobSystemAdapter::QueueJobs(Job** jobs, JPH::uint numJobs) {
    for (JPH::uint i = 0; i < numJobs; ++i) {
        QueueJob(jobs[i]);
    }
}

void JoltJobSystemAdapter::FreeJob(Job* job) {
    delete job;
}

} // namespace engine::physics
