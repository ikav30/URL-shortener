#ifndef CLICK_LOGGER_H
#define CLICK_LOGGER_H

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include "database.h"

// Moves click persistence OFF the request hot-path. Redirects just enqueue a code (O(1));
// a background thread drains the queue and writes clicks to SQLite in batched transactions.
class ClickLogger {
public:
    explicit ClickLogger(Database& db) : db_(db), running_(true) {
        worker_ = std::thread([this] { run(); });
    }
    ~ClickLogger() {
        { std::lock_guard<std::mutex> lk(mtx_); running_ = false; }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    // Called on every redirect — cheap, never touches disk.
    void enqueue(const std::string& code) {
        { std::lock_guard<std::mutex> lk(mtx_); queue_.push(code); }
        cv_.notify_one();
    }

private:
    void run() {
        while (true) {
            std::vector<std::string> batch;
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait(lk, [this] { return !queue_.empty() || !running_; });
                if (!running_ && queue_.empty()) return;
                while (!queue_.empty() && batch.size() < 1000) {
                    batch.push_back(std::move(queue_.front()));
                    queue_.pop();
                }
            }
            db_.logClicksBatch(batch);   // one transaction per batch
        }
    }

    Database&               db_;
    std::queue<std::string> queue_;
    std::mutex              mtx_;
    std::condition_variable cv_;
    std::thread            worker_;
    std::atomic<bool>      running_;
};

#endif
