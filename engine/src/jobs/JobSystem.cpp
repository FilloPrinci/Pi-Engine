#include "engine/jobs/JobSystem.h"

#include <algorithm>

namespace engine::jobs {

JobSystem::~JobSystem() {
    Shutdown();
}

bool JobSystem::Init(std::uint32_t workerCount) {
    if (workerCount == 0) {
        const unsigned int hc = std::thread::hardware_concurrency();
        workerCount = (hc > 1) ? (hc - 1) : 1; // leave one core for the caller (docs/01 9.3)
    }

    m_queues.reserve(workerCount);
    for (std::uint32_t i = 0; i < workerCount; ++i) {
        m_queues.push_back(std::make_unique<WorkerQueue>());
    }

    m_workers.reserve(workerCount);
    for (std::uint32_t i = 0; i < workerCount; ++i) {
        m_workers.emplace_back(&JobSystem::WorkerLoop, this, i);
    }
    return true;
}

void JobSystem::Shutdown() {
    if (m_workers.empty()) {
        return;
    }
    m_shuttingDown.store(true, std::memory_order_release);
    m_wakeCondition.notify_all();
    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
    m_queues.clear();
    m_shuttingDown.store(false, std::memory_order_release);
}

void JobSystem::ParallelFor(std::size_t itemCount,
                             const std::function<void(std::size_t, std::size_t)>& fn) {
    if (itemCount == 0) {
        return;
    }

    const std::uint32_t workerCount = GetWorkerCount();
    if (workerCount == 0) {
        // Not initialized: fall back to running inline rather than silently doing nothing.
        fn(0, itemCount);
        return;
    }

    const std::size_t chunkCount = std::min<std::size_t>(workerCount, itemCount);
    const std::size_t chunkSize = (itemCount + chunkCount - 1) / chunkCount;

    std::atomic<std::size_t> remaining{chunkCount};
    std::mutex doneMutex;
    std::condition_variable doneCondition;

    for (std::size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
        const std::size_t begin = chunkIndex * chunkSize;
        const std::size_t end = std::min(begin + chunkSize, itemCount);

        PushJob(static_cast<std::uint32_t>(chunkIndex % workerCount),
                [&fn, begin, end, &remaining, &doneMutex, &doneCondition]() {
                    fn(begin, end);
                    if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        std::lock_guard<std::mutex> lock(doneMutex);
                        doneCondition.notify_all();
                    }
                });
    }

    std::unique_lock<std::mutex> lock(doneMutex);
    doneCondition.wait(lock, [&remaining] { return remaining.load(std::memory_order_acquire) == 0; });
}

void JobSystem::Submit(std::function<void()> job) {
    const std::uint32_t workerCount = GetWorkerCount();
    if (workerCount == 0) {
        // Not initialized: run inline rather than dropping it (mirrors ParallelFor's
        // own not-initialized fallback).
        job();
        return;
    }

    const std::uint32_t index = m_nextSubmitWorker.fetch_add(1, std::memory_order_relaxed) % workerCount;
    PushJob(index, std::move(job));
}

void JobSystem::WorkerLoop(std::uint32_t workerIndex) {
    while (!m_shuttingDown.load(std::memory_order_acquire)) {
        JobFunction job;
        if (TryPopLocal(workerIndex, job) || TrySteal(workerIndex, job)) {
            job();
        } else {
            std::unique_lock<std::mutex> lock(m_wakeMutex);
            m_wakeCondition.wait(lock, [this] {
                return m_shuttingDown.load(std::memory_order_acquire) ||
                       m_pendingJobCount.load(std::memory_order_acquire) > 0;
            });
        }
    }
}

bool JobSystem::TryPopLocal(std::uint32_t workerIndex, JobFunction& outJob) {
    WorkerQueue& queue = *m_queues[workerIndex];
    std::lock_guard<std::mutex> lock(queue.mutex);
    if (queue.jobs.empty()) {
        return false;
    }
    outJob = std::move(queue.jobs.front());
    queue.jobs.pop_front();
    m_pendingJobCount.fetch_sub(1, std::memory_order_relaxed);
    return true;
}

bool JobSystem::TrySteal(std::uint32_t workerIndex, JobFunction& outJob) {
    const std::uint32_t count = static_cast<std::uint32_t>(m_queues.size());
    for (std::uint32_t offset = 1; offset < count; ++offset) {
        const std::uint32_t victim = (workerIndex + offset) % count;
        WorkerQueue& queue = *m_queues[victim];
        std::lock_guard<std::mutex> lock(queue.mutex);
        if (!queue.jobs.empty()) {
            outJob = std::move(queue.jobs.back());
            queue.jobs.pop_back();
            m_pendingJobCount.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

void JobSystem::PushJob(std::uint32_t workerIndex, JobFunction job) {
    {
        WorkerQueue& queue = *m_queues[workerIndex];
        std::lock_guard<std::mutex> lock(queue.mutex);
        queue.jobs.push_back(std::move(job));
    }
    m_pendingJobCount.fetch_add(1, std::memory_order_relaxed);
    m_wakeCondition.notify_all();
}

} // namespace engine::jobs
