// Simple concurrent load tester for the URL shortener.
// Usage:  bench.exe [path] [threads] [requestsPerThread]
//   e.g.  bench.exe /1 8 5000    -> 8 threads x 5000 GETs against http://localhost:8080/1
#include "lib/httplib.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <string>

int main(int argc, char** argv) {
    const std::string host = "127.0.0.1";   // NOT "localhost" — avoids ~200ms IPv6 fallback on Windows
    const int         port = 8080;
    const std::string path      = argc > 1 ? argv[1] : "/1";
    const int         threads   = argc > 2 ? std::stoi(argv[2]) : 8;
    const int         perThread = argc > 3 ? std::stoi(argv[3]) : 5000;

    std::vector<std::vector<double>> lat(threads);
    std::atomic<long> ok{0}, fail{0};

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    for (int i = 0; i < threads; ++i) {
        pool.emplace_back([&, i] {
            httplib::Client cli(host, port);
            cli.set_keep_alive(true);            // reuse the TCP connection
            cli.set_tcp_nodelay(true);           // disable Nagle (avoid ~40ms/req delay)
            cli.set_follow_location(false);      // measure the 302 itself, don't chase it
            lat[i].reserve(perThread);
            for (int j = 0; j < perThread; ++j) {
                auto s = std::chrono::steady_clock::now();
                auto res = cli.Get(path.c_str());
                auto e = std::chrono::steady_clock::now();
                lat[i].push_back(std::chrono::duration<double, std::milli>(e - s).count());
                if (res && (res->status == 302 || res->status == 200)) ok++; else fail++;
            }
        });
    }
    for (auto& t : pool) t.join();
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::vector<double> all;
    for (auto& v : lat) all.insert(all.end(), v.begin(), v.end());
    std::sort(all.begin(), all.end());
    auto pct = [&](double p) { return all.empty() ? 0.0 : all[(size_t)(p * (all.size() - 1))]; };

    long total = ok + fail;
    std::cout << "path:        " << path << "  (" << threads << " threads x " << perThread << ")\n";
    std::cout << "requests:    " << total << "  (ok=" << ok << ", fail=" << fail << ")\n";
    std::cout << "time:        " << secs << " s\n";
    std::cout << "throughput:  " << (long)(total / secs) << " req/sec\n";
    std::cout << "latency p50: " << pct(0.50) << " ms\n";
    std::cout << "latency p90: " << pct(0.90) << " ms\n";
    std::cout << "latency p99: " << pct(0.99) << " ms\n";
    return 0;
}
