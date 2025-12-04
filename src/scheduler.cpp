#include "scheduler.hpp"
#include <fmt/core.h>

Scheduler::Scheduler() : stop(false) {
    // Create single worker thread
    workerThread = std::thread(&Scheduler::worker, this);
    fmt::print("Scheduler initialized with single worker thread\n");
}

Scheduler::~Scheduler() {
    shutdown();
}

std::string Scheduler::getType() const {
    return "Scheduler";
}

void Scheduler::display() const {
    fmt::print("Scheduler: {} pending tasks, running: {}\n", 
               getPendingTasks(), isRunning());
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
    }
}

void Scheduler::schedule(Task task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        
        if (stop.load()) {
            fmt::print("Warning: Scheduler is shutting down, task rejected\n");
            return;
        }
        
        taskQueue.push(std::move(task));
    }
    condition.notify_one();
}

size_t Scheduler::getPendingTasks() const {
    std::unique_lock<std::mutex> lock(queueMutex);
    return taskQueue.size();
}

bool Scheduler::isRunning() const {
    return !stop.load();
}

void Scheduler::waitForAll() {
    while (getPendingTasks() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Additional small delay to ensure worker has finished current task
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void Scheduler::shutdown() {
    if (stop.load()) {
        return; // Already shut down
    }
    
    fmt::print("Shutting down scheduler...\n");
    
    // Signal thread to stop
    stop.store(true);
    condition.notify_one();
    
    // Wait for worker thread to finish
    if (workerThread.joinable()) {
        workerThread.join();
    }
    
    // Clear any remaining tasks
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        std::queue<Task> empty;
        taskQueue.swap(empty);
    }
    
    fmt::print("Scheduler shutdown complete\n");
}
