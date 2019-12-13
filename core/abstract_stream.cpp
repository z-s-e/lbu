/* Copyright 2015-2016 Zeno Sebastian Endemann <zeno.endemann@googlemail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lbu/abstract_stream.h"

namespace lbu {
namespace stream {

static void throw_missing_impl()
{
    throw std::logic_error("implementation missing");
}

abstract_input_stream::~abstract_input_stream()
{
}

array_ref<const void> abstract_input_stream::get_read_buffer(Mode)
{
    // Should never be called, only provided for subclasses that are not buffered
    throw_missing_impl();
    return {};
}

abstract_output_stream::~abstract_output_stream()
{
}

array_ref<void> abstract_output_stream::get_write_buffer(Mode)
{
    // Should never be called, only provided for subclasses that are not buffered
    throw_missing_impl();
    return {};
}

bool abstract_output_stream::write_buffer_flush(Mode)
{
    // Should never be called, only provided for subclasses that are not buffered
    throw_missing_impl();
    return {};
}


} // namespace stream
} // namespace lbu
