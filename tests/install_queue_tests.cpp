#include "util/install_queue.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

int main() {
    inst::InstallQueue queue;
    queue.reset(5, {0, 1, 2, 2, 99});
    assert((queue.snapshot() == std::vector<size_t>{0, 1, 2}));
    assert(queue.next() == 0);
    assert(queue.state(0) == inst::InstallQueue::State::Active);
    assert(!queue.next());
    assert(!queue.remove(0));
    assert(!queue.add(0));
    assert(queue.remove(1));
    assert(queue.add(3));
    assert(queue.add(1));
    assert(!queue.add(1));
    assert((queue.snapshot() == std::vector<size_t>{2, 3, 1}));
    queue.finish();
    assert(queue.state(0) == inst::InstallQueue::State::Completed);
    assert(!queue.add(0));
    assert(queue.next() == 2);
    queue.finish();
    assert(queue.next() == 3);
    queue.finish();
    assert(queue.next() == 1);
    queue.finish();
    assert(!queue.next());
    assert(queue.completedCount() == 4);
    // The shop can append another title after the last active title finishes,
    // preserving completion history and preventing completed titles from re-entry.
    assert(queue.add(4));
    assert(!queue.add(0));
    assert(queue.next() == 4);
    assert(queue.completedCount() == 4);
    queue.finish();
    assert(queue.completedCount() == 5);
    assert(!queue.remove(4));

    // Pending edits race a consumer holding the active title. Neither thread
    // can remove/requeue that active title or start a second one.
    queue.reset(100, {0});
    assert(queue.next() == 0);
    std::atomic<bool> done{false};
    std::thread editor([&] {
        for (size_t round = 0; round < 100; ++round)
            for (size_t id = 1; id < 100; ++id) { queue.add(id); queue.remove(id); }
        done = true;
    });
    while (!done) {
        assert(!queue.remove(0));
        assert(!queue.add(0));
        assert(!queue.next());
        for (auto id : queue.snapshot()) assert(id != 0);
    }
    editor.join();
    queue.finish();
    assert(!queue.add(0));
    queue.reset(2, {1});
    assert(queue.completedCount() == 0);
    assert(queue.next() == 1);
    std::cout << "Install queue tests passed\n";
}
