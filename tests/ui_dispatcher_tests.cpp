#include "util/ui_dispatcher.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

int main() {
    inst::UiDispatcher ui;
    const auto owner = std::this_thread::get_id();
    assert(ui.invoke([] { return 7; }) == 7);
    std::vector<int> events;
    int applied = 0;
    std::thread progress([&] {
        for (int i = 0; i < 10000; ++i)
            ui.postLatest("progress", [&, i] {
                assert(std::this_thread::get_id() == owner);
                ++applied; events.push_back(i);
            });
    });
    progress.join();
    ui.drain();
    assert(applied == 1 && events == std::vector<int>{9999});

    // A worker can wait for a UI decision and receive its value or exception.
    // The callback can call invoke again on the owner without deadlocking.
    std::atomic<bool> done{false};
    std::thread worker([&] {
        ui.postLatest("progress", [&] { events.push_back(10); });
        const int answer = ui.invoke([&] {
            assert(std::this_thread::get_id() == owner);
            events.push_back(20);
            return ui.invoke([] { return 42; });
        });
        assert(answer == 42);
        ui.postLatest("progress", [&] { events.push_back(30); });
        try {
            ui.invoke([]() -> void { throw std::runtime_error("UI decision failed"); });
            assert(false);
        } catch (const std::runtime_error& error) {
            assert(std::string(error.what()) == "UI decision failed");
        }
        done = true;
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done && std::chrono::steady_clock::now() < deadline) {
        ui.drain();
        std::this_thread::yield();
    }
    assert(done);
    worker.join();
    ui.drain();
    assert((events == std::vector<int>{9999, 10, 20, 30}));

    std::thread wrongThread([&] {
        try { ui.drain(); assert(false); }
        catch (const std::logic_error&) {}
    });
    wrongThread.join();
    std::cout << "UI dispatcher tests passed\n";
}
