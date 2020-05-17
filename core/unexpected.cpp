/* Copyright 2020 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lbu/unexpected.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <new>
#include <system_error>

namespace lbu {

    void unexpected_system_error(int errnum)
    {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw std::system_error(errnum, std::system_category());
#else
        std::fprintf(stderr, "Unexpected system error: %s\n", std::strerror(errnum));
        std::terminate();
#endif
    }

    void unexpected_memory_exhaustion()
    {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw std::bad_alloc();
#else
        std::fprintf(stderr, "Unexpected memory exhaustion\n");
        std::terminate();
#endif
    }

    void unexpected_call()
    {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw std::logic_error("unexpected call");
#else
        std::fprintf(stderr, "Unexpected call\n");
        std::terminate();
#endif
    }

} // namespace lbu
