#include <wait_queue.h>

#include <cstdio>
#include <cstdlib>

namespace {
    void check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::printf("FAIL: %s\n", message);
            std::exit(1);
        }
    }

    void test_clear_releases_nodes()
    {
        artos::wait_queue_t      queue;
        artos::wait_queue_node_t first { nullptr };
        artos::wait_queue_node_t second { nullptr };

        queue.push_back(first);
        queue.push_back(second);
        queue.clear();

        check(queue.empty(), "cleared wait queue was not empty");
        check(first.queue == nullptr, "clear left the first node associated with the queue");
        check(second.queue == nullptr, "clear left the second node associated with the queue");
    }
}

int main()
{
    test_clear_releases_nodes();
    std::printf("1 test, 0 failed\n");
    return 0;
}
