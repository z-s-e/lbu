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

#include "lbu/abstract_stream.h"

namespace lbu {
namespace stream {

abstract_input_stream::~abstract_input_stream()
{
}

array_ref<const void> abstract_input_stream::get_read_buffer(Mode)
{
    assert(false); // Should never be called, only provided for subclasses that are not buffered
    return {};
}

abstract_output_stream::~abstract_output_stream()
{
}

array_ref<void> abstract_output_stream::get_write_buffer(Mode)
{
    assert(false); // Should never be called, only provided for subclasses that are not buffered
    return {};
}

bool abstract_output_stream::write_buffer_flush(Mode)
{
    assert(false); // Should never be called, only provided for subclasses that are not buffered
    return {};
}


} // namespace stream
} // namespace lbu
