/*
 * Copyright (c) 2019, Tom Oleson <tom dot oleson at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * The names of its contributors may NOT be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "logger.h"

cm_log::multiplex_logger mx_log;
cm_log::rolling_file_logger
    app_log("./log/", "vortex", ".log", ((24 * 60) * 60) /*seconds*/, 6 /* retain # */);

void vortex::init_logs() {

    cm_log::console.set_log_level(cm_log::level::trace);
    cm_log::console.set_date_time_format("%m/%d/%Y %H:%M:%S");
    cm_log::console.set_message_format("${date_time} [${lvl}]: ${msg}");

    app_log.set_log_level(cm_log::level::trace);
    app_log.set_date_time_format("%m/%d/%Y %H:%M:%S");
    app_log.set_message_format("${date_time}${millis} [${lvl}] <${thread}>: ${msg}"); 

    mx_log.set_log_level(cm_log::level::trace);
    mx_log.add(cm_log::console);
    mx_log.add(app_log);

    set_default_logger(&mx_log);
}
