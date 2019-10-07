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

#include <unistd.h>     // for getopt()
#include "vortex.h"

const char *__banner__ =
R"(         ______)""\n"
R"(        / ____ \)""\n"
R"(   ____/ /    \ \)""\n"
R"(  / ____/   __ \ \)""\n"  
R"( / /    \__/   / / VORTEX)""\n"
R"(/ /  \__/  \__/ /  Rotational Data Cache)""\n"
R"(\ \     \__/  \ \  Copyright (C) 2019, Tom Oleson, All Rights Reserved.)""\n"
R"( \ \____/  \   \ \)""\n"
R"(  \____ \  /   / /)""\n"
R"(       \ \____/ /)""\n"
R"(        \______/)""\n";

int main(int argc, char *argv[]) {

    puts(__banner__);

    int opt;
    int port = 56000;
    int log_lvl = (cm_log::level::en) cm_log::level::info;
    bool verbose = false;

    while((opt = getopt(argc, argv, "hl:p:v")) != -1) {
        switch(opt) {
            case 'p':
                port = atoi(optarg);
                break;

            case 'v':
                verbose = true;
                break;

            case 'l':
                log_lvl = atoi(optarg);
                break;

            case 'h':
            default:
                printf("usage: %s [-p port] [-l level] [-v]\n", argv[0]);
                exit(0);
        }
    }

    vortex::init_logs((cm_log::level::en) log_lvl);
    cm_log::always(cm_util::format("VORTEX %s build: %s %s", VERSION ,__DATE__,__TIME__));
    
    vortex::init_storage();
    vortex::run(port);

    return 0;
}
