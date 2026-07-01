#pragma once

#include <asio.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

class AsioIOContextPool {
public:
    enum class ServiceType { TCP, HTTP, RTSP };

    static AsioIOContextPool& GetInstance(ServiceType type = ServiceType::TCP) {
        (void)type;
        static AsioIOContextPool instance;
        return instance;
    }

    asio::io_context& GetIOContext() {
        const std::size_t index =
            next_index_.fetch_add(1, std::memory_order_relaxed);
        return *io_contexts_[index % io_contexts_.size()];
    }

    std::size_t Size() const { return io_contexts_.size(); }

    void Stop() {
        const bool was_stopped = stopped_.exchange(true);
        if (was_stopped) {
            return;
        }

        for (auto& guard : work_guards_) {
            guard.reset();
        }
        for (auto& io_context : io_contexts_) {
            io_context->stop();
        }
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                if (thread.get_id() == std::this_thread::get_id()) {
                    thread.detach();
                } else {
                    thread.join();
                }
            }
        }
    }

private:
    using WorkGuard =
        asio::executor_work_guard<asio::io_context::executor_type>;

    AsioIOContextPool()
        : AsioIOContextPool(DefaultPoolSize()) {
    }

    explicit AsioIOContextPool(std::size_t size) {
        const std::size_t pool_size = std::max<std::size_t>(1, size);
        io_contexts_.reserve(pool_size);
        work_guards_.reserve(pool_size);
        threads_.reserve(pool_size);

        for (std::size_t i = 0; i < pool_size; ++i) {
            auto io_context = std::make_unique<asio::io_context>();
            work_guards_.push_back(
                std::make_unique<WorkGuard>(
                    asio::make_work_guard(*io_context)));
            io_contexts_.push_back(std::move(io_context));
        }

        for (auto& io_context : io_contexts_) {
            threads_.emplace_back([io = io_context.get()]() {
                io->run();
            });
        }
    }

    ~AsioIOContextPool() {
        Stop();
    }

    static std::size_t DefaultPoolSize() {
        const unsigned int hardware_threads = std::thread::hardware_concurrency();
        if (hardware_threads == 0) {
            return 2;
        }
        return std::clamp<std::size_t>(hardware_threads, 2, 4);
    }

    std::vector<std::unique_ptr<asio::io_context>> io_contexts_;
    std::vector<std::unique_ptr<WorkGuard>> work_guards_;
    std::vector<std::thread> threads_;
    std::atomic<std::size_t> next_index_{0};
    std::atomic<bool> stopped_{false};
};
