#include "scheduler.hpp"

#include <fmt/core.h>

Scheduler::Scheduler(size_t numThreads) : stop(false), activeTasks(0), threadCount(numThreads) {
    // Ensure at least one thread
    if (threadCount == 0) {
        threadCount = 1;
    }

    // Create worker threads
    workerThreads.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        workerThreads.emplace_back(&Scheduler::worker, this);
    }

    fmt::print("Scheduler initialized with {} worker thread{}\n",
               threadCount, threadCount == 1 ? "" : "s");
}

Scheduler::~Scheduler() {
    shutdown();
}

std::string Scheduler::getType() const {
    return "Scheduler";
}

void Scheduler::display() const {
    fmt::print("Scheduler: {} worker thread{}, {} pending task{}, {} active, running: {}\n",
               threadCount, threadCount == 1 ? "" : "s",
               getPendingTasks(), getPendingTasks() == 1 ? "" : "s",
               activeTasks.load(),
               isRunning());
}

void Scheduler::worker() {
    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(queueMutex);

            // Wait for a task or stop signal
            condition.wait(lock, [this] {
                return !taskQueue.empty() || stop.load();
            });

            // Exit if stopping and no more tasks
            if (stop.load() && taskQueue.empty()) {
                return;
            }

            // Get the next task
            if (!taskQueue.empty()) {
                task = std::move(taskQueue.front());
                taskQueue.pop();
                activeTasks.fetch_add(1);
            } else {
                continue; // Spurious wakeup, continue waiting
            }
        }

        // Execute the task outside of the lock
        try {
            task();
        } catch (const std::exception& e) {
            fmt::print("Task execution failed: {}\n", e.what());
        } catch (...) {
            fmt::print("Task execution failed with unknown exception\n");
        }

        // Task completed
        activeTasks.fetch_sub(1);
        completionCondition.notify_all();
    }
}

size_t Scheduler::getPendingTasks() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return taskQueue.size();
}

bool Scheduler::isRunning() const {
    return !stop.load();
}

void Scheduler::waitForAll() {
    std::unique_lock<std::mutex> lock(queueMutex);
    completionCondition.wait(lock, [this] {
        return taskQueue.empty() && activeTasks.load() == 0;
    });
}

void Scheduler::shutdown() {
    if (stop.load()) {
        return; // Already shut down
    }

    fmt::print("Shutting down scheduler...\n");

    // Signal all threads to stop
    stop.store(true);
    condition.notify_all();

    // Wait for all worker threads to finish
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // Clear any remaining tasks
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        std::queue<Task> empty;
        taskQueue.swap(empty);
    }

    fmt::print("Scheduler shutdown complete\n");
}
