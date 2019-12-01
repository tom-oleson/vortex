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
    std::string pub;
    bool remove = false;

    watcher() {}
    ~watcher() {}
    
    watcher(const int fd_, const std::string tag_, const std::string pub_, bool remove_):
     fd(fd_), tag(tag_), pub(pub_), remove(remove_) {}
    watcher(const watcher &r): fd(r.fd), tag(r.tag), pub(r.pub), remove(r.remove) {}
    
    watcher &operator = (const watcher &r) {
        fd = r.fd;
        tag = r.tag;
        pub = r.pub;
        remove = r.remove;
        return *this;
    }
};

cm_queue::double_queue<std::string> pub_queue;

class publish_store: protected cm::mutex {

protected:
    // unordered map for faster access vs. map using buckets
    std::unordered_map<std::string,std::set<std::string>> _map;

public:

    bool check(const std::string &name) {
        lock();
        bool b = _map.find(name) != _map.end();
        unlock();
        return b;
    }

    bool add(const std::string &name, const std::string &value) {
        lock();
        std::set<std::string> &s = _map[name];
        s.insert(value);
        unlock();
        return true;
    }

    bool publish(const std::string &name, const std::string &value, cm_cache::cache_event &event) {
        lock();
        bool published = false;
        if(_map.find(name) != _map.end()) {
            std::set<std::string> &s = _map[name];
            for(auto &key: s) {

                CM_LOG_TRACE {
                    cm_log::info(cm_util::format("%d: publish: %s --> %s", event.fd, name.c_str(), key.c_str()));
                    cm_log::hex_dump(cm_log::level::info, value.c_str(), value.size(), 16);
                }

                // publish data to specified key
                std::string request = key;   //+key
                request.append(" ");
                request.append(value); 

                // add request to pub queue
                pub_queue.push_back( request );
                published = true;
            }
        }
        unlock();
        return published;
    }

    size_t remove(const std::string &name) {
        lock();
        size_t num_erased = _map.erase(name);
        unlock();
        return num_erased;   
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

publish_store publishers;

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

    bool check(const std::string &name, const watcher &w) {
        lock();
        std::vector<watcher> &v = _map[name];
 
        // scan for matching watcher
        bool found = false;
        for(auto it = v.begin(); it != v.end(); it++) {
            if(it->fd == w.fd && it->tag == w.tag && it->remove == w.remove) {
                found = true;
                break;
            }
        }       
        unlock();
        return found;
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
                CM_LOG_TRACE { cm_log::info(cm_util::format("removed key: [%s] (no more watchers)", name.c_str())); }

                // clean up publishers, there are no active sockets
                // interacting with this name...                
                if(publishers.check(name)) {
                    publishers.remove(name);
                }
            }
            else {
                i++;
            }
        }
        
