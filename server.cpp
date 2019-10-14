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


struct watcher {
    int fd = -1;         // notify socket
    std::string tag;

    watcher() {}
    ~watcher() {}
    
    watcher(const int fd_, const std::string tag_): fd(fd_), tag(tag_) {}
    watcher(const watcher &r): fd(r.fd), tag(r.tag) {}
    
    watcher &operator = (const watcher &r) {
        fd = r.fd;
        tag = r.tag;
        return *this;
    }
};


class watcher_store: protected cm::mutex {

protected:
    // unordered map for faster access vs. map using buckets
    std::unordered_map<std::string,std::vector<watcher>> _map;

public:

    bool check(const std::string &name) {
        lock();
        bool b = _map.find(name) != _map.end();
        unlock();
        return b;
    }

    bool add(const std::string &name, const watcher &w) {
        lock();
        std::vector<watcher> &v = _map[name];
        v.push_back(w);
        unlock();
        return true;
    }

    size_t remove(const std::string &name) {
        lock();
        size_t num_erased = _map.erase(name);
        unlock();
        return num_erased;   
    }
    
    size_t remove(int fd) {
        size_t num_erased = 0;
        lock();

        for(auto i = _map.begin(); i != _map.end();) {

            std::vector<watcher> &v = i->second;

            // remove matching fd
            for(auto it = v.begin(); it != v.end();) {
                if(it->fd == fd) {
                    it = v.erase(it);
                    num_erased++;
                }
                else {
                    it++;
                }
            }

            // remove if no other sockets watching this key 
            if(v.size() == 0) {
                std::string name = std::move(i->first);    
                i = _map.erase(i);
                cm_log::info(cm_util::format("removed key: [%s] (no more watchers)", name.c_str()));
            }
            else {
                i++;
            }
        }
        
        unlock();
        return num_erased;   
    }

    void notify(const std::string &name, const std::string &value) {
        lock();
        if(_map.find(name) != _map.end()) {
            std::vector<watcher> &v = _map[name];
            for(auto &_watcher: v) {
                cm_net::send(_watcher.fd,
                cm_util::format("%s:%s\n", _watcher.tag.c_str(), value.c_str()));               
            }
        }
        unlock();
    }

    size_t size() {
        lock();
        size_t size = _map.size();
        unlock();
        return size;
    }

    void clear() {
        lock();
        _map.clear();
        unlock();
    }
};


watcher_store watchers;


class vortex_processor: public cm_cache::scanner_processor {

public:
    bool do_add(const std::string &name, const std::string &value, cm_cache::cache_event &event) {

        cm_log::info(cm_util::format("+%s %s", name.c_str(), value.c_str()));

        // journal first to guard rotation
        journal.info(event.request);

        cm_store::mem_store.set(name, value);

        event.name.assign(name);
        event.value.assign(value);
        event.notify = true;
        event.result.assign("OK");
        do_result(event);

        return true;
    }

    bool do_read(const std::string &name, cm_cache::cache_event &event) {
    
        cm_log::info(cm_util::format("$%s", name.c_str()));

        journal.lock();  // guard rotationn
        event.value = cm_store::mem_store.find(name);
        journal.unlock();

        event.name.assign(name);
        if(event.value.size() > 0) {
            event.result.assign(cm_util::format("%s", event.value.c_str()));
        }
        else {
            event.result.assign("NF");
        }
        do_result(event);
        return true;
    }

    bool do_remove(const std::string &name, cm_cache::cache_event &event) {
    
        cm_log::info(cm_util::format("-%s", name.c_str()));

        // journal first to guard rotation
        journal.info(event.request);

        int num = cm_store::mem_store.remove(name);
        event.result.assign(cm_util::format("(%d)", num));
        do_result(event);

        return true;
    }

    bool do_watch(const std::string &name, const std::string &tag, cm_cache::cache_event &event) {

        cm_log::info(cm_util::format("*%s #%s", name.c_str(), tag.c_str()));

        journal.lock();     // guard rotationn
        event.value = cm_store::mem_store.find(name);
        journal.unlock();

        event.name = name;
        event.tag = tag;

        watchers.add(name, watcher(event.fd, tag));

        event.result.assign(cm_util::format("%s:%s", tag.c_str(), event.value.c_str()));
        do_result(event);

        return true;
    }

    bool do_result(cm_cache::cache_event &event) {
        cm_log::info(cm_util::format("%s", event.result.c_str()));
        return true;
    }

    bool do_input(const std::string &in_str, cm_cache::cache_event &event) { 
        return true;
    }

    bool do_error(const std::string &expr, const std::string &err, cm_cache::cache_event &event) {
        event.result.assign(cm_util::format("error: %s", err.c_str(), expr.c_str()));
        cm_log::error(event.result);
        return false;
    }
};

vortex_processor processor;

void request_handler(void *arg) {

    cm_net::input_event *event = (cm_net::input_event *) arg;
    std::string request = std::move(event->msg);
    int socket = event->fd;
    bool eof = event->eof;

    // new connection event (not input)
    if(event->connect) {
        return;        
    }

    // if this in an EOF event (client disconnected)
    if(event->eof) {
        // remove socket from all watchers
        int num = watchers.remove(socket);
        if(num > 0) {
            cm_log::info(cm_util::format("%d: socket removed from %d watcher(s)", socket, num));
        }
        return;
    }


    cm_log::info(cm_util::format("%d: received request:", socket));
    cm_log::hex_dump(cm_log::level::info, request.c_str(), request.size(), 16);

    cm_cache::cache cache(&processor);
    cm_cache::cache_event req_event;
    
    req_event.fd = socket;
    req_event.request.assign(request);

    cache.eval(request, req_event);
    
    cm_net::send(socket, req_event.result.append("\n"));

    if(req_event.notify) {
        watchers.notify(req_event.name, req_event.value);
    }
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
