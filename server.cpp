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

//////////////////////////////////// client //////////////////////////////////


cm_net::client_thread *client = nullptr;
bool connected = false;
int host_port = -1;
int echo_fd = -1;

void server_echo(int fd, const char *buf, size_t sz);

void _sleep(int interval /* ms */) {

    // allow other threads to do some work
    long n = 1000000;
    time_t s = 0;
    if(interval > 0) {
        n = ((long) interval % 1000L) * 1000000L;
        s = (time_t) interval / 1000;
    }
    timespec delay = {s, n};
    nanosleep(&delay, NULL);    // interruptable
}

// network client received data from remote vortex server
void client_receive(int socket, const char *buf, size_t sz) {

    std::string request(buf, sz);

    if(request == "$:VORTEX\n") {
        server_echo(socket, "$:VORTEX_CLIENT\n", 16);
    }

    CM_LOG_TRACE {
        cm_log::trace(cm_util::format("%d: received response:", socket));
        cm_log::hex_dump(cm_log::level::trace, buf, sz, 16);
    }

    // cm_net::input_event *event = new cm_net::input_event(socket,
    //     request);

    // if(nullptr != event) {
    //     // add data to thread pool which will call this vortex
    //     // server's request handler
    //     thread_pool_ptr->add_task(request_handler, event, request_dealloc);
    // }
    // else {
    //     cm_log::critical("client_receive: pool_server: error: event allocation failed!");
    // }
}

// echo request to remote vortex server
void server_echo(int fd, const char *buf, size_t sz) {

    // send data to remote vortex server
    int written = cm_net::write(fd, buf, sz);
    if(written < 0) {
        cm_net::err("server_receive: net_write", errno);
    }
    else {
        CM_LOG_TRACE {
            cm_log::trace(cm_util::format("%d: echo request:", fd));
            cm_log::hex_dump(cm_log::level::trace, buf, written, 16);
        }
    }
}

//////////////////////////////////// server //////////////////////////////////

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


bool loop_analysis(std::vector<std::pair<std::string, std::string>> &input) {

    // flatten key/value pairs into route strings
    // examples:
    // 12/03/2019 18:21:42 [info]: etok xray pub etok
    // 12/03/2019 18:21:42 [info]: etok red
    // 12/03/2019 18:21:42 [info]: etok pub etok
    // 12/03/2019 18:21:42 [info]: xray pub etok
    // 12/03/2019 18:21:42 [info]: pub etok xray pub etok    

    std::vector<std::string> route[input.size()];
    int index = 0;

    // for each input 
    for(auto it = input.begin(); it != input.end(); ++it) {

        std::string in_key = it->first;
        std::string in_pub = it->second;

        route[index].push_back(in_key);
        route[index].push_back(in_pub);

        // look for key that matches in_pub
        for(auto pt = input.begin(); pt != input.end(); ++pt) {
            std::string key = pt->first;
            std::string pub = pt->second;
            if(key == in_pub) {
                route[index].push_back(pub);
                // new search term forward
                in_pub = pub;
            }
         }
         index++;
    }

    // count the number of times each key occurs in the route
    // more than once means it is a loop in the route

    bool found_loop = false;

    for(int k = 0; k < input.size(); k++) {
        std::string s;
        std::map<std::string,int> counts;

        // build output log string and key count
        for(auto it = route[k].begin(); it != route[k].end(); ++it) {
            s += (*it) + " ";
            counts[*it]++;
        }

        // sum the number of keys that occur more than once;
        // that is the total number of loops in the route
        int loop_sum = 0;
        for(auto &it : counts) {
            int count = it.second;
            if(count > 1) {
                loop_sum += (count - 1);
                found_loop = true;
            }
        }

        std::string loop_report = loop_sum > 0 ?
         cm_util::format("%d loop(s)", loop_sum) : "OK";

        std::string msg = cm_util::format("%s: %s", s.c_str(), 
            loop_report.c_str());

        if(loop_sum > 0) cm_log::error(msg.c_str());
        else cm_log::info(msg.c_str());
    }

    return found_loop == false;
}

// when notify has a watcher with a re-pub key, it will put the publish
// request in this queue
cm_queue::double_queue<std::string> pub_queue;

class watcher_store: protected cm::mutex {

protected:
    // unordered map for faster access vs. map using buckets
    std::unordered_map<std::string,std::vector<watcher>> _map;

public:


    void get_publishers(std::vector<std::pair<std::string, std::string>> &outv) {

        for(auto it = _map.begin(); it != _map.end(); ++it) {
            std::string name = it->first;
            std::vector<watcher> &v = it->second;

            for(auto wit = v.begin(); wit != v.end(); ++wit) {
                if(wit->pub.size() > 0) {
                    auto p = std::make_pair(name, wit->pub.substr(1));
                    outv.push_back(p);
                }
            }
        }
    }
    
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

