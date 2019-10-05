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

#include "storage.h"

class vortex_processor: public cm_cache::scanner_processor {

public:
    bool do_add(const std::string &name, const std::string &value) {
        cm_log::info(cm_util::format("+%s %s", name.c_str(), value.c_str()));

        cm_store::mem_store.set(name, value);
        return true;
    }

    bool do_read(const std::string &name) {
        cm_log::info(cm_util::format("$%s", name.c_str()));

        std::string value = cm_store::mem_store.find(name);
        if(value.size() > 0) {
            cm_log::info(cm_util::format("%s", value.c_str()));
        }
        else {
            cm_log::info("NF");
        }
        return true;
    }

    bool do_remove(const std::string &name) {
        cm_log::info(cm_util::format("-%s", name.c_str()));

        int num = cm_store::mem_store.remove(name);
        cm_log::info(cm_util::format("(%d)", num));
        return true;
    }

    bool do_watch(const std::string &name, const std::string &tag) {
        cm_log::info(cm_util::format("*%s #%s", name.c_str(), tag.c_str()));

        std::string value = cm_store::mem_store.find(name);
        cm_log::info(cm_util::format("%s:%s", tag.c_str(), value.c_str()));
        return true;
    }

    bool do_error(const std::string &expr, const std::string &err) {
        cm_log::error(cm_util::format("error: %s", err.c_str(), expr.c_str()));
        return false;
    }
};

vortex_processor processor;
cm_cache::cache cache(&processor);

void vortex::init_storage() {

    // cache.eval("+foo 'bar'");   // add
    // cache.eval("$foo");         // read
    // cache.eval("*foo #0");      // watch
    // cache.eval("-foo");         // remove
    // cache.eval("$foo");         // read

}
