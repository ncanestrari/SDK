#include "clock.hpp"
#include <fmt/core.h>
#include <thread>
#include <vector>

void demonstrateRealTimeClock() {
    fmt::print("=== Real Time Clock ===\n");

    GlobalClock::useRealTime();

    auto start = GlobalClock::now();
    fmt::print("Starting at time: {} ns\n", start.time_since_epoch().count());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto end = GlobalClock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    fmt::print("After 100ms sleep: {} ms elapsed\n", elapsed.count());
    fmt::print("\n");
}

void demonstrateSimulatorClockRealTime() {
    fmt::print("=== Simulator Clock (RealTime Mode) ===\n");

    GlobalClock::useSimulator(1.0);
    auto* simClock = GlobalClock::getSimulator();

    auto start = GlobalClock::now();
    fmt::print("Starting at time: {} ns\n", start.time_since_epoch().count());

    // Normal speed (1x)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto elapsed1 = std::chrono::duration_cast<std::chrono::milliseconds>(
        GlobalClock::now() - start
    );
    fmt::print("After 100ms sleep (1x speed): {} ms elapsed\n", elapsed1.count());

    // Speed up time (2x)
    simClock->setTimeScale(2.0);
    auto checkpoint1 = GlobalClock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto elapsed2 = std::chrono::duration_cast<std::chrono::milliseconds>(
        GlobalClock::now() - checkpoint1
    );
    fmt::print("After 100ms sleep (2x speed): {} ms elapsed\n", elapsed2.count());

    // Slow down time (0.5x)
    simClock->setTimeScale(0.5);
    auto checkpoint2 = GlobalClock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto elapsed3 = std::chrono::duration_cast<std::chrono::milliseconds>(
        GlobalClock::now() - checkpoint2
    );
    fmt::print("After 100ms sleep (0.5x speed): {} ms elapsed\n", elapsed3.count());

    fmt::print("\n");
}

void demonstrateSimulatorClockManual() {
    fmt::print("=== Simulator Clock (Manual Mode) ===\n");

    GlobalClock::useSimulator();
    auto* simClock = GlobalClock::getSimulator();
    simClock->setMode(SimulatorClock::Mode::MANUAL);
    simClock->reset();

    auto start = GlobalClock::now();
    fmt::print("Starting at time: {} ns\n", start.time_since_epoch().count());

    // Manually advance time
    simClock->advance(std::chrono::milliseconds(100));
    auto time1 = GlobalClock::now();
    fmt::print("After advancing 100ms: {} ms\n",
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   time1.time_since_epoch()
               ).count());

    // Advance by seconds
    simClock->advance(std::chrono::seconds(5));
    auto time2 = GlobalClock::now();
    fmt::print("After advancing 5s: {} ms\n",
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   time2.time_since_epoch()
               ).count());

    // Set absolute time
    simClock->setTime(std::chrono::hours(1));
    auto time3 = GlobalClock::now();
    fmt::print("After setting to 1 hour: {} ms\n",
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   time3.time_since_epoch()
               ).count());

    fmt::print("\n");
}

void demonstrateSimulatorClockPause() {
    fmt::print("=== Simulator Clock (Pause/Resume) ===\n");

    GlobalClock::useSimulator(1.0);
    auto* simClock = GlobalClock::getSimulator();
    simClock->reset();

    auto start = GlobalClock::now();
    fmt::print("Starting at time: {} ns\n", start.time_since_epoch().count());

    // Run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto time1 = GlobalClock::now();
    fmt::print("After 50ms: {} ms elapsed\n",
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   time1 - start
               ).count());

    // Pause
    simClock->pause();
    fmt::print("Clock paused\n");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto time2 = GlobalClock::now();
    fmt::print("After 100ms sleep (paused): {} ms elapsed (should be same as before)\n",
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   time2 - start
               ).count());

    // Resume
    simClock->resume();
    fmt::print("Clock resumed\n");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto time3 = GlobalClock::now();
    fmt::print("After 50ms sleep (resumed): {} ms elapsed\n",
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   time3 - start
               ).count());

    fmt::print("\n");
}

