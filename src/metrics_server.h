/*
 * metrics_server.h
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

#ifndef _METRICS_SERVER_H
#define _METRICS_SERVER_H

#include <stdint.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

// Splits "host:port", "[ipv6]:port" or ":port" (all interfaces, host set to "").
// The port must be 1-65535.  On failure returns false and sets *error.
bool parse_listen_address(const std::string& spec, std::string* host, std::string* port, std::string* error);

// Builds the complete HTTP response for one request.  request_head is everything up to and
// including the blank line that ends the headers.  render is called only for GET/HEAD /metrics
// and returns false when no metrics could be produced (answered with 503).
std::string handle_metrics_request(const std::string& request_head, const std::function<bool(std::string*)>& render);

// Hands a metrics render request from the HTTP thread to the thread that owns the data
// (the first output thread), which polls pending() once per loop.  Only that thread reads
// the per-channel state, as it already does for the stats file, so serving the endpoint
// adds no new reader of the demod thread's data.
class MetricsExchange {
   public:
    MetricsExchange() : requested_(0), served_(0) {}

    // HTTP side: blocks until a render that started after this call is published
    bool request(std::string* text, std::chrono::milliseconds timeout);

    // Render side, lock free so it can be checked every output loop
    bool pending(void) const { return requested_.load(std::memory_order_acquire) != served_.load(std::memory_order_acquire); }
    // Call before rendering and pass the result to publish(): it marks which requests the render can satisfy
    uint64_t begin_render(void) const { return requested_.load(std::memory_order_acquire); }
    void publish(uint64_t generation, std::string text);

   private:
    std::mutex mutex_;
    std::condition_variable published_;
    std::atomic<uint64_t> requested_;
    std::atomic<uint64_t> served_;
    std::string text_;
};

// Minimal HTTP/1.1 server: one connection at a time, one request per connection.
class MetricsServer {
   public:
    typedef std::function<std::string(const std::string& request_head)> Handler;

    MetricsServer() : listen_fd_(-1), port_(0), stop_(false) {}
    ~MetricsServer() { stop(); }

    // Binds and starts the serving thread.  An empty host listens on all interfaces
    // (IPv6 dual-stack where available).  Port "0" picks a free port, see port().
    bool start(const std::string& host, const std::string& port, Handler handler, std::string* error);
    void stop(void);
    int port(void) const { return port_; }

   private:
    void run(void);
    void serve_connection(int fd);

    int listen_fd_;
    int port_;
    std::atomic<bool> stop_;
    Handler handler_;
    std::thread thread_;
};

#endif /* _METRICS_SERVER_H */
