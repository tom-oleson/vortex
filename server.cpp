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

#include "server.h"

extern vortex::journal_logger journal;

class vortex_processor: public cm_cache::scanner_processor {

public:
    bool do_add(const std::string &name, const std::string &value, std::string &result) {
        cm_log::info(cm_util::format("+%s %s", name.c_str(), value.c_str()));

        cm_store::mem_store.set(name, value);
        do_result(result.assign("OK"));
        return true;
    }

    bool do_read(const std::string &name, std::string &result) {
        cm_log::info(cm_util::format("$%s", name.c_str()));

        std::string value = cm_store::mem_store.find(name);
        if(value.size() > 0) {
            do_result(result.assign(cm_util::format("%s", value.c_str())));
        }
        else {
            do_result(result.assign("NF"));
        }
        return true;
    }

    bool do_remove(const std::string &name, std::string &result) {
        cm_log::info(cm_util::format("-%s", name.c_str()));

        int num = cm_store::mem_store.remove(name);
        do_result(result.assign(cm_util::format("(%d)", num)));
        return true;
    }

    bool do_watch(const std::string &name, const std::string &tag, std::string &result) {
        cm_log::info(cm_util::format("*%s #%s", name.c_str(), tag.c_str()));

        std::string value = cm_store::mem_store.find(name);
        do_result(result.assign(cm_util::format("%s:%s", tag.c_str(), value.c_str())));
        return true;
    }

    bool do_result(const std::string &result) {
        cm_log::info(cm_util::format("%s", result.c_str()));
        return true;
    }

    bool do_input(const std::string &in_str, std::string &expr) {
        return true;
    }

    bool do_error(const std::string &expr, const std::string &err, std::string &result) {
        cm_log::error(result.assign(cm_util::format("error: %s", err.c_str(), expr.c_str())));
        return false;
    }
};

vortex_processor processor;

void request_handler(void *arg) {

    cm_net::input_event *event = (cm_net::input_event *) arg;
    std::string request = std::move(event->msg);
    int socket = event->fd;

    // write all add/remove requests to journal log
    const char op = request[0];
    if(op == '+' || op == '-') {
        journal.info(request);
    }

    cm_log::info(cm_util::format("%d: received request:", socket));
    cm_log::hex_dump(cm_log::level::info, request.c_str(), request.size(), 16);

    cm_cache::cache cache(&processor);

    std::string response;
    cache.eval(request, response);
    
    cm_net::send(socket, response.append("\n"));
}

void request_dealloc(void *arg) {
    delete (cm_net::input_event *) arg;
}

void vortex::run(int port) {


    // create thread pool that will do work for the server
    cm_thread::pool thread_pool(6);

    // startup tcp server
    cm_net::pool_server server(port, &thread_pool, request_handler,
        request_dealloc);

    while(1) {
        timespec delay = {0, 100000000};   // 100 ms
        nanosleep(&delay, NULL);
    }

    // wait for pool_server threads to complete all work tasks
    thread_pool.wait_all();

}
