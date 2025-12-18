
#include "scheduler.hpp"
#include "object.hpp"

#include <fmt/core.h>

#include <chrono>
#include <thread>

// Example functions for demonstration
void simpleTask(int id) {
    fmt::print("Executing task {}\n", id);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    fmt::print("Task {} completed\n", id);
}

int computeValue(int input) {
    fmt::print("Computing value for input: {}\n", input);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int result = input * input + 10;
    fmt::print("Computed result: {}\n", result);
    return result;
}

std::string processString(const std::string& str) {
    fmt::print("Processing string: '{}'\n", str);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    return "Processed: " + str;
}

int main() {
    fmt::print("=== Scheduler with ObjectRegistry Example ===\n");
    
    // Get the ObjectRegistry instance
    ObjectRegistry& registry = ObjectRegistry::getInstance();
    
    // Create and register schedulers
    auto scheduler1 = std::make_shared<Scheduler>();
    auto scheduler2 = std::make_shared<Scheduler>();
    
    registry.registerObject("main_scheduler", scheduler1);
    registry.registerObject("background_scheduler", scheduler2);
    
    fmt::print("Registered schedulers in ObjectRegistry\n");
    
    // Show registered objects
    auto names = registry.getObjectNames();
    fmt::print("Registered objects: ");
    for (const auto& name : names) {
        fmt::print("{} ", name);
    }
    fmt::print("\n\n");
    
    fmt::print("1. Testing schedule() with ObjectRegistry retrieval\n");
    
    // Get scheduler from registry and use it
    auto mainScheduler = std::dynamic_pointer_cast<Scheduler>(
        registry.getObject("main_scheduler")
    );
    
    if (mainScheduler) {
        mainScheduler->display();
        
        // Schedule some tasks
        for (int i = 1; i <= 3; ++i) {
            mainScheduler->schedule([&i](){ simpleTask(i); });
        }
        
        fmt::print("Scheduled 3 tasks\n");
        mainScheduler->display();
    }
    
    fmt::print("\n2. Testing prepare() with background scheduler\n");
    
    auto bgScheduler = std::dynamic_pointer_cast<Scheduler>(
        registry.getObject("background_scheduler")
    );
    
    if (bgScheduler) {
        bgScheduler->display();
        
        // Prepare tasks that return values
        auto future1 = bgScheduler->prepare([]() -> int { return computeValue(5); });
        auto future2 = bgScheduler->prepare([]() -> std::string { return processString("Hello"); });
        
        fmt::print("Tasks prepared on background scheduler\n");
        bgScheduler->display();
        
        // Get results
        try {
            int result1 = future1.get();
            std::string result2 = future2.get();
            
            fmt::print("Background results - Number: {}, String: '{}'\n", 
                      result1, result2);
        } catch (const std::exception& e) {
            fmt::print("Error getting results: {}\n", e.what());
        }
    }
    
    fmt::print("\n3. Testing Object interface\n");
    
    // Test Object interface through registry
    for (const auto& name : registry.getObjectNames()) {
        auto obj = registry.getObject(name);
        if (obj) {
            fmt::print("Object '{}' type: {}, display: ", name, obj->getType());
            obj->display();
        }
    }
    
    fmt::print("\n4. Mixed workload example\n");
    
    if (mainScheduler) {
        // Mix of different task types
        mainScheduler->schedule([]() {
            fmt::print("Lambda task on main scheduler\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        });
        
        auto asyncResult = mainScheduler->prepare([]() {
            fmt::print("Async computation\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return 42;
        });
        
        fmt::print("Main thread working while tasks execute...\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        
        int result = asyncResult.get();
        fmt::print("Async result: {}\n", result);
    }
    
    fmt::print("\n5. Waiting for all tasks and cleanup\n");
    
    // Wait for all schedulers to finish their tasks
    if (mainScheduler) {
        mainScheduler->waitForAll();
        fmt::print("Main scheduler tasks completed\n");
    }
    
    if (bgScheduler) {
        bgScheduler->waitForAll();
        fmt::print("Background scheduler tasks completed\n");
    }
    
    // Show final status
    fmt::print("\nFinal scheduler status:\n");
    for (const auto& name : registry.getObjectNames()) {
        auto obj = registry.getObject(name);
        if (obj && obj->getType() == "Scheduler") {
            fmt::print("{}: ", name);
            obj->display();
        }
    }
    
    fmt::print("\nSchedulers will shutdown automatically when destroyed...\n");
    
    return 0;
}
