#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace engine::jobs {

// Work-stealing task graph (docs/01 section 9.3/4): worker threads with mutex-protected
// per-worker deques (not lock-free -- correct and simple beats fast-but-subtle for the
// engine's first real parallel workload; revisit only if profiling on real hardware shows
// contention actually matters). Idle workers steal from the back of a busy worker's deque
// while that worker pops from its own front, minimizing collisions between the two ends.
//
// Culling (M2), physics (M4, via physics/JoltJobSystemAdapter), and later animation all
// share this one scheduler -- never a second, independent thread pool (CLAUDE.md rule 2).
class JobSystem {
public:
    JobSystem() = default;
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // `workerCount` == 0 picks `hardware_concurrency() - 1` (docs/01 section 9.3: leaves
    // one core for the calling/main thread), clamped to at least 1.
    bool Init(std::uint32_t workerCount = 0);
    void Shutdown();

    // Splits [0, itemCount) into up to GetWorkerCount() contiguous chunks, runs
    // `fn(beginIndex, endIndex)` for each chunk across the worker pool, and blocks the
    // calling thread until every chunk has finished. The one parallel-work primitive M2
    // needs (frustum culling, renderer/FrustumCuller.cpp); a general job-graph/dependency
    // API can grow out of this later if a milestone actually needs one.
    void ParallelFor(std::size_t itemCount, const std::function<void(std::size_t, std::size_t)>& fn);

    std::uint32_t GetWorkerCount() const { return static_cast<std::uint32_t>(m_workers.size()); }

private:
    using JobFunction = std::function<void()>;

    struct WorkerQueue {
        std::deque<JobFunction> jobs;
        std::mutex mutex;
    };

    void WorkerLoop(std::uint32_t workerIndex);
    bool TryPopLocal(std::uint32_t workerIndex, JobFunction& outJob);
    bool TrySteal(std::uint32_t workerIndex, JobFunction& outJob);
    void PushJob(std::uint32_t workerIndex, JobFunction job);

    std::vector<std::thread> m_workers;
    std::vector<std::unique_ptr<WorkerQueue>> m_queues;

    std::mutex m_wakeMutex;
    std::condition_variable m_wakeCondition;
    std::atomic<bool> m_shuttingDown{false};
    std::atomic<std::uint32_t> m_pendingJobCount{0}; // wakes idle workers only when > 0
};

} // namespace engine::jobs
