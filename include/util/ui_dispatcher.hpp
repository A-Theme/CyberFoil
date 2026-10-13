#pragma once
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>

namespace inst {
// UI objects belong to one thread. Worker commands are acknowledged on that
// thread; frequent progress updates replace an older pending update by key.
class UiDispatcher {
    struct Command { std::string key; std::function<void()> run; };
    const std::thread::id owner = std::this_thread::get_id();
    std::mutex mutex;
    std::deque<Command> commands;
public:
    bool isOwner() const { return std::this_thread::get_id() == owner; }

    template<class F> auto invoke(F fn) -> std::invoke_result_t<F> {
        if (isOwner()) return fn();
        auto task = std::make_shared<std::packaged_task<std::invoke_result_t<F>()>>(std::move(fn));
        auto result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex);
            commands.push_back({{}, [task] { (*task)(); }});
        }
        return result.get();
    }

    void postLatest(const std::string& key, std::function<void()> fn) {
        if (isOwner()) { fn(); return; }
        std::lock_guard<std::mutex> lock(mutex);
        // Only coalesce within the suffix after the last acknowledged command.
        // A new title or dialog must observe all earlier progress in order.
        for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
            if (it->key.empty()) break;
            if (it->key == key) { it->run = std::move(fn); return; }
        }
        commands.push_back({key, std::move(fn)});
    }

    void drain() {
        if (!isOwner()) throw std::logic_error("UI commands must run on the UI thread");
        std::deque<Command> frame;
        {
            std::lock_guard<std::mutex> lock(mutex);
            frame.swap(commands);
        }
        // No lock while callbacks run. Work posted during this frame is handled
        // next frame so a busy producer cannot starve input or rendering.
        for (auto& command : frame) command.run();
    }
};
}
