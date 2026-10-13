#pragma once
#include <algorithm>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

namespace inst {
// Catalog indices stay stable for the entire session. The active item is never
// part of the editable pending deque.
class InstallQueue {
public:
    enum class State { Available, Pending, Active, Completed };
private:
    mutable std::mutex mutex;
    std::deque<size_t> pending;
    std::vector<size_t> completed;
    std::optional<size_t> active;
    size_t catalogSize = 0;
    std::vector<State> states;
public:
    State state(size_t id) const {
        std::lock_guard<std::mutex> lock(mutex);
        return id < states.size() ? states[id] : State::Available;
    }
    void reset(size_t count, const std::vector<size_t>& selected) {
        std::lock_guard<std::mutex> lock(mutex);
        catalogSize = count; pending.clear(); completed.clear(); active.reset();
        states.assign(count, State::Available);
        for (auto id : selected)
            if (id < count && states[id] == State::Available) { pending.push_back(id); states[id] = State::Pending; }
    }
    bool add(size_t id) {
        std::lock_guard<std::mutex> lock(mutex);
        if (id >= catalogSize || states[id] != State::Available) return false;
        pending.push_back(id); states[id] = State::Pending; return true;
    }
    bool remove(size_t id) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = std::find(pending.begin(), pending.end(), id);
        if (it == pending.end()) return false;
        pending.erase(it); states[id] = State::Available; return true;
    }
    std::optional<size_t> next() {
        std::lock_guard<std::mutex> lock(mutex);
        if (active || pending.empty()) return {};
        active = pending.front(); pending.pop_front(); states[*active] = State::Active; return active;
    }
    void finish() {
        std::lock_guard<std::mutex> lock(mutex);
        if (active) { completed.push_back(*active); states[*active] = State::Completed; }
        active.reset();
    }
    std::vector<size_t> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex);
        return {pending.begin(), pending.end()};
    }
    size_t completedCount() const {
        std::lock_guard<std::mutex> lock(mutex);
        return completed.size();
    }
};
}
