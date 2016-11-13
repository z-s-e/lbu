/* Copyright 2015-2016 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 *
 * This file is part of the lbu library.
 *
 * lbu is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the licence, or (at your option) any later version.
 *
 * lbu is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef LIBLBU_TIME_H
#define LIBLBU_TIME_H

#include <chrono>
#include <time.h>

namespace lbu {
namespace time {

    inline struct timespec duration_to_timespec(std::chrono::nanoseconds ns)
    {
        struct timespec ts;
        ts.tv_nsec = ns.count() % std::nano::den;
        ts.tv_sec = ns.count() / std::nano::den;
        return ts;
    }

    inline std::chrono::nanoseconds timespec_to_duration(struct timespec ts)
    {
        return std::chrono::nanoseconds(std::chrono::nanoseconds::rep(ts.tv_sec) * std::nano::den + ts.tv_nsec);
    }

} // namespace time
} // namespace lbu

#endif // LIBLBU_TIME_H