void demonstrateScopedOverride() {
    fmt::print("=== Scoped Clock Override ===\n");

    // Start with real-time clock
    GlobalClock::useRealTime();
    auto start1 = GlobalClock::now();
    fmt::print("Using RealTime clock\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto elapsed1 = std::chrono::duration_cast<std::chrono::milliseconds>(
        GlobalClock::now() - start1
    );
    fmt::print("Elapsed: {} ms\n", elapsed1.count());

    {
        // Temporarily use simulator clock
        ScopedClockOverride override(std::make_unique<SimulatorClock>(10.0));
        auto* simClock = GlobalClock::getSimulator();
        if (simClock) {
            fmt::print("Temporarily using Simulator clock (10x speed)\n");
            auto start2 = GlobalClock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            auto elapsed2 = std::chrono::duration_cast<std::chrono::milliseconds>(
                GlobalClock::now() - start2
            );
            fmt::print("Elapsed: {} ms (should be ~500ms)\n", elapsed2.count());
        }
    }

    // Back to real-time clock
    fmt::print("Back to RealTime clock\n");
    auto start3 = GlobalClock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto elapsed3 = std::chrono::duration_cast<std::chrono::milliseconds>(
        GlobalClock::now() - start3
    );
    fmt::print("Elapsed: {} ms\n", elapsed3.count());

    fmt::print("\n");
}

// Example: Simulating a game loop with variable time step
void demonstrateGameLoop() {
    fmt::print("=== Game Loop Simulation ===\n");

    GlobalClock::useSimulator(1.0);
    auto* simClock = GlobalClock::getSimulator();
    simClock->setMode(SimulatorClock::Mode::MANUAL);
    simClock->reset();

    const auto fixedTimeStep = std::chrono::milliseconds(16); // ~60 FPS
    auto lastTime = GlobalClock::now();

    fmt::print("Simulating game loop with 16ms fixed time step\n");

    for (int frame = 0; frame < 10; ++frame) {
        // Advance simulator time by fixed step
        simClock->advance(fixedTimeStep);
        auto currentTime = GlobalClock::now();

        auto deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        fmt::print("Frame {}: Time = {} ms, Delta = {} ms\n",
                   frame,
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       currentTime.time_since_epoch()
                   ).count(),
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       deltaTime
                   ).count());
    }

    fmt::print("\n");
}

// Example: Testing time-dependent behavior at different speeds
void demonstrateTimeScaling() {
    fmt::print("=== Time Scaling for Testing ===\n");

    GlobalClock::useSimulator(1.0);
    auto* simClock = GlobalClock::getSimulator();

    std::vector<double> scales = {0.1, 1.0, 10.0, 100.0};

    for (double scale : scales) {
        simClock->setTimeScale(scale);
        simClock->reset();

        auto start = GlobalClock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto end = GlobalClock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        fmt::print("Time scale {:.1f}x: 50ms real time = {} ms sim time\n",
                   scale, elapsed.count());
    }

    fmt::print("\n");
}

int main() {
    fmt::print("Clock System Examples\n");
    fmt::print("=====================\n\n");

    demonstrateRealTimeClock();
    demonstrateSimulatorClockRealTime();
    demonstrateSimulatorClockManual();
    demonstrateSimulatorClockPause();
    demonstrateScopedOverride();
    demonstrateGameLoop();
    demonstrateTimeScaling();

    fmt::print("=== Usage Summary ===\n");
    fmt::print("1. Use GlobalClock::useRealTime() for normal execution\n");
    fmt::print("2. Use GlobalClock::useSimulator() for testing/simulation\n");
    fmt::print("3. Set time scale with simClock->setTimeScale()\n");
    fmt::print("4. Use MANUAL mode for deterministic testing\n");
    fmt::print("5. Use pause()/resume() for debugging\n");
    fmt::print("6. Use ScopedClockOverride for temporary clock changes\n");

    return 0;
}
