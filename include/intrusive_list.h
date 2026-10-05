#pragma once

#include <memory>

namespace artos {
    struct list_hook_t
    {
        list_hook_t* prev {};
        list_hook_t* next {};
    };

    class intrusive_list_t
    {
    public:
        intrusive_list_t() noexcept
        {
            root.prev = std::addressof(root);
            root.next = std::addressof(root);
        }

        [[nodiscard]]
        bool empty() const noexcept
        {
            return root.next == std::addressof(root);
        }

        void push_front(list_hook_t& node) noexcept
        {
            link_before(*root.next, node);
        }

        void push_back(list_hook_t& node) noexcept
        {
            link_before(root, node);
        }

        list_hook_t* pop_front() noexcept
        {
            if (empty())
            {
                return nullptr;
            }

            auto* front = root.next;
            erase(*front);
            return front;
        }

        list_hook_t* pop_back() noexcept
        {
            if (empty())
            {
                return nullptr;
            }

            auto* end = root.prev;
            erase(*end);
            return end;
        }

        void erase(list_hook_t& node) noexcept
        {
            unlink(node);

            node.prev = nullptr;
            node.next = nullptr;
        }

        void insert(list_hook_t& location, list_hook_t& node) noexcept
        {
            link_before(location, node);
        }

        list_hook_t* front() noexcept
        {
            return root.next;
        }

        const list_hook_t* front() const noexcept
        {
            return root.next;
        }

        list_hook_t* end() noexcept
        {
            return std::addressof(root);
        }

        const list_hook_t* end() const noexcept
        {
            return std::addressof(root);
        }

        void rotate_after(list_hook_t& node) noexcept
        {
            node.prev->next = node.next;
            node.next->prev = node.prev;
            node.prev = root.prev;
            root.prev->next = std::addressof(node);
            root.prev = std::addressof(node);
            node.next = std::addressof(root);
        }

        void clear() noexcept
        {
            auto* p = front();
            while (p != end())
            {
                auto* n = p->next;
                p->next = nullptr;
                p->prev = nullptr;
                p = n;
            }

            root.next = std::addressof(root);
            root.prev = std::addressof(root);
        }

    private:
        list_hook_t root {};

        static void link_before(list_hook_t& location, list_hook_t& node) noexcept
        {
            node.prev = location.prev;
            node.next = std::addressof(location);

            location.prev->next = std::addressof(node);
            location.prev = std::addressof(node);
        }

        static void unlink(list_hook_t& node) noexcept
        {
            node.prev->next = node.next;
            node.next->prev = node.prev;
        }
    };
}
