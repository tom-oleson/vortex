
#include <unistd.h>     // for getopt()
#include "vortex.h"

const char *__banner__ =
R"(         ____)""\n"
R"(        /    \)""\n"
R"(   ____/   __ \)""\n"  
R"(  /    \__/   /  VORTEX)""\n"
R"( /  \__/  \__/   Rotational Data Cache)""\n"
R"( \     \__/  \   Copyright (C) 2019, Tom Oleson, All Rights Reserved.)""\n"
R"(  \____/  \   \)""\n"
R"(       \  /   /)""\n"
R"(        \____/)""\n";

int main(int argc, char *argv[]) {

    puts(__banner__);

    int opt;
    int port = 56000;
    bool verbose = false;

    while((opt = getopt(argc, argv, "hp:v")) != -1) {
        switch(opt) {
            case 'p':
                port = atoi(optarg);
                break;

            case 'v':
                verbose = true;
                break;

            case 'h':
            default:
                printf("usage: %s [-p port] [-v]\n", argv[0]);
                exit(0);
        }
    }

    vortex::init_logs();
    cm_log::always(cm_util::format("VORTEX %s build: %s %s", VERSION ,__DATE__,__TIME__));
    cm_log::info(cm_util::format("Listening on port: %d", port));

    return 0;
}
