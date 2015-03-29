#include "free_list.hpp"

#include <cassert>
#include <cstddef>
#include <functional>
#include <utility>

using namespace foonathan::memory;
using namespace detail;

namespace
{
    // pre: ptr
    char*& next(void* ptr) noexcept
    {
        return *static_cast<char**>(ptr);
    }

    // pre: mem, el_size > sizeof(mem), size >= el_size
    void* build_list(void* &mem, std::size_t el_size, std::size_t size) noexcept
    {
        auto no_blocks = size / el_size;
        auto ptr = static_cast<char*>(mem);
        for (std::size_t i = 0u; i != no_blocks - 1; ++i, ptr += el_size)
            next(ptr) = ptr + el_size;
        return ptr;
    }

    // pre: list, mem
    std::pair<void*, void*> find_position(void *list, void *mem) noexcept
    {
        auto greater = std::greater<char*>();
        auto prev = static_cast<char*>(list);
        if (greater(prev, static_cast<char*>(mem)))
            return std::make_pair(nullptr, list);

        auto ptr = next(prev);
        while (ptr)
        {
            if (greater(ptr, static_cast<char*>(mem)))
                break;
            prev = ptr;
            ptr = next(ptr);
        }

        return std::make_pair(prev, ptr);
    }

    // pre: beg
    bool check_n(char* &cur, std::size_t n, std::size_t el_size) noexcept
    {
        --n; // we already have one (cur)
        if (n == 0u)
            return true;
        for (; cur; cur += el_size)
        {
            if (next(cur) == cur + el_size)
            {
                if (--n == 0)
                    break;
            }
            else
                return false;
        }
        // next(cur) is the last element of the array
        cur = next(cur);
        return true;
    }
}

constexpr std::size_t free_memory_list::min_element_size;
constexpr std::size_t free_memory_list::min_element_alignment;

free_memory_list::free_memory_list(std::size_t el_size) noexcept
: first_(nullptr), el_size_(std::max(min_element_size, el_size)), capacity_(0u)
{}

free_memory_list::free_memory_list(std::size_t el_size,
                                   void *mem, std::size_t size) noexcept
: free_memory_list(el_size)
{
    insert(mem, size);
}

void free_memory_list::insert(void *mem, std::size_t size) noexcept
{
    capacity_ += size;
    auto last = build_list(mem, el_size_, size);
    next(last) = first_;
    first_ = static_cast<char*>(mem);
}

void free_memory_list::insert_ordered(void *mem, std::size_t size) noexcept
{
    capacity_ += size;
    if (empty())
        return insert(mem, size);

    auto pos = find_position(first_, mem);
    insert_between(pos.first, pos.second,
                   mem, size);
}

void free_memory_list::insert_between(void *pre, void *after,
                                      void *mem, std::size_t size) noexcept
{
    auto last = build_list(mem, el_size_, size);

    if (pre)
        next(pre) = static_cast<char*>(mem);
    else
        first_ = static_cast<char*>(mem);

    next(last) = static_cast<char*>(after);
}

void* free_memory_list::allocate() noexcept
{
    capacity_ -= el_size_;
    auto block = first_;
    first_ = next(first_);
    return block;
}

void* free_memory_list::allocate(std::size_t n) noexcept
{
    capacity_ -= n * el_size_;
    for(auto cur = first_; cur; cur = next(cur))
    {
        auto start = cur;
        if (check_n(cur, n, el_size_))
        {
            // found n continuos nodes
            // cur is the last element, next(cur) is the next free node
            first_ = next(cur);
            return start;
        }
    }
    return nullptr;
}

void free_memory_list::deallocate(void *ptr) noexcept
{
    capacity_ += el_size_;
    next(ptr) = first_;
    first_ = static_cast<char*>(ptr);
}

void free_memory_list::deallocate_ordered(void *ptr) noexcept
{
    capacity_ += el_size_;
    auto pos = find_position(first_, ptr);
    insert_between(pos.first, pos.second, ptr, el_size_);
}
