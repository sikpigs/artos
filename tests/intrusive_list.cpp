#include <intrusive_list.h>

#include <array>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>

namespace {
    using artos::intrusive_list_t;
    using artos::list_hook_t;

    void check(bool condition, const char* message)
    {
        // Unlike assert(), these checks also run in Release/NDEBUG builds.
        if (!condition)
            throw std::runtime_error(message);
    }

    void expect_list(intrusive_list_t& list, std::initializer_list<list_hook_t*> nodes)
    {
        check(list.empty() == (nodes.size() == 0), "empty() disagrees with expected size");
        auto*        current = list.front();
        list_hook_t* previous = nullptr;
        for (auto* expected : nodes)
        {
            check(current != nullptr, "forward traversal ended before expected node");
            check(current == expected, "forward traversal has wrong/missing node");
            check(current->prev == previous, "prev does not point to preceding node");
            previous = current;
            current = current->next;
        }
        check(current == nullptr, "forward traversal has extra node or cycle");

        // Bound traversal by the expected sequence: corrupt cycles cannot hang a test.
        current = previous;
        auto it = nodes.end();
        while (it != nodes.begin())
        {
            --it;
            check(current != nullptr, "backward traversal ended before expected node");
            check(current == *it, "backward traversal has wrong/missing node");
            current = current->prev;
        }
        check(current == nullptr, "backward traversal has extra node or cycle");
    }

    void expect_detached(const list_hook_t& node)
    {
        check(node.prev == nullptr && node.next == nullptr, "erased node still has links");
    }

    struct fixture
    {
        list_hook_t      a {}, b {}, c {};
        intrusive_list_t list;

        fixture()
        {
            // Establish head/tail using the public API, then seed the public hook
            // links explicitly. This isolates erase/insert from the known push
            // bugs. Separate tests below exercise push without this setup.
            list.push_back(a);
            list.push_back(b);
            list.push_back(c);
            a = { nullptr, &b };
            b = { &a, &c };
            c = { &b, nullptr };
            expect_list(list, { &a, &b, &c });
        }
    };

    struct test_case
    {
        const char* name;
        void (*run)();
    };

    const std::array cases {
        test_case { "empty list",
                    [] {
                        intrusive_list_t list;
                        expect_list(list, {});
                    } },
        test_case { "push_front singleton",
                    [] {
                        list_hook_t      a {};
                        intrusive_list_t list;
                        list.push_front(a);
                        expect_list(list, { &a });
                    } },
        test_case { "push_back singleton",
                    [] {
                        list_hook_t      a {};
                        intrusive_list_t list;
                        list.push_back(a);
                        expect_list(list, { &a });
                    } },
        test_case { "push_front preserves existing nodes",
                    [] {
                        list_hook_t      a {}, b {}, c {};
                        intrusive_list_t list;
                        list.push_front(a);
                        list.push_front(b);
                        expect_list(list, { &b, &a });
                        list.push_front(c);
                        expect_list(list, { &c, &b, &a });
                    } },
        test_case { "push_back preserves existing nodes",
                    [] {
                        list_hook_t      a {}, b {}, c {};
                        intrusive_list_t list;
                        list.push_back(a);
                        list.push_back(b);
                        expect_list(list, { &a, &b });
                        list.push_back(c);
                        expect_list(list, { &a, &b, &c });
                    } },
        test_case { "mixed pushes",
                    [] {
                        list_hook_t      a {}, b {}, c {};
                        intrusive_list_t list;
                        list.push_back(a);
                        list.push_front(b);
                        list.push_back(c);
                        expect_list(list, { &b, &a, &c });
                    } },
        test_case { "erase singleton and reuse list",
                    [] {
                        list_hook_t      a {}, b {};
                        intrusive_list_t list;
                        list.push_front(a);
                        list.erase(a);
                        expect_list(list, {});
                        expect_detached(a);
                        list.push_back(b);
                        expect_list(list, { &b });
                        list.erase(b);
                        expect_list(list, {});
                        expect_detached(b);
                        list.push_front(a);
                        expect_list(list, { &a });
                    } },
        test_case { "erase head",
                    [] {
                        fixture f;
                        f.list.erase(f.a);
                        expect_list(f.list, { &f.b, &f.c });
                        expect_detached(f.a);
                    } },
        test_case { "erase middle",
                    [] {
                        fixture f;
                        f.list.erase(f.b);
                        expect_list(f.list, { &f.a, &f.c });
                        expect_detached(f.b);
                    } },
        test_case { "erase tail",
                    [] {
                        fixture f;
                        f.list.erase(f.c);
                        expect_list(f.list, { &f.a, &f.b });
                        expect_detached(f.c);
                    } },
        test_case { "drain from front and reuse",
                    [] {
                        fixture f;
                        f.list.erase(f.a);
                        f.list.erase(f.b);
                        f.list.erase(f.c);
                        expect_list(f.list, {});
                        expect_detached(f.a);
                        expect_detached(f.b);
                        expect_detached(f.c);
                        f.list.push_back(f.b);
                        expect_list(f.list, { &f.b });
                    } },
        test_case { "drain from back and reuse",
                    [] {
                        fixture f;
                        f.list.erase(f.c);
                        f.list.erase(f.b);
                        f.list.erase(f.a);
                        expect_list(f.list, {});
                        f.list.push_front(f.c);
                        expect_list(f.list, { &f.c });
                    } },
        test_case { "insert before singleton",
                    [] {
                        list_hook_t      a {}, b {};
                        intrusive_list_t list;
                        list.push_back(a);
                        list.insert(a, b);
                        expect_list(list, { &b, &a });
                    } },
        test_case { "insert before head",
                    [] {
                        fixture     f;
                        list_hook_t node {};
                        f.list.insert(f.a, node);
                        expect_list(f.list, { &node, &f.a, &f.b, &f.c });
                    } },
        test_case { "insert before middle",
                    [] {
                        fixture     f;
                        list_hook_t node {};
                        f.list.insert(f.b, node);
                        expect_list(f.list, { &f.a, &node, &f.b, &f.c });
                    } },
        test_case { "insert before tail",
                    [] {
                        fixture     f;
                        list_hook_t node {};
                        f.list.insert(f.c, node);
                        expect_list(f.list, { &f.a, &f.b, &node, &f.c });
                    } },
        test_case { "append after insert before tail",
                    [] {
                        fixture     f;
                        list_hook_t inserted {}, appended {};
                        f.list.insert(f.c, inserted);
                        f.list.push_back(appended);
                        expect_list(f.list, { &f.a, &f.b, &inserted, &f.c, &appended });
                    } },
        test_case { "erase location after insert preserves inserted node",
                    [] {
                        fixture     f;
                        list_hook_t node {};
                        f.list.insert(f.b, node);
                        f.list.erase(f.b);
                        expect_list(f.list, { &f.a, &node, &f.c });
                        expect_detached(f.b);
                    } },
        test_case { "removed node can move to another list",
                    [] {
                        fixture          f;
                        intrusive_list_t other;
                        f.list.erase(f.b);
                        other.push_back(f.b);
                        expect_list(f.list, { &f.a, &f.c });
                        expect_list(other, { &f.b });
                    } },
    };
}

int main()
{
    unsigned failed = 0;
    for (const auto& test : cases)
    {
        try
        {
            test.run();
            std::printf("PASS: %s\n", test.name);
        }
        catch (const std::exception& error)
        {
            ++failed;
            std::printf("FAIL: %s: %s\n", test.name, error.what());
        }
    }
    std::printf("%zu tests, %u failed\n", cases.size(), failed);
    return failed == 0 ? 0 : 1;
}
