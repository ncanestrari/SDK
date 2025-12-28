#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

// Forward declaration for simulator clock
class SimulatorClock;

// Clock interface that can be used as a drop-in replacement for std::steady_clock
class Clock {
public:
    using duration = std::chrono::nanoseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<Clock, duration>;

    static constexpr bool is_steady = true;

    virtual ~Clock() = default;

    // Get current time point
    virtual time_point now() const = 0;

    // Convert to/from system clock for external interfacing
    static time_point from_system(std::chrono::system_clock::time_point tp);
    static std::chrono::system_clock::time_point to_system(time_point tp);
};

// Real-time clock - uses system steady_clock
class RealTimeClock : public Clock {
private:
    std::chrono::steady_clock::time_point baseTime;
    std::chrono::nanoseconds baseOffset;

public:
    RealTimeClock();

    time_point now() const override;

    // Reset the clock base time
    void reset();
};

// Simulator clock - allows time scaling and manual control
class SimulatorClock : public Clock {
public:
    enum class Mode {
        REALTIME,    // Follow real time with optional scaling
        MANUAL,      // Manually advanced by calling advance()
        PAUSED       // Time is frozen
    };

private:
    mutable std::mutex mutex;

    // Base time when clock was created/reset
    time_point baseTime;

    // Current simulated time (relative to base)
    std::chrono::nanoseconds elapsedTime;

    // Last real time we updated (for REALTIME mode)
    std::chrono::steady_clock::time_point lastRealTime;

    // Time scaling factor (1.0 = normal, 2.0 = twice as fast, 0.5 = half speed)
    std::atomic<double> timeScale;

    // Current mode
    std::atomic<Mode> mode;

    // Update elapsed time based on real time (used in REALTIME mode)
    void updateFromRealTime();

public:
    SimulatorClock();
    explicit SimulatorClock(double scale);

    time_point now() const override;

    // Set time scale (only affects REALTIME mode)
    // scale > 1.0: speed up time
    // scale < 1.0: slow down time
    // scale = 0.0: effectively pauses (but use PAUSED mode instead)
    void setTimeScale(double scale);
    double getTimeScale() const;

    // Set clock mode
    void setMode(Mode newMode);
    Mode getMode() const;

    // Manually advance time (only works in MANUAL mode)
    void advance(std::chrono::nanoseconds delta);

    // Convenience methods for advancing time
    template<typename Rep, typename Period>
    void advance(std::chrono::duration<Rep, Period> delta) {
        advance(std::chrono::duration_cast<std::chrono::nanoseconds>(delta));
    }

    // Pause/resume (convenience methods for setMode)
    void pause();
    void resume();

    // Reset the clock to time zero
    void reset();

    // Set absolute time
    void setTime(std::chrono::nanoseconds time);

    template<typename Rep, typename Period>
    void setTime(std::chrono::duration<Rep, Period> time) {
        setTime(std::chrono::duration_cast<std::chrono::nanoseconds>(time));
    }

    // Get elapsed time since base
    std::chrono::nanoseconds elapsed() const;
};

// Global clock instance (can be swapped between RealTime and Simulator)
class GlobalClock {
private:
    static std::unique_ptr<Clock> instance;
    static std::mutex instanceMutex;

    friend class ScopedClockOverride;

public:

    // Get the current global clock
    static Clock& get();

    // Set a custom clock implementation
    static void set(std::unique_ptr<Clock> clock);

    // Convenience methods to set specific clock types
    static void useRealTime();
    static void useSimulator(double scale = 1.0);

    // Get as specific clock type (returns nullptr if wrong type)
    static RealTimeClock* getRealTime();
    static SimulatorClock* getSimulator();

    // Static interface matching std::chrono::steady_clock
    using time_point = Clock::time_point;
    using duration = Clock::duration;
    using rep = Clock::rep;
    using period = Clock::period;
    static constexpr bool is_steady = true;

    static time_point now();
};

// Helper to temporarily override the global clock
class ScopedClockOverride {
private:
    std::unique_ptr<Clock> previousClock;

public:
    explicit ScopedClockOverride(std::unique_ptr<Clock> temporaryClock);
    ~ScopedClockOverride();

    // Non-copyable, non-movable
    ScopedClockOverride(const ScopedClockOverride&) = delete;
    ScopedClockOverride& operator=(const ScopedClockOverride&) = delete;
    ScopedClockOverride(ScopedClockOverride&&) = delete;
    ScopedClockOverride& operator=(ScopedClockOverride&&) = delete;
};
