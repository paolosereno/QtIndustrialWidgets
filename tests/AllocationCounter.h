// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

// Counts the calls to the global operator new of the current thread while an AllocationScope is alive (#8).
//
// Include this header in exactly one source file of a test executable: it replaces the global operator new and
// operator delete of that executable. Code compiled into the executable is always counted. Whether calls made inside
// shared libraries are counted depends on the platform: on Linux (ELF) the libraries' calls bind to this replacement;
// with a Windows DLL or a macOS two-level-namespace dylib they do not. Each test therefore checks first that the
// counter sees an allocation it knows of.

#include <cstddef>
#include <cstdlib>
#include <new>

namespace AllocationCounter {

inline thread_local bool counting = false;
inline thread_local long long count = 0;

/** \brief Counts the operator new calls of this thread while alive; count() returns them. */
class AllocationScope
{
public:
    AllocationScope() : m_start(AllocationCounter::count) { counting = true; }
    ~AllocationScope() { counting = false; }
    AllocationScope(const AllocationScope &) = delete;
    AllocationScope &operator=(const AllocationScope &) = delete;
    [[nodiscard]] long long count() const { return AllocationCounter::count - m_start; }

private:
    long long m_start;
};

inline void *allocate(std::size_t size)
{
    if (counting) ++count;
    if (void *p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}

} // namespace AllocationCounter

void *operator new(std::size_t size) { return AllocationCounter::allocate(size); }
void *operator new[](std::size_t size) { return AllocationCounter::allocate(size); }
void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    try {
        return AllocationCounter::allocate(size);
    } catch (...) {
        return nullptr;
    }
}
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    try {
        return AllocationCounter::allocate(size);
    } catch (...) {
        return nullptr;
    }
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { std::free(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { std::free(p); }
