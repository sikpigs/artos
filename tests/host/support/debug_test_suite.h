#pragma once

#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string_view>

#include <sys/wait.h>
#include <unistd.h>

namespace host_tests {
    struct test_case_t
    {
        const char* name;
        void (*run)();
    };

    struct check_failure_t
    {
    };

    inline bool debug_suite_active = false;

    inline void check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAIL: %s\n", message);
            if (debug_suite_active)
            {
                throw check_failure_t {};
            }
            std::exit(1);
        }
    }

    template<typename Cases>
    int run_debug_suite(const Cases& cases)
    {
        debug_suite_active = true;
        unsigned failures = 0;
        for (const auto& test : cases)
        {
            std::printf("RUN: %s\n", test.name);
            std::fflush(stdout);
            try
            {
                test.run();
                std::printf("PASS: %s\n", test.name);
            }
            catch (const check_failure_t&)
            {
                ++failures;
                std::printf("FAIL: %s\n", test.name);
            }
            std::fflush(stdout);
        }
        debug_suite_active = false;
        std::printf("%zu tests, %u failed\n", std::size(cases), failures);
        return failures == 0 ? 0 : 1;
    }

    template<typename Cases>
    int run_cases(int argc, char** argv, const Cases& cases)
    {
        if (argc == 2 && std::string_view { argv[1] } == "--debug")
        {
            return run_debug_suite(cases);
        }
        if (argc == 3 && std::string_view { argv[1] } == "--case")
        {
            for (const auto& test : cases)
            {
                if (std::string_view { argv[2] } == test.name)
                {
                    test.run();
                    return 0;
                }
            }
        }
        if (argc != 1)
        {
            std::fprintf(stderr, "Usage: %s [--debug | --case NAME]\nCases:\n", argv[0]);
            for (const auto& test : cases)
            {
                std::fprintf(stderr, "  %s\n", test.name);
            }
            return 2;
        }

        unsigned failures = 0;
        for (const auto& test : cases)
        {
            std::fflush(nullptr);
            const auto child = fork();
            check(child >= 0, "fork failed");
            if (child == 0)
            {
                test.run();
                std::exit(0);
            }
            int status = 0;
            check(waitpid(child, &status, 0) == child, "waitpid failed");
            const bool passed = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            std::printf("%s: %s\n", passed ? "PASS" : "FAIL", test.name);
            if (WIFSIGNALED(status))
            {
                std::printf("  terminated by signal %d\n", WTERMSIG(status));
            }
            failures += !passed;
        }
        std::printf("%zu tests, %u failed\n", std::size(cases), failures);
        return failures == 0 ? 0 : 1;
    }
}
