/*
 * test_metrics_server.cpp
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

#include "test_base_class.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "metrics_server.h"

using namespace std;

class MetricsServerTest : public TestBaseClass {
   protected:
    static bool render_ok(string* out) {
        *out = "metric_a\t1\n";
        return true;
    }
    static bool render_fail(string*) { return false; }

    static bool has_header(const string& response, const string& header) { return response.find("\r\n" + header + "\r\n") != string::npos; }

    static string body_of(const string& response) {
        size_t end = response.find("\r\n\r\n");
        return end == string::npos ? "" : response.substr(end + 4);
    }

    // Sends request to 127.0.0.1:port and returns everything the server sends back before closing
    static string http_exchange(int port, const string& request) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        EXPECT_GE(fd, 0);
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        EXPECT_EQ(connect(fd, (sockaddr*)&addr, sizeof(addr)), 0);
        EXPECT_EQ(send(fd, request.data(), request.size(), 0), (ssize_t)request.size());
        string response;
        char buf[4096];
        ssize_t n;
        while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
            response.append(buf, n);
        }
        close(fd);
        return response;
    }
};

// parse_listen_address

TEST_F(MetricsServerTest, listen_address_ipv4) {
    string host, port, err;
    ASSERT_TRUE(parse_listen_address("0.0.0.0:9101", &host, &port, &err)) << err;
    EXPECT_EQ(host, "0.0.0.0");
    EXPECT_EQ(port, "9101");
}

TEST_F(MetricsServerTest, listen_address_hostname) {
    string host, port, err;
    ASSERT_TRUE(parse_listen_address("localhost:1", &host, &port, &err)) << err;
    EXPECT_EQ(host, "localhost");
    EXPECT_EQ(port, "1");
}

TEST_F(MetricsServerTest, listen_address_bracketed_ipv6) {
    string host, port, err;
    ASSERT_TRUE(parse_listen_address("[::]:9101", &host, &port, &err)) << err;
    EXPECT_EQ(host, "::");
    EXPECT_EQ(port, "9101");
    ASSERT_TRUE(parse_listen_address("[::1]:65535", &host, &port, &err)) << err;
    EXPECT_EQ(host, "::1");
    EXPECT_EQ(port, "65535");
}

TEST_F(MetricsServerTest, listen_address_empty_host_means_all_interfaces) {
    string host = "unchanged", port, err;
    ASSERT_TRUE(parse_listen_address(":9101", &host, &port, &err)) << err;
    EXPECT_EQ(host, "");
    EXPECT_EQ(port, "9101");
}

TEST_F(MetricsServerTest, listen_address_rejects_invalid) {
    const char* invalid[] = {
        "",            // nothing
        "9101",        // no host separator
        "host:",       // no port
        "host:0",      // port out of range
        "host:65536",  // port out of range
        "host:abc",    // not a number
        "host:91x",    // trailing garbage
        "host:-1",     // sign
        "::1:9101",    // unbracketed IPv6 is ambiguous
        "[::1:9101",   // unterminated bracket
        "[::1]9101",   // missing colon after bracket
        "[]:9101",     // empty bracketed host
    };
    for (const char* spec : invalid) {
        string host, port, err;
        EXPECT_FALSE(parse_listen_address(spec, &host, &port, &err)) << "accepted \"" << spec << "\"";
        EXPECT_FALSE(err.empty()) << "no error message for \"" << spec << "\"";
    }
}

// handle_metrics_request

TEST_F(MetricsServerTest, get_metrics_returns_body) {
    string r = handle_metrics_request("GET /metrics HTTP/1.1\r\nHost: x\r\n\r\n", render_ok);
    EXPECT_EQ(r.rfind("HTTP/1.1 200 OK\r\n", 0), 0u) << r;
    EXPECT_TRUE(has_header(r, "Content-Type: text/plain; version=0.0.4; charset=utf-8")) << r;
    EXPECT_TRUE(has_header(r, "Content-Length: 11")) << r;
    EXPECT_TRUE(has_header(r, "Connection: close")) << r;
    EXPECT_EQ(body_of(r), "metric_a\t1\n");
}

TEST_F(MetricsServerTest, query_string_is_ignored) {
    string r = handle_metrics_request("GET /metrics?x=1 HTTP/1.1\r\n\r\n", render_ok);
    EXPECT_EQ(r.rfind("HTTP/1.1 200 OK\r\n", 0), 0u) << r;
}

TEST_F(MetricsServerTest, http_1_0_is_accepted) {
    string r = handle_metrics_request("GET /metrics HTTP/1.0\r\n\r\n", render_ok);
    EXPECT_EQ(r.rfind("HTTP/1.1 200 OK\r\n", 0), 0u) << r;
}

TEST_F(MetricsServerTest, head_has_length_but_no_body) {
    string r = handle_metrics_request("HEAD /metrics HTTP/1.1\r\n\r\n", render_ok);
    EXPECT_EQ(r.rfind("HTTP/1.1 200 OK\r\n", 0), 0u) << r;
    EXPECT_TRUE(has_header(r, "Content-Length: 11")) << r;
    EXPECT_EQ(body_of(r), "");
}

TEST_F(MetricsServerTest, unknown_path_is_404) {
    bool rendered = false;
    string r = handle_metrics_request("GET /other HTTP/1.1\r\n\r\n", [&](string* out) {
        rendered = true;
        return render_ok(out);
    });
    EXPECT_EQ(r.rfind("HTTP/1.1 404 Not Found\r\n", 0), 0u) << r;
    EXPECT_FALSE(rendered);
}

TEST_F(MetricsServerTest, other_method_is_405_with_allow) {
    string r = handle_metrics_request("POST /metrics HTTP/1.1\r\n\r\n", render_ok);
    EXPECT_EQ(r.rfind("HTTP/1.1 405 Method Not Allowed\r\n", 0), 0u) << r;
    EXPECT_TRUE(has_header(r, "Allow: GET, HEAD")) << r;
}

TEST_F(MetricsServerTest, malformed_request_is_400) {
    const char* malformed[] = {
        "\r\n\r\n", "GET\r\n\r\n", "GET /metrics\r\n\r\n", "GET /metrics FTP/1.0\r\n\r\n", "GET  /metrics HTTP/1.1\r\n\r\n", "GET /metrics HTTP/1.1 extra\r\n\r\n",
    };
    for (const char* request : malformed) {
        string r = handle_metrics_request(request, render_ok);
        EXPECT_EQ(r.rfind("HTTP/1.1 400 Bad Request\r\n", 0), 0u) << "for request: " << request << "\n" << r;
    }
}

TEST_F(MetricsServerTest, render_failure_is_503) {
    string r = handle_metrics_request("GET /metrics HTTP/1.1\r\n\r\n", render_fail);
    EXPECT_EQ(r.rfind("HTTP/1.1 503 Service Unavailable\r\n", 0), 0u) << r;
}

// MetricsExchange

TEST_F(MetricsServerTest, exchange_not_pending_initially) {
    MetricsExchange ex;
    EXPECT_FALSE(ex.pending());
}

TEST_F(MetricsServerTest, exchange_times_out_without_publisher) {
    MetricsExchange ex;
    string out;
    EXPECT_FALSE(ex.request(&out, chrono::milliseconds(20)));
}

TEST_F(MetricsServerTest, exchange_delivers_published_text) {
    MetricsExchange ex;
    atomic<bool> stop(false);
    thread publisher([&] {
        while (!stop) {
            if (ex.pending()) {
                uint64_t generation = ex.begin_render();
                ex.publish(generation, "fresh\n");
            }
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    });
    string out;
    EXPECT_TRUE(ex.request(&out, chrono::seconds(5)));
    EXPECT_EQ(out, "fresh\n");
    EXPECT_FALSE(ex.pending());
    stop = true;
    publisher.join();
}

TEST_F(MetricsServerTest, exchange_render_started_before_request_does_not_satisfy_it) {
    MetricsExchange ex;
    uint64_t stale_generation = ex.begin_render();
    thread requester([&] {
        string out;
        EXPECT_TRUE(ex.request(&out, chrono::seconds(5)));
        EXPECT_EQ(out, "new\n");
    });
    while (!ex.pending()) {
        this_thread::yield();
    }
    ex.publish(stale_generation, "old\n");
    EXPECT_TRUE(ex.pending());
    ex.publish(ex.begin_render(), "new\n");
    requester.join();
}

// MetricsServer

TEST_F(MetricsServerTest, server_serves_over_tcp) {
    MetricsServer server;
    string err;
    ASSERT_TRUE(server.start("127.0.0.1", "0", [](const string& head) { return handle_metrics_request(head, render_ok); }, &err)) << err;
    ASSERT_GT(server.port(), 0);

    string r = http_exchange(server.port(), "GET /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_EQ(r.rfind("HTTP/1.1 200 OK\r\n", 0), 0u) << r;
    EXPECT_EQ(body_of(r), "metric_a\t1\n");

    // a second request on a new connection works too
    r = http_exchange(server.port(), "GET /nope HTTP/1.1\r\n\r\n");
    EXPECT_EQ(r.rfind("HTTP/1.1 404 Not Found\r\n", 0), 0u) << r;

    server.stop();
}

TEST_F(MetricsServerTest, server_rejects_oversized_request_head) {
    MetricsServer server;
    string err;
    ASSERT_TRUE(server.start("127.0.0.1", "0", [](const string& head) { return handle_metrics_request(head, render_ok); }, &err)) << err;

    string request = "GET /metrics HTTP/1.1\r\nX-Big: " + string(64 * 1024, 'a') + "\r\n\r\n";
    string r = http_exchange(server.port(), request);
    EXPECT_EQ(r.rfind("HTTP/1.1 431 Request Header Fields Too Large\r\n", 0), 0u) << r.substr(0, 200);

    server.stop();
}

TEST_F(MetricsServerTest, server_start_fails_when_port_in_use) {
    MetricsServer first, second;
    string err;
    auto handler = [](const string& head) { return handle_metrics_request(head, render_ok); };
    ASSERT_TRUE(first.start("127.0.0.1", "0", handler, &err)) << err;
    EXPECT_FALSE(second.start("127.0.0.1", to_string(first.port()), handler, &err));
    EXPECT_FALSE(err.empty());
    first.stop();
}

TEST_F(MetricsServerTest, server_stop_is_idempotent_and_safe_without_start) {
    MetricsServer never_started;
    never_started.stop();

    MetricsServer server;
    string err;
    ASSERT_TRUE(server.start("127.0.0.1", "0", [](const string& head) { return handle_metrics_request(head, render_ok); }, &err)) << err;
    server.stop();
    server.stop();
}
