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
R"( \ \____/  \   \ \ Made in the U.S.A.)""\n"
R"(  \____ \  /   / /)""\n"
R"(       \ \____/ /)""\n"
R"(        \______/)""\n";


void usage(int argc, char *argv[]) {
    printf("usage: %s [-p<port>] [-l<level>] [-i<interval>] [-k<keep>] [-c host:port] [-v]\n", argv[0]);
    puts("");
    puts("-p port       Listen on port");
    puts("-l level      Log level");
    puts("-i interval   Cache rotation interval");
    puts("-k keep       Number of journal logs to keep in rotation");
    puts("-c host:port  Connect to host and port");
    puts("-n name       Name for this instance");
    puts("-v            Output version/build info to console");
    puts("");
}


int main(int argc, char *argv[]) {

    puts(__banner__);

    int opt;
    int port = 54000;
    int log_lvl = (cm_log::level::en) cm_log::level::trace;
    bool version = false;
    int interval = 0;
    int keep = 0;
    std::string host_name = "localhost";
    int host_port = -1;
    std::string instance_name = "vortex";

    std::vector<std::string> v;

    while((opt = getopt(argc, argv, "hl:p:i:k:c:n:v")) != -1) {
        switch(opt) {
            case 'p':
                port = atoi(optarg);
                break;

            case 'v':
                version = true;
                break;

            case 'c':
                v = cm_util::split(optarg, ':');
                if(v.size() == 2) {
                    host_name = v[0];
                    host_port = atoi(v[1].c_str());
                }
                break;

            case 'n':
                instance_name = std::string(optarg);
                break;

            case 'l':
                log_lvl = atoi(optarg);
                break;

            case 'i':
                interval = atoi(optarg);
                break;

            case 'k':
                keep = atoi(optarg);
                break;                

            case 'h':
            default:
                printf("usage: %s [-p<port>] [-l<level>] [-i<interval>] [-k<keep>] [-c host:port] [-n name] [-v]\n", argv[0]);
                exit(0);
        }
    }

    vortex::init_logs((cm_log::level::en) log_lvl, interval, keep);
    cm_log::always(cm_util::format("VORTEX %s build: %s %s", VERSION ,__DATE__,__TIME__));
    
    vortex::init_storage();
    vortex::run(port, host_name, host_port, instance_name);

    return 0;
}
