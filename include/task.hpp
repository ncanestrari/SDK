#pragma once

#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

// Type-erased task that can store any callable with arguments and return value
class Task {
private:
    // Abstract base for type erasure
    struct TaskBase {
        virtual ~TaskBase() = default;
        virtual void execute() = 0;
    };

    // Concrete implementation storing function, arguments, and return value
    template<typename Func, typename... Args>
    struct TaskImpl : TaskBase {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        Func func;
        std::tuple<Args...> args;

        TaskImpl(Func&& f, Args&&... a)
            : func(std::forward<Func>(f))
            , args(std::forward<Args>(a)...) {}

        void execute() override {
            std::apply(func, args);
        }
    };

    std::unique_ptr<TaskBase> impl;

public:
    Task() = default;

    // Constructor from function and arguments
    template<typename Func, typename... Args>
    Task(Func&& func, Args&&... args)
        : impl(std::make_unique<TaskImpl<std::decay_t<Func>, std::decay_t<Args>...>>(
            std::forward<Func>(func),
            std::forward<Args>(args)...)) {}

    // Execute the task
    void operator()() {
        if (impl) {
            impl->execute();
        }
    }

    // Check if task is valid
    explicit operator bool() const {
        return impl != nullptr;
    }
};