            // notify watchers
            std::vector<watcher> &v = _map[name];
            for(auto &_watcher: v) {

                CM_LOG_TRACE {
                    cm_log::info(cm_util::format("%d: notify: %s #%s %s", _watcher.fd, name.c_str(), _watcher.tag.c_str(), _watcher.pub.c_str()));
                    cm_log::hex_dump(cm_log::level::info, value.c_str(), value.size(), 16);
                }

                cm_net::send(_watcher.fd,
                cm_util::format("%s:%s\n", _watcher.tag.c_str(), value.c_str()));

                if(_watcher.pub.size() > 0) {
                    // publish data to specified key
                    std::string request = _watcher.pub;   //+key
                    request.append(" ");
                    request.append(value); 
                    // add request to pub queue
                    pub_queue.push_back( request );
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

        if(echo_fd != -1) {
            server_echo(echo_fd, event.request.c_str(), event.request.size());
        }

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

            if(echo_fd != -1) {
                server_echo(echo_fd, event.request.c_str(), event.request.size());
            }

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

        if(echo_fd != -1) {
            server_echo(echo_fd, event.request.c_str(), event.request.size());
        }

        int num = cm_store::mem_store.remove(name);
        event.result.assign(cm_util::format("(%d):%s", num, name.c_str()));
        return do_result(event);
    }

    bool do_watch(const std::string &name, const std::string &tag, cm_cache::cache_event &event) {

        cm_log::info(cm_util::format("*%s #%s %s", name.c_str(), tag.c_str(), event.pub_name.c_str()));

        journal.lock();     // guard rotation
        event.value = cm_store::mem_store.find(name);
        journal.unlock();

        event.name = name;
        event.tag = tag;

        // if pub name, do loop_analysis before allowing in new watcher
        std::string pub_name = event.pub_name;
        if(pub_name.size() > 0) {
            std::vector<std::pair<std::string, std::string>> input;

            // get current publishers
            watchers.get_publishers(input);

            // add candidate pair
            auto p = std::make_pair(name, pub_name.substr(1));
            input.push_back(p);

            if(loop_analysis(input) == false) {
                pub_name = "";
                cm_log::warning(cm_util::format("[%s] did not pass loop analysis, ignoring", event.pub_name.c_str()));
            }
        }

        watcher w(event.fd, event.tag, pub_name, false /*remove*/);
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

        cm_log::info(cm_util::format("@%s #%s %s", name.c_str(), tag.c_str(), event.pub_name.c_str()));

        journal.lock();     // guard rotationn
        event.value = cm_store::mem_store.find(name);
        journal.unlock();

        event.name = name;
        event.tag = tag;

        // if pub name, do loop_analysis before allowing in new watcher
        std::string pub_name = event.pub_name;
        if(pub_name.size() > 0) {
            std::vector<std::pair<std::string, std::string>> input;

            // get current publishers
            watchers.get_publishers(input);

            // add candidate pair
            auto p = std::make_pair(name, pub_name.substr(1));
            input.push_back(p);

            if(loop_analysis(input) == false) {
                pub_name = "";
                cm_log::warning(cm_util::format("[%s] did not pass loop analysis, ignoring", event.pub_name.c_str()));
            }
        }

        watcher w(event.fd, event.tag, pub_name, true /*remove*/);
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

            // notify watchers
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

        if(echo_fd == socket) {
            echo_fd = -1;
            cm_log::warning(cm_util::format("%d: vortex to vortex discontinued", socket));
        }

        return;
    }

    CM_LOG_TRACE {
        cm_log::trace(cm_util::format("%d: received request:", socket));
        cm_log::hex_dump(cm_log::level::trace, request.c_str(), request.size(), 16);
    }

    if(echo_fd == -1 && request == "$:VORTEX_CLIENT\n") {
        echo_fd = socket;
        cm_log::info(cm_util::format("%d: vortex to vortex established", socket));
        return;
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

    // process any publish requests generated in notify
    // by this incoming request...

    while(pub_queue.size() > 0) {
         item = pub_queue.pop_front();
         req_event.clear();
         req_event.fd = socket;
         req_event.request.assign(item);
         cache.eval(item, req_event);

         if(pub_queue.size() > 0) {
             timespec delay = {0, 10000000};   // 10 ms
             nanosleep(&delay, NULL);  
         } 
    }
}

void request_dealloc(void *arg) {
    delete (cm_net::input_event *) arg;
}

void vortex::run(int port, const std::string &host_name, int _host_port) {

    if(_host_port != -1) {
    host_port = _host_port;
        cm_log::info(cm_util::format("remote vortex server: %s:%d", host_name.c_str(), host_port));
    }

    connected = false;
    time_t next_connect_time = 0;

    // create thread pool that will do work for the server
    cm_thread::pool thread_pool(6);

    // startup tcp server
    cm_net::pool_server server(port, &thread_pool, request_handler,
        request_dealloc);

    while( !server.is_done() ) {
    //while(1) {
        // timespec delay = {0, 100000000};   // 100 ms
        // nanosleep(&delay, NULL);

        _sleep(1000);

        if(host_port != -1) {

            if(nullptr == client) {
                client = new cm_net::client_thread(host_name, host_port, client_receive);
                next_connect_time = cm_time::clock_seconds() + 60;
            }

            // if client thread is running, we are connected
            if(nullptr != client && client->is_connected()) {
                if(!connected) {
                    connected = true;
                }
            }

            // if client thread is NOT running, we are NOT connected
            if(nullptr != client && !client->is_connected()) {
                if(connected) {
                    connected = false;
                }

                if(cm_time::clock_seconds() > next_connect_time) {
                    // attempt reconnect
                    client->start();
                    next_connect_time = cm_time::clock_seconds() + 60;
                }
            }
        }
    }

    // wait for pool_server threads to complete all work tasks
    thread_pool.wait_all();
}
