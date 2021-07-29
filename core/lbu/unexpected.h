/* Copyright 2020 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBLBU_UNEXPECTED_H
#define LIBLBU_UNEXPECTED_H

#include "lbu/lbu_global.h"

namespace lbu {

    void LIBLBU_EXPORT unexpected_system_error(int errnum);
    void LIBLBU_EXPORT unexpected_memory_exhaustion();
    void LIBLBU_EXPORT unexpected_call();

} // namespace lbu

#endif // LIBLBU_UNEXPECTED_H
