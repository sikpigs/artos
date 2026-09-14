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
        bool empty() const noexcept
        {
            return head == nullptr;
        }

        void push_front(list_hook_t& node) noexcept
        {
            node.prev = nullptr;
            node.next = head;

            if (head)
            {
                head->prev = std::addressof(node);
            }
            else
            {
                tail = std::addressof(node);
            }

            head = std::addressof(node);
        }

        void push_back(list_hook_t& node) noexcept
        {
            node.next = nullptr;
            node.prev = tail;

            if (tail)
            {
                tail->next = std::addressof(node);
            }
            else
            {
                head = std::addressof(node);
            }

            tail = std::addressof(node);
        }

        void erase(list_hook_t& node) noexcept
        {
            if (node.prev)
            {
                node.prev->next = node.next;
            }
            else
            {
                head = node.next;
            }

            if (node.next)
            {
                node.next->prev = node.prev;
            }
            else
            {
                tail = node.prev;
            }

            node.prev = nullptr;
            node.next = nullptr;
        }

        void insert(list_hook_t& location, list_hook_t& node) noexcept
        {
            node.prev = location.prev;

            if (location.prev)
            {
                location.prev->next = std::addressof(node);
                node.prev = location.prev;
                node.next = std::addressof(location);
            }
            else
            {
                node.next = head;
                tail = head;
                head = std::addressof(node);
            }

            location.prev = std::addressof(node);
        }

        list_hook_t* front() noexcept
        {
            return head;
        }

        const list_hook_t* front() const noexcept
        {
            return head;
        }

    private:
        list_hook_t* head {};
        list_hook_t* tail {};
    };
}