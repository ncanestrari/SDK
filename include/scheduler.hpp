#pragma once

#include "object.hpp"
#include "task.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

class Scheduler : public Object {
private:
    std::queue<Task> taskQueue;
    std::thread workerThread;
    mutable std::mutex queueMutex;
    std::condition_variable condition;
    std::condition_variable completionCondition;
    std::atomic<bool> stop;
    std::atomic<size_t> activeTasks;

    // Worker thread function
    void worker();
    
public:
    // Constructor - creates single worker thread
    Scheduler();
    
    // Destructor - stops worker thread
    ~Scheduler() override;
    
    // Delete copy constructor and assignment operator
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    
    // Override Object methods
    std::string getType() const override;
    void display() const override;
    
    // Schedule a function for asynchronous execution (fire-and-forget)
    template<typename Func, typename... Args>
    void schedule(Func&& func, Args&&... args) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);

            if (stop.load()) {
                //fmt::print("Warning: Scheduler is shutting down, task rejected\n");
                return;
            }

            taskQueue.emplace(std::forward<Func>(func), std::forward<Args>(args)...);
        }
        condition.notify_one();
    }
    
    // Prepare a function for execution and return a future for the result
    template<typename Func, typename... Args>
    auto prepare(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        auto taskPtr = std::make_shared<std::packaged_task<ReturnType()>>(
            [func = std::forward<Func>(func), args...]() mutable {
                return func(args...);
            }
        );

        std::future<ReturnType> result = taskPtr->get_future();

        schedule([taskPtr]() {
            (*taskPtr)();
        });

        return result;
    }
    
    // Get the number of pending tasks
    size_t getPendingTasks() const;
    
    // Check if scheduler is running
    bool isRunning() const;
    
    // Wait for all pending tasks to complete
    void waitForAll();
    
    // Stop accepting new tasks and shutdown
    void shutdown();
};
