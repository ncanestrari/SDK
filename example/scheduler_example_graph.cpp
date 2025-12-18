
#include "scheduler.hpp"

#include <fmt/core.h>

#include <chrono>
#include <functional>
#include <thread>

class Work {
private:
    Scheduler scheduler;
    int cycleCount;
    static const int MAX_CYCLES = 3;

    void moo() {
        fmt::print("[moo] Starting execution (cycle {})\n", cycleCount + 1);

        // Simulate workload
        std::this_thread::sleep_for(std::chrono::seconds(1));

        fmt::print("[moo] Completed work\n");

        // Increment cycle counter
        cycleCount++;

        if (cycleCount < MAX_CYCLES) {
            fmt::print("[moo] Scheduling foo for cycle {}\n", cycleCount + 1);
            scheduler.schedule([this](){ foo(); });
        } else {
            fmt::print("[moo] Reached {} cycles, exiting\n", MAX_CYCLES);
        }
    }

    void goo() {
        fmt::print("[goo] Starting execution\n");

        // Simulate workload
        std::this_thread::sleep_for(std::chrono::seconds(1));

        fmt::print("[goo] Completed work, scheduling moo\n");
        scheduler.schedule([this](){ moo(); });
    }

    void foo() {
        fmt::print("[foo] Starting execution\n");

        // Simulate workload
        std::this_thread::sleep_for(std::chrono::seconds(1));

        fmt::print("[foo] Completed work, scheduling goo\n");
        scheduler.schedule([this](){ goo(); });
    }

public:
    Work() : cycleCount(0) {}

    void start() {
        fmt::print("=== Scheduler Example: Nested Task Graph ===\n");
        fmt::print("This example demonstrates a cycle of tasks:\n");
        fmt::print("  foo -> goo -> moo -> foo (repeats {} times)\n\n", MAX_CYCLES);

        // Start the cycle by scheduling foo
        fmt::print("Starting cycle 1...\n");
        scheduler.schedule([this](){ foo(); });

        // Wait for all tasks to complete
        fmt::print("\nWaiting for all tasks to complete...\n\n");
        scheduler.waitForAll();

        fmt::print("\n=== All tasks completed successfully ===\n");
        fmt::print("Total cycles executed: {}\n", cycleCount);
    }
};

int main() {
    Work work;
    work.start();

    return 0;
}
