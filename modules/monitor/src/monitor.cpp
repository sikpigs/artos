#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <iterator>
#include <limits>
#include <span>
#include <string_view>
#include <system_error>

#include <artos.h>
#include <board_services.h>
#include <build_config.h>
#include <console/console.h>
#include <monitor/monitor.h>

static constexpr std::array<char, 2>   new_line_buffer = { '\n', '\r' };
static constexpr std::array<char, 1>   escape_buffer = { '\\' };
static constexpr std::span<const char> new_line { new_line_buffer };
static constexpr std::span<const char> escape { escape_buffer };

static std::uint32_t read_line(artos::console::buffered_console_t& console, std::span<char> line_in) noexcept
{
    auto       p = line_in.begin();
    const auto end = line_in.end();
    bool       overflow = false;
    while (true)
    {
        std::uint8_t c;
        c = console.read_byte();
        console.write(c);
        if (c == '\n')
        {
            continue;
        }
        if (c == '\r')
        {
            break;
        }
        if (overflow)
        {
            continue; // Discard the rest of an oversized command through CR.
        }
        if (c == 0x08 || c == 0x7F)
        {
            if (p > line_in.begin())
            {
                --p;
                console.write(' ');
                console.write(0x08);
            }
            continue;
        }

        if (p == end)
        {
            overflow = true;
            continue;
        }
        *p = c;
        ++p;
    }

    return overflow ? std::numeric_limits<std::uint32_t>::max()
                    : static_cast<std::uint32_t>(std::distance(line_in.begin(), p));
}

void to_chars_hex(std::span<char> in, std::uint32_t value)
{
    for (auto it = in.rbegin(); it != in.rend(); ++it)
    {
        const std::uint_fast8_t n = value & 0x0F;
        value >>= 4;
        if (n < 0x0A)
        {
            *it = static_cast<char>(n + static_cast<std::uint_fast8_t>(0x30));
        }
        else
        {
            *it = static_cast<char>(n + static_cast<std::uint_fast8_t>(0x37));
        }
    }
}

void dump(artos::console::buffered_console_t& usart, std::span<const volatile std::uint32_t> words)
{
    std::array<char, 8> buffer;
    for (std::size_t row = 0; row < words.size();)
    {
        std::array<char, 18> interp;
        interp.fill(' ');
        interp.front() = '|';
        interp.back() = '|';

        usart.write(new_line);
        to_chars_hex(buffer, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(words.data() + row)));
        usart.write(buffer);
        usart.write(' ');

        for (std::size_t column = 0; column < 4; ++column)
        {
            usart.write(' ');
            if (row < words.size())
            {
                // Exactly one 32-bit hardware read per word; format the local copy.
                const std::uint32_t value = words[row++];
                to_chars_hex(buffer, value);
                usart.write(buffer);
                const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
                for (std::size_t byte = 0; byte < sizeof(value); ++byte)
                {
                    interp[1 + column * sizeof(value) + byte]
                        = std::isprint(bytes[byte]) ? static_cast<char>(bytes[byte]) : '.';
                }
            }
            else
            {
                buffer.fill(' ');
                usart.write(buffer);
            }
            if (column == 1)
            {
                usart.write(' ');
            }
        }
        usart.write(' ');
        usart.write(interp);
    }
}

static void write_text(artos::console::buffered_console_t& console, std::string_view value)
{
    console.write(std::span<const char>(value.data(), value.size()));
}

#ifdef CONFIG_ARTOS_THREAD_STATS
static void write_decimal(artos::console::buffered_console_t& console, std::uint64_t value)
{
    std::array<char, 20> buffer {};
    const auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (ec == std::errc {})
    {
        console.write(std::span<const char>(buffer.data(), static_cast<std::size_t>(end - buffer.data())));
    }
}
#endif

