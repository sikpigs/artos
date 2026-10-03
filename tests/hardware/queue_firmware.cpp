#include <artos.h>
#include <queue.h>

#include <cstdint>
#include <new>

namespace {
    artos::queue_storage_t<int, 1> reader_storage {};
    artos::queue_storage_t<int, 1> writer_storage {};
    artos::queue_storage_t<int, 1> timeout_storage {};
    alignas(artos::queue_t<int>) std::byte reader_object[sizeof(artos::queue_t<int>)];
    alignas(artos::queue_t<int>) std::byte writer_object[sizeof(artos::queue_t<int>)];
    alignas(artos::queue_t<int>) std::byte timeout_object[sizeof(artos::queue_t<int>)];
    artos::queue_t<int>*           reader = nullptr;
    artos::queue_t<int>*           writer = nullptr;
    artos::queue_t<int>*           timeout_queue = nullptr;
    artos::thread::storage_t<2048> reader_thread_storage {};
    artos::thread::storage_t<2048> writer_thread_storage {};

    volatile std::uint32_t reader_result = 0xff;
    volatile std::uint32_t writer_result = 0xff;
    volatile std::uint32_t reader_output = 0;
    volatile std::uint32_t worker_done = 0;

    [[noreturn]] void finish(std::uint32_t code) noexcept;

    void reader_entry(void*)
    {
        int output = 42;
        reader_result = static_cast<std::uint32_t>(reader->pop_front(output, 1000));
        reader_output = static_cast<std::uint32_t>(output);
        worker_done |= 1U;
    }

    void writer_entry(void*)
    {
        writer_result = static_cast<std::uint32_t>(writer->push_back(2, 1000));
        worker_done |= 2U;
    }

    bool is_blocked(artos::thread::id_t id)
    {
        artos::thread::snapshot_array_t snapshot {};
        const auto                      count = artos::thread::get_snapshot(snapshot);
        for (std::size_t index = 0; index < count; ++index)
        {
            if (snapshot[index].id == id)
            {
                return snapshot[index].state == artos::thread::state_t::blocked;
            }
        }
        return false;
    }

    void require(bool condition, std::uint32_t code)
    {
        if (!condition)
        {
            finish(code);
        }
    }
}

extern "C"
{
    volatile std::uint32_t queue_hw_result = 0xffffffffU;
    volatile std::uint32_t queue_hw_stage = 0;
}

extern "C" [[gnu::noinline, noreturn]] void queue_hw_done() noexcept
{
    for (;;)
    {
        __asm__ volatile("nop");
    }
}

namespace {
    [[noreturn]] void finish(std::uint32_t code) noexcept
    {
        queue_hw_result = code;
        queue_hw_done();
    }
}

extern "C" void app_main() noexcept
{
    using artos::queue_result_t;
    reader = ::new (reader_object) artos::queue_t<int> { reader_storage };
    writer = ::new (writer_object) artos::queue_t<int> { writer_storage };
    timeout_queue = ::new (timeout_object) artos::queue_t<int> { timeout_storage };
    queue_hw_stage = 1;
    require(writer->push_back(1, 0) == queue_result_t::success, 1);

    const auto reader_id = artos::thread::create(
        reader_entry, "QREADER", nullptr, artos::thread::priority_t::low, reader_thread_storage);
    require(static_cast<bool>(reader_id), 2);
    const auto writer_id = artos::thread::create(
        writer_entry, "QWRITER", nullptr, artos::thread::priority_t::low, writer_thread_storage);
    require(static_cast<bool>(writer_id), 3);

    artos::thread::sleep_for(3);
    queue_hw_stage = 2;
    require(is_blocked(*reader_id), 4);
    require(is_blocked(*writer_id), 5);

    reader->close_and_wait();
    queue_hw_stage = 3;
    int closed_output = 99;
    require(reader->pop_front(closed_output, 0) == queue_result_t::closed && closed_output == 99, 6);
    writer->close_and_wait();
    queue_hw_stage = 4;

    for (unsigned attempt = 0; worker_done != 3U && attempt < 20; ++attempt)
    {
        artos::thread::sleep_for(1);
    }
    require(worker_done == 3U, 7);
    require(reader_result == static_cast<std::uint32_t>(queue_result_t::closed), 8);
    require(writer_result == static_cast<std::uint32_t>(queue_result_t::closed), 9);
    require(reader_output == 42U, 10);

    int output = 77;
    require(reader->pop_front(output, 0) == queue_result_t::closed && output == 77, 11);
    require(reader->push_back(3, 0) == queue_result_t::closed, 12);
    require(writer->pop_front(output, 0) == queue_result_t::closed && output == 77, 13);
    require(writer->push_back(3, 0) == queue_result_t::closed, 14);
    reader->close();
    writer->close();
    reader->close_and_wait();
    writer->close_and_wait();

    queue_hw_stage = 5;
    require(timeout_queue->pop_front(output, 3) == queue_result_t::timeout && output == 77, 15);
    require(timeout_queue->push_back(1, 0) == queue_result_t::success, 16);
    require(timeout_queue->push_back(2, 3) == queue_result_t::timeout, 17);
    require(timeout_queue->pop_front(output, 0) == queue_result_t::success && output == 1, 18);
    timeout_queue->close_and_wait();
    queue_hw_stage = 6;
    finish(0);
}
