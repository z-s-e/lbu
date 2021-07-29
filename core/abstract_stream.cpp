/* Copyright 2015-2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lbu/abstract_stream.h"

#include "lbu/unexpected.h"

namespace lbu {
namespace stream {

abstract_input_stream::~abstract_input_stream()
{
}

array_ref<const void> abstract_input_stream::get_read_buffer(Mode)
{
    // Should never be called, only provided for subclasses that are not buffered
    unexpected_call();
    return {};
}

abstract_output_stream::~abstract_output_stream()
{
}

array_ref<void> abstract_output_stream::get_write_buffer(Mode)
{
    // Should never be called, only provided for subclasses that are not buffered
    unexpected_call();
    return {};
}

bool abstract_output_stream::write_buffer_flush(Mode)
{
    // Should never be called, only provided for subclasses that are not buffered
    unexpected_call();
    return {};
}


} // namespace stream
} // namespace lbu