static void measure_snapshot_latency(artos::console::buffered_console_t& console)
{
#ifdef CONFIG_ARTOS_THREAD_STATS
    static artos::thread::snapshot_array_t snapshot {};
    constexpr std::uint32_t                sample_count = 32;
    std::uint32_t                          min_cycles = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t                          max_cycles = 0;
    std::uint64_t                          total_cycles = 0;
    std::size_t                            thread_count = 0;

    for (std::uint32_t sample = 0; sample < sample_count; ++sample)
    {
        const auto start = board::stopwatch::count();
        thread_count = artos::thread::get_snapshot(snapshot);
        const auto elapsed = board::stopwatch::count() - start;
        min_cycles = std::min(min_cycles, elapsed);
        max_cycles = std::max(max_cycles, elapsed);
        total_cycles += elapsed;
    }

    const auto clock_hz = board::stopwatch::clock_hz();
    console.write(new_line);
    write_text(console, "snapshot threads=");
    write_decimal(console, thread_count);
    write_text(console, " samples=");
    write_decimal(console, sample_count);
    write_text(console, " cycles min/avg/max=");
    write_decimal(console, min_cycles);
    write_text(console, "/");
    write_decimal(console, total_cycles / sample_count);
    write_text(console, "/");
    write_decimal(console, max_cycles);
    write_text(console, " max_us=");
    write_decimal(console, (static_cast<std::uint64_t>(max_cycles) * 1'000'000 + clock_hz - 1) / clock_hz);
    write_text(console, " tick_pct=");
    write_decimal(console,
                  (static_cast<std::uint64_t>(max_cycles) * build_config::artos::tick_hz * 100 + clock_hz - 1)
                      / clock_hz);
    write_text(console, "%");
#else
    console.write(new_line);
    write_text(console, "snapshot latency requires CONFIG_ARTOS_THREAD_STATS=y");
#endif
}

static bool process_line(artos::console::buffered_console_t& console, std::span<const char> line)
{
    if (line.empty())
    {
        return false;
    }
    if (std::string_view { line.data(), line.size() } == "snapshot")
    {
        measure_snapshot_latency(console);
        return true;
    }
    auto*       cursor = line.data();
    const auto* end = cursor + line.size();
    const auto  skip_space = [&] {
        while (cursor != end && std::isspace(static_cast<unsigned char>(*cursor)))
        {
            ++cursor;
        }
    };
    const auto parse_hex = [&](std::uint32_t& value) {
        const auto [next, ec] = std::from_chars(cursor, end, value, 16);
        cursor = next;
        return ec == std::errc {};
    };

    std::uint32_t address;
    skip_space();
    if (!parse_hex(address) || address % sizeof(std::uint32_t) != 0)
    {
        return false;
    }
    skip_space();
    // Bound counts without overflowing the 32-bit address space.
    const auto    available_words = (std::numeric_limits<std::uint32_t>::max() - address) / sizeof(std::uint32_t) + 1;
    std::uint32_t count = 1;
    if (cursor != end && *cursor == ':')
    {
        ++cursor;
        // A 128-character line holds at most 64 whitespace-separated values.
        // Validate the entire command before performing any hardware writes.
        std::array<std::uint32_t, 64> values;
        std::size_t                   value_count = 0;
        skip_space();
        while (cursor != end)
        {
            if (value_count == values.size() || !parse_hex(values[value_count]))
            {
                return false;
            }
            ++value_count;
            if (cursor != end && !std::isspace(static_cast<unsigned char>(*cursor)))
            {
                return false;
            }
            skip_space();
        }
        if (value_count == 0 || value_count > available_words)
        {
            return false;
        }
        auto* destination = reinterpret_cast<volatile std::uint32_t*>(address);
        for (std::size_t i = 0; i < value_count; ++i)
        {
            destination[i] = values[i];
        }
        return true;
    }
    if (cursor != end && *cursor == '.')
    {
        ++cursor;
        skip_space();
        if (!parse_hex(count))
        {
            return false;
        }
        skip_space();
    }
    if (cursor != end || count == 0 || count > available_words)
    {
        return false;
    }
    const auto* start = reinterpret_cast<const volatile std::uint32_t*>(address);
    dump(console, std::span(start, count));
    return true;
}

void monitor::run()
{
    auto&                 console = artos::console::system_console();
    std::array<char, 128> in_buffer;
    bool                  valid = false;
    while (true)
    {
        if (!valid)
        {
            console.write(escape);
        }
        console.write(new_line);
        const auto line_len = read_line(console, in_buffer);
        valid = line_len != std::numeric_limits<std::uint32_t>::max()
                && process_line(console, std::span<const char>(in_buffer.data(), line_len));
    }
}
