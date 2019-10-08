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

extern cm_log::rolling_file_logger journal;

class journal_processor: public cm_cache::scanner_processor {

public:
    bool do_add(const std::string &name, const std::string &value, std::string &result) {
        cm_store::mem_store.set(name, value);
        return true;
    }

    bool do_read(const std::string &name, std::string &result) {
        return true;
    }

    bool do_remove(const std::string &name, std::string &result) {
        int num = cm_store::mem_store.remove(name);
        return true;
    }

    bool do_watch(const std::string &name, const std::string &tag, std::string &result) {
        return true;
    }

    bool do_result(const std::string &result) {
        return true;
    }

    bool do_input(const std::string &in_str, std::string &expr) {
        //cm_log::info(cm_util::format("%s", in_str.c_str()));
        if(in_str.size() > 2) {
            // seek space between timestamp and expr
            int pos = in_str.find(" ");
            if(pos > 1) {
                expr.assign( in_str.substr(pos + 1) );
                return true;
            }
            else {
                // raw input line with no timestamp
                expr.assign(in_str);
            }
        }
        return true;
    }

    bool do_error(const std::string &expr, const std::string &err, std::string &result) {
        cm_log::error(result.assign(cm_util::format("error: %s", err.c_str(), expr.c_str())));
        return false;
    }
};

void vortex::init_storage() {

    journal_processor processor;
    cm_cache::cache cache(&processor);

    std::vector<std::string> matches;

    // scan ./journal directory and match any that end with ".log"
    // note: use of raw string literal to avoid need to escape \ in regex
    cm_util::dir_scan("./journal", R"(.+\.log$)", matches);

    if(matches.size() > 1) {
        std::sort(matches.begin(), matches.end());
    }
    
    int count = 0;
    for(auto name : matches) {
        std::string path = "./journal/" + name;
        // if not the current log file, add to rotation list
        if(name != "data.log") {
            journal.rotation_list_add(path);
        }
        count = cache.load(path);
        cm_log::info(cm_util::format("%s: %d", name.c_str(), count ));
    }
    cm_log::info(cm_util::format("journals: %d", matches.size()));
}