        unlock();
        return num_erased;   
    }

    bool notify(const std::string &name, const std::string &value, cm_cache::cache_event &event) {
        lock();
        bool do_remove = false;
        if(_map.find(name) != _map.end()) {
            std::vector<watcher> &v = _map[name];
            for(auto &_watcher: v) {

                CM_LOG_TRACE {
                    cm_log::info(cm_util::format("%d: notify: %s #%s %s", _watcher.fd, name.c_str(), _watcher.tag.c_str(), _watcher.pub.c_str()));
                    cm_log::hex_dump(cm_log::level::info, value.c_str(), value.size(), 16);
                }

                cm_net::send(_watcher.fd,
                cm_util::format("%s:%s\n", _watcher.tag.c_str(), value.c_str()));

                CM_LOG_TRACE { cm_log::trace(cm_util::format("_watcher.pub.size() = %d", _watcher.pub.size())); }

                if(_watcher.pub.size() > 0) {
                    std::string pub_name = _watcher.pub.substr(1); // remove + on +key
                    CM_LOG_TRACE { cm_log::trace(cm_util::format("published on notify: %s", pub_name.c_str())); }
                    if(publishers.publish(pub_name, value, event)) {
                        CM_LOG_TRACE { cm_log::trace(cm_util::format("DONE: published on notify: %s", pub_name.c_str())); }
                    }
                }

                if(_watcher.remove) { 
                    do_remove = true;
                }
            }
        }
        unlock();
        return do_remove;
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

        //cm_log::info(cm_util::format("+%s %s", name.c_str(), value.c_str()));

        // journal first to guard rotation
        journal.info(event.request);

        cm_store::mem_store.set(name, value);

        event.name.assign(name);
        event.value.assign(value);
        event.notify = true;
        event.result.assign(cm_util::format("OK:%s", name.c_str()));
        return do_result(event);
    }

    bool do_read(const std::string &name, cm_cache::cache_event &event) {
    
        //cm_log::info(cm_util::format("$%s", name.c_str()));

        journal.lock();  // guard rotation
        event.value = cm_store::mem_store.find(name);
        journal.unlock();

        event.name.assign(name);
        if(event.value.size() > 0) {
            event.result.assign(cm_util::format("%s:%s", name.c_str(), event.value.c_str()));
        }
        else {
            event.result.assign(cm_util::format("NF:%s", name.c_str()));
        }
        return do_result(event);
    }

    bool do_read_remove(const std::string &name, cm_cache::cache_event &event) {
    
        //cm_log::info(cm_util::format("!%s", name.c_str()));

        journal.lock();  // guard rotation
        event.value = cm_store::mem_store.find(name);
        journal.unlock();

        event.name.assign(name);
        if(event.value.size() > 0) {
            event.result.assign(cm_util::format("%s:%s", name.c_str(), event.value.c_str()));

            journal.info(event.request);
            int num = cm_store::mem_store.remove(name);
        }
        else {
            event.result.assign(cm_util::format("NF:%s", name.c_str()));
        }
        return do_result(event);
    }

    bool do_remove(const std::string &name, cm_cache::cache_event &event) {
    
        cm_log::info(cm_util::format("-%s", name.c_str()));

        // journal first to guard rotation
        journal.info(event.request);

        int num = cm_store::mem_store.remove(name);
        event.result.assign(cm_util::format("(%d):%s", num, name.c_str()));
        return do_result(event);
    }

    bool do_watch(const std::string &name, const std::string &tag, cm_cache::cache_event &event) {

        cm_log::info(cm_util::format("*%s #%s", name.c_str(), tag.c_str()));

        journal.lock();     // guard rotation
        event.value = cm_store::mem_store.find(name);
        journal.unlock();

        event.name = name;
        event.tag = tag;

        // create publishers inline from watcher requests
        if(event.pub_name.size() > 0) {
            publishers.add(name, event.pub_name);
            // publishers removed when watchers disconnect
        }

        watcher w(event.fd, event.tag, event.pub_name, false /*remove*/);
        if(watchers.check(name, w) == false) {
            watchers.add(name, w);
        }
        else {
            CM_LOG_TRACE { cm_log::trace(cm_util::format("watch already active: *%s #%s",
             event.name.c_str(), event.tag.c_str())); }
        }

        event.result.assign(cm_util::format("%s:%s", tag.c_str(), event.value.c_str()));
        return do_result(event);
    }

    bool do_watch_remove(const std::string &name, const std::string &tag, cm_cache::cache_event &event) {

        //cm_log::info(cm_util::format("@%s #%s", name.c_str(), tag.c_str()));

        journal.lock();     // guard rotationn
        event.value = cm_store::mem_store.find(name);
        journal.unlock();

        event.name = name;
        event.tag = tag;

        // create publishers inline from watcher requests
        if(event.pub_name.size() > 0) {
            publishers.add(name, event.pub_name);
            // publishers removed when watchers disconnect
        }        

        watcher w(event.fd, event.tag, event.pub_name, true /*remove*/);
        if(watchers.check(name, w) == false) {
            watchers.add(name, w);
        }
        else {
            CM_LOG_TRACE { cm_log::trace(cm_util::format("watch already active: @%s #%s",
             event.name.c_str(), event.tag.c_str())); }
        }

        event.result.assign(cm_util::format("%s:%s", tag.c_str(), event.value.c_str()));
        return do_result(event);
    }

    bool do_result(cm_cache::cache_event &event) {

        cm_net::send(event.fd, event.result.append("\n"));

        CM_LOG_TRACE {
            cm_log::trace(cm_util::format("%d: sent response:", event.fd));
            cm_log::hex_dump(cm_log::level::trace, event.result.c_str(), event.result.size(), 16);
        }

        if(event.notify) {

            if(watchers.notify(event.name, event.value, event)) {
                cm_store::mem_store.remove(event.name);
                CM_LOG_TRACE { cm_log::trace(cm_util::format("removed on notify: %s", event.name.c_str())); }
            }
        }

        return true;
    }

    bool do_input(const std::string &in_str, cm_cache::cache_event &event) { 
        return true;
    }

    bool do_error(const std::string &expr, const std::string &err, cm_cache::cache_event &event) {
        event.result.assign(cm_util::format("error: %s: %s", err.c_str(), expr.c_str()));
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
        // send $:VORTEX
        std::string hello("$:VORTEX\n");
        cm_net::send(socket, hello);
        CM_LOG_TRACE {
            cm_log::info(cm_util::format("%d: sent hello:", socket));
            cm_log::hex_dump(cm_log::level::info, hello.c_str(), hello.size(), 16);
        }
        return;        
    }

    // if this in an EOF event (client disconnected)
    if(event->eof) {
        // remove socket from all watchers
        int num = watchers.remove(socket);
        if(num > 0) {
            CM_LOG_TRACE { cm_log::info(cm_util::format("%d: socket removed from %d watcher(s)", socket, num)); }
        }
        return;
    }

    CM_LOG_TRACE {
        cm_log::trace(cm_util::format("%d: received request:", socket));
        cm_log::hex_dump(cm_log::level::trace, request.c_str(), request.size(), 16);
    }

    cm_cache::cache cache(&processor);
    cm_cache::cache_event req_event;

    std::stringstream ss(request);
    std::string item;

    while (std::getline (ss, item, '\n')) {
        item.append("\n");
        req_event.clear();
        req_event.fd = socket;
        req_event.request.assign(item);
        cache.eval(item, req_event);
    }

    // process any publish requests generated by this incoming request...

    while(pub_queue.size() > 0) {
        item = pub_queue.pop_front();
        item.append("\n");
        req_event.request.assign(item);
        cache.eval(item, req_event);   
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
