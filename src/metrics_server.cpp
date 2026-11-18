/*
 * metrics_server.cpp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "metrics_server.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

using namespace std;

static const size_t max_request_head = 8192;
static const int request_timeout_ms = 5000;
static const int linger_timeout_ms = 1000;
static const int accept_poll_ms = 100;

#ifdef MSG_NOSIGNAL
static const int send_flags = MSG_NOSIGNAL;
#else
static const int send_flags = 0;
#endif

bool parse_listen_address(const string& spec, string* host, string* port, string* error) {
    string h, p;
    if (!spec.empty() && spec[0] == '[') {
        size_t close = spec.find(']');
        if (close == string::npos) {
            *error = "missing ']' after IPv6 address";
            return false;
        }
        if (close == 1) {
            *error = "empty IPv6 address";
            return false;
        }
        if (close + 1 >= spec.size() || spec[close + 1] != ':') {
            *error = "expected ':' and a port after ']'";
            return false;
        }
        h = spec.substr(1, close - 1);
        p = spec.substr(close + 2);
    } else {
        size_t colon = spec.rfind(':');
        if (colon == string::npos) {
            *error = "expected host:port";
            return false;
        }
        h = spec.substr(0, colon);
        if (h.find(':') != string::npos) {
            *error = "IPv6 addresses must be in brackets, e.g. [::1]:9101";
            return false;
        }
        p = spec.substr(colon + 1);
    }

    if (p.empty() || p.size() > 5 || p.find_first_not_of("0123456789") != string::npos) {
        *error = "port must be a number from 1 to 65535";
        return false;
    }
    long n = strtol(p.c_str(), NULL, 10);
    if (n < 1 || n > 65535) {
        *error = "port must be a number from 1 to 65535";
        return false;
    }

    *host = h;
    *port = p;
    return true;
}

static string build_response(int status, const char* reason, const string& extra_headers, const string& content_type, const string& body, bool include_body) {
    string r = "HTTP/1.1 " + to_string(status) + " " + reason + "\r\n";
    r += "Content-Type: " + content_type + "\r\n";
    r += "Content-Length: " + to_string(body.size()) + "\r\n";
    r += extra_headers;
    r += "Connection: close\r\n\r\n";
    if (include_body) {
        r += body;
    }
    return r;
}

static string text_response(int status, const char* reason, const string& extra_headers = "") {
    return build_response(status, reason, extra_headers, "text/plain; charset=utf-8", string(reason) + "\n", true);
}

string handle_metrics_request(const string& request_head, const function<bool(string*)>& render) {
    size_t eol = request_head.find('\n');
    string line = request_head.substr(0, eol);
    if (!line.empty() && line[line.size() - 1] == '\r') {
        line.erase(line.size() - 1);
    }

    // method SP request-target SP HTTP-version, with exactly one space between them
    size_t sp1 = line.find(' ');
    size_t sp2 = sp1 == string::npos ? string::npos : line.find(' ', sp1 + 1);
    if (sp1 == string::npos || sp2 == string::npos || line.find(' ', sp2 + 1) != string::npos || sp1 == 0 || sp2 == sp1 + 1) {
        return text_response(400, "Bad Request");
    }
    string method = line.substr(0, sp1);
    string target = line.substr(sp1 + 1, sp2 - sp1 - 1);
    string version = line.substr(sp2 + 1);
    if (version != "HTTP/1.0" && version != "HTTP/1.1") {
        return text_response(400, "Bad Request");
    }

    string path = target.substr(0, target.find('?'));
    if (path != "/metrics") {
        return text_response(404, "Not Found");
    }
    if (method != "GET" && method != "HEAD") {
        return text_response(405, "Method Not Allowed", "Allow: GET, HEAD\r\n");
    }

    string body;
    if (!render(&body)) {
        return text_response(503, "Service Unavailable");
    }
    return build_response(200, "OK", "", "text/plain; version=0.0.4; charset=utf-8", body, method == "GET");
}

bool MetricsExchange::request(string* text, chrono::milliseconds timeout) {
    unique_lock<mutex> lock(mutex_);
    const uint64_t mine = requested_.load(memory_order_relaxed) + 1;
    requested_.store(mine, memory_order_release);
    if (!published_.wait_for(lock, timeout, [&] { return served_.load(memory_order_relaxed) >= mine; })) {
        return false;
    }
    *text = text_;
    return true;
}

void MetricsExchange::publish(uint64_t generation, string text) {
    {
        lock_guard<mutex> lock(mutex_);
        if (generation <= served_.load(memory_order_relaxed)) {
            return;
        }
        text_ = std::move(text);
        served_.store(generation, memory_order_release);
    }
    published_.notify_all();
}

static int remaining_ms(chrono::steady_clock::time_point deadline) {
    auto left = chrono::duration_cast<chrono::milliseconds>(deadline - chrono::steady_clock::now()).count();
    return left > 0 ? (int)left : 0;
}

static bool send_all(int fd, const string& data, chrono::steady_clock::time_point deadline) {
    size_t sent = 0;
    while (sent < data.size()) {
        pollfd p = {fd, POLLOUT, 0};
        int ready = poll(&p, 1, remaining_ms(deadline));
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready <= 0) {
            return false;
        }
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, send_flags);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        }
        if (n <= 0) {
            return false;
        }
        sent += n;
    }
    return true;
}

// Closing a socket that still has unread request data makes the kernel send a reset,
// which can make the client discard the response it has not read yet.  So stop sending,
// then read and discard until the client closes or a short timeout passes.
static void lingering_close(int fd) {
    shutdown(fd, SHUT_WR);
    auto deadline = chrono::steady_clock::now() + chrono::milliseconds(linger_timeout_ms);
    char buf[4096];
    while (true) {
        pollfd p = {fd, POLLIN, 0};
        int ready = poll(&p, 1, remaining_ms(deadline));
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready <= 0 || recv(fd, buf, sizeof(buf), 0) <= 0) {
            break;
        }
    }
    close(fd);
}

void MetricsServer::serve_connection(int fd) {
    auto deadline = chrono::steady_clock::now() + chrono::milliseconds(request_timeout_ms);
    string head;
    char buf[1024];
    bool complete = false;
    while (!complete && head.size() <= max_request_head) {
        pollfd p = {fd, POLLIN, 0};
        int ready = poll(&p, 1, remaining_ms(deadline));
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready <= 0) {
            break;
        }
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        }
        if (n <= 0) {
            break;
        }
        // rescan only the new bytes, plus 3 for a terminator split across reads
        size_t from = head.size() > 3 ? head.size() - 3 : 0;
        head.append(buf, n);
        size_t end = head.find("\r\n\r\n", from);
        if (end == string::npos) {
            end = head.find("\n\n", from);
        }
        complete = end != string::npos;
    }

    if (complete && head.size() <= max_request_head) {
        send_all(fd, handler_(head), deadline);
    } else if (head.size() > max_request_head) {
        send_all(fd, text_response(431, "Request Header Fields Too Large"), deadline);
    }
    lingering_close(fd);
}

void MetricsServer::run(void) {
    while (!stop_.load()) {
        pollfd p = {listen_fd_, POLLIN, 0};
        if (poll(&p, 1, accept_poll_ms) <= 0) {
            continue;
        }
        int fd = accept(listen_fd_, NULL, NULL);
        if (fd < 0) {
            continue;
        }
        fcntl(fd, F_SETFD, FD_CLOEXEC);
#ifdef SO_NOSIGPIPE
        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
        serve_connection(fd);
    }
}

static int open_listener(const addrinfo* ai, bool dual_stack, string* error) {
    int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0) {
        *error = strerror(errno);
        return -1;
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (dual_stack && ai->ai_family == AF_INET6) {
        int zero = 0;
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof(zero));
    }
    if (bind(fd, ai->ai_addr, ai->ai_addrlen) != 0 || listen(fd, 16) != 0) {
        *error = strerror(errno);
        close(fd);
        return -1;
    }
    return fd;
}

bool MetricsServer::start(const string& host, const string& port, Handler handler, string* error) {
    if (listen_fd_ >= 0) {
        *error = "already started";
        return false;
    }

    const bool all_interfaces = host.empty();
    // all interfaces: prefer a dual-stack IPv6 socket, fall back to IPv4 where IPv6 is unavailable
    vector<int> families;
    if (all_interfaces) {
        families.push_back(AF_INET6);
        families.push_back(AF_INET);
    } else {
        families.push_back(AF_UNSPEC);
    }

    string last_error = "no usable address";
    for (int family : families) {
        addrinfo hints = {};
        hints.ai_family = family;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        addrinfo* result = NULL;
        int rc = getaddrinfo(all_interfaces ? NULL : host.c_str(), port.c_str(), &hints, &result);
        if (rc != 0) {
            last_error = gai_strerror(rc);
            continue;
        }
        for (addrinfo* ai = result; ai != NULL && listen_fd_ < 0; ai = ai->ai_next) {
            listen_fd_ = open_listener(ai, all_interfaces, &last_error);
        }
        freeaddrinfo(result);
        if (listen_fd_ >= 0) {
            break;
        }
    }
    if (listen_fd_ < 0) {
        *error = last_error;
        return false;
    }

    sockaddr_storage bound = {};
    socklen_t len = sizeof(bound);
    if (getsockname(listen_fd_, (sockaddr*)&bound, &len) == 0) {
        port_ = ntohs(bound.ss_family == AF_INET6 ? ((sockaddr_in6*)&bound)->sin6_port : ((sockaddr_in*)&bound)->sin_port);
    }

    handler_ = std::move(handler);
    stop_.store(false);
    thread_ = thread(&MetricsServer::run, this);
    return true;
}

void MetricsServer::stop(void) {
    stop_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
}
