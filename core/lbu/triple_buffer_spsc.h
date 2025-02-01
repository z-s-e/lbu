/* Copyright 2025 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_TRIPLE_BUFFER_SPSC_H
#define LIBLBU_TRIPLE_BUFFER_SPSC_H

#include <atomic>
#include <stdint.h>

namespace lbu {

template< class T >
class triple_buffer_spsc {
public:
    T& current_producer_buffer() { return buffer[producer_idx]; }
    T& current_consumer_buffer() { return buffer[consumer_idx]; }

    void swap_producer_buffer() { producer_idx = idle_idx.exchange(producer_idx); }
    void swap_consumer_buffer() { consumer_idx = idle_idx.exchange(consumer_idx); }

    T buffer[3] = {};

private:
    uint8_t producer_idx = 0;
    uint8_t consumer_idx = 1;
    std::atomic<uint8_t> idle_idx = {2};
};

}

#endif
