#include "clock.hpp"
#include <stdexcept>

// Clock base class implementation
Clock::time_point Clock::from_system(std::chrono::system_clock::time_point tp) {
    auto duration = tp.time_since_epoch();
    return time_point(std::chrono::duration_cast<Clock::duration>(duration));
}

std::chrono::system_clock::time_point Clock::to_system(time_point tp) {
    auto duration = tp.time_since_epoch();
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(duration)
    );
}

// RealTimeClock implementation
RealTimeClock::RealTimeClock()
    : baseTime(std::chrono::steady_clock::now()),
      baseOffset(0) {}

Clock::time_point RealTimeClock::now() const {
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = currentTime - baseTime;
    auto totalTime = baseOffset + std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
    return Clock::time_point(totalTime);
}

void RealTimeClock::reset() {
    baseTime = std::chrono::steady_clock::now();
    baseOffset = std::chrono::nanoseconds(0);
}

// SimulatorClock implementation
SimulatorClock::SimulatorClock()
    : baseTime(Clock::time_point(std::chrono::nanoseconds(0))),
      elapsedTime(0),
      lastRealTime(std::chrono::steady_clock::now()),
      timeScale(1.0),
      mode(Mode::REALTIME) {}

SimulatorClock::SimulatorClock(double scale)
    : baseTime(Clock::time_point(std::chrono::nanoseconds(0))),
      elapsedTime(0),
      lastRealTime(std::chrono::steady_clock::now()),
      timeScale(scale),
      mode(Mode::REALTIME) {}

void SimulatorClock::updateFromRealTime() {
    auto currentRealTime = std::chrono::steady_clock::now();
    auto realDelta = currentRealTime - lastRealTime;
    lastRealTime = currentRealTime;

    // Apply time scale
    double scale = timeScale.load();
    auto scaledDelta = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double, std::nano>(realDelta.count() * scale)
    );

    elapsedTime += scaledDelta;
}

Clock::time_point SimulatorClock::now() const {
    std::lock_guard<std::mutex> lock(mutex);

    Mode currentMode = mode.load();

    if (currentMode == Mode::REALTIME) {
        // Update time based on real time passage
        const_cast<SimulatorClock*>(this)->updateFromRealTime();
    }
    // For MANUAL and PAUSED modes, just return current elapsed time

    return baseTime + elapsedTime;
}

void SimulatorClock::setTimeScale(double scale) {
    if (scale < 0.0) {
        throw std::invalid_argument("Time scale cannot be negative");
    }

    std::lock_guard<std::mutex> lock(mutex);

    // Update to current time before changing scale
    if (mode.load() == Mode::REALTIME) {
        updateFromRealTime();
    }

    timeScale.store(scale);
}

double SimulatorClock::getTimeScale() const {
    return timeScale.load();
}

void SimulatorClock::setMode(Mode newMode) {
    std::lock_guard<std::mutex> lock(mutex);

    Mode oldMode = mode.load();

    if (oldMode == newMode) {
        return;
    }

    // If transitioning from REALTIME, update to current time first
    if (oldMode == Mode::REALTIME) {
        updateFromRealTime();
    }

    // If transitioning to REALTIME, reset the real time reference
    if (newMode == Mode::REALTIME) {
        lastRealTime = std::chrono::steady_clock::now();
    }

    mode.store(newMode);
}

SimulatorClock::Mode SimulatorClock::getMode() const {
    return mode.load();
}

void SimulatorClock::advance(std::chrono::nanoseconds delta) {
    std::lock_guard<std::mutex> lock(mutex);

    if (mode.load() != Mode::MANUAL) {
        throw std::logic_error("Cannot manually advance time unless in MANUAL mode");
    }

    elapsedTime += delta;
}

void SimulatorClock::pause() {
    setMode(Mode::PAUSED);
}

void SimulatorClock::resume() {
    setMode(Mode::REALTIME);
}

void SimulatorClock::reset() {
    std::lock_guard<std::mutex> lock(mutex);

    elapsedTime = std::chrono::nanoseconds(0);
    lastRealTime = std::chrono::steady_clock::now();
}

void SimulatorClock::setTime(std::chrono::nanoseconds time) {
    std::lock_guard<std::mutex> lock(mutex);

    elapsedTime = time;

    // If in REALTIME mode, reset the real time reference
    if (mode.load() == Mode::REALTIME) {
        lastRealTime = std::chrono::steady_clock::now();
    }
}

std::chrono::nanoseconds SimulatorClock::elapsed() const {
    std::lock_guard<std::mutex> lock(mutex);

    // Get current time without modifying state
    if (mode.load() == Mode::REALTIME) {
        auto currentRealTime = std::chrono::steady_clock::now();
        auto realDelta = currentRealTime - lastRealTime;
        double scale = timeScale.load();
        auto scaledDelta = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double, std::nano>(realDelta.count() * scale)
        );
        return elapsedTime + scaledDelta;
    }

    return elapsedTime;
}

// GlobalClock implementation
std::unique_ptr<Clock> GlobalClock::instance = std::make_unique<RealTimeClock>();
std::mutex GlobalClock::instanceMutex;

Clock& GlobalClock::get() {
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (!instance) {
        instance = std::make_unique<RealTimeClock>();
    }
    return *instance;
}

void GlobalClock::set(std::unique_ptr<Clock> clock) {
    std::lock_guard<std::mutex> lock(instanceMutex);
    instance = std::move(clock);
}

void GlobalClock::useRealTime() {
    set(std::make_unique<RealTimeClock>());
}

void GlobalClock::useSimulator(double scale) {
    set(std::make_unique<SimulatorClock>(scale));
}

RealTimeClock* GlobalClock::getRealTime() {
    std::lock_guard<std::mutex> lock(instanceMutex);
    return dynamic_cast<RealTimeClock*>(instance.get());
}

SimulatorClock* GlobalClock::getSimulator() {
    std::lock_guard<std::mutex> lock(instanceMutex);
    return dynamic_cast<SimulatorClock*>(instance.get());
}

GlobalClock::time_point GlobalClock::now() {
    return get().now();
}

// ScopedClockOverride implementation
ScopedClockOverride::ScopedClockOverride(std::unique_ptr<Clock> temporaryClock) {
    std::lock_guard<std::mutex> lock(GlobalClock::instanceMutex);
    previousClock = std::move(GlobalClock::instance);
    GlobalClock::instance = std::move(temporaryClock);
}

ScopedClockOverride::~ScopedClockOverride() {
    std::lock_guard<std::mutex> lock(GlobalClock::instanceMutex);
    GlobalClock::instance = std::move(previousClock);
}
