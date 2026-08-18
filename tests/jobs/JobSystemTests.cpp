#include "engine/jobs/JobSystem.h"

#include <doctest/doctest.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

using engine::jobs::JobSystem;

TEST_CASE("ParallelFor visits every index in range exactly once") {
    JobSystem jobSystem;
    REQUIRE(jobSystem.Init(4));

    constexpr std::size_t kItemCount = 10000;
    std::vector<std::atomic<int>> hitCounts(kItemCount);
    for (auto& count : hitCounts) {
        count = 0;
    }

    jobSystem.ParallelFor(kItemCount, [&hitCounts](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            hitCounts[i].fetch_add(1, std::memory_order_relaxed);
        }
    });

    for (std::size_t i = 0; i < kItemCount; ++i) {
        CHECK(hitCounts[i].load() == 1);
    }

    jobSystem.Shutdown();
}

TEST_CASE("ParallelFor blocks the caller until all chunks complete") {
    JobSystem jobSystem;
    REQUIRE(jobSystem.Init(4));

    std::atomic<int> sum{0};
    jobSystem.ParallelFor(1000, [&sum](std::size_t begin, std::size_t end) {
        int local = 0;
        for (std::size_t i = begin; i < end; ++i) {
            local += static_cast<int>(i);
        }
        sum.fetch_add(local, std::memory_order_relaxed);
    });

    // If ParallelFor returned before all chunks finished, this would be wrong (or racy
    // under a sanitizer) -- reading `sum` right after the call must already be complete.
    int reference = 0;
    for (int i = 0; i < 1000; ++i) {
        reference += i;
    }
    CHECK(sum.load() == reference);

    jobSystem.Shutdown();
}

TEST_CASE("ParallelFor works with more chunks than items") {
    JobSystem jobSystem;
    REQUIRE(jobSystem.Init(8));

    std::atomic<int> visits{0};
    jobSystem.ParallelFor(3, [&visits](std::size_t begin, std::size_t end) {
        visits.fetch_add(static_cast<int>(end - begin), std::memory_order_relaxed);
    });

    CHECK(visits.load() == 3);
    jobSystem.Shutdown();
}

TEST_CASE("GetWorkerCount reflects the requested worker count") {
    JobSystem jobSystem;
    REQUIRE(jobSystem.Init(3));
    CHECK(jobSystem.GetWorkerCount() == 3);
    jobSystem.Shutdown();
}

TEST_CASE("Submit runs every submitted job exactly once") {
    // Unlike ParallelFor, Submit() doesn't block the caller (physics/JoltJobSystemAdapter,
    // M4, needs fire-and-forget: Jolt's own Job::Execute() tracks completion/dependencies)
    // -- the test needs its own synchronization to know every job has actually run.
    JobSystem jobSystem;
    REQUIRE(jobSystem.Init(4));

    constexpr int kJobCount = 500;
    std::atomic<int> completed{0};
    std::mutex doneMutex;
    std::condition_variable doneCondition;

    for (int i = 0; i < kJobCount; ++i) {
        jobSystem.Submit([&completed, &doneMutex, &doneCondition]() {
            if (completed.fetch_add(1, std::memory_order_acq_rel) + 1 == kJobCount) {
                std::lock_guard<std::mutex> lock(doneMutex);
                doneCondition.notify_all();
            }
        });
    }

    std::unique_lock<std::mutex> lock(doneMutex);
    doneCondition.wait(lock, [&completed] { return completed.load() == kJobCount; });
    CHECK(completed.load() == kJobCount);

    jobSystem.Shutdown();
}
