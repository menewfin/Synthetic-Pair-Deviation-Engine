#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>

namespace arbitrage {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // Delete copy constructor and assignment
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // Submit a task to the thread pool
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using return_type = decltype(f(args...));
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            
            tasks_.emplace([task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return result;
    }
    
    // Submit a batch of tasks
    template<typename F, typename Iterator>
    std::vector<std::future<decltype(std::declval<F>()(std::declval<typename Iterator::value_type>()))>>
    submit_batch(F&& f, Iterator begin, Iterator end) {
        using value_type = typename Iterator::value_type;
        using return_type = decltype(f(std::declval<value_type>()));
        
        std::vector<std::future<return_type>> futures;
        futures.reserve(std::distance(begin, end));
        
        for (auto it = begin; it != end; ++it) {
            futures.push_back(submit(f, *it));
        }
        
        return futures;
    }
    
    // Wait for all tasks to complete
    void wait_all();
    
    // Get the number of worker threads
    size_t num_threads() const { return workers_.size(); }
    
    // Get the number of pending tasks
    size_t pending_tasks() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }
    
    // Get the number of active tasks
    size_t active_tasks() const {
        return active_tasks_.load();
    }
    
    // Stop the thread pool
    void stop();
    
private:
    void worker_thread();
    
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::condition_variable finished_condition_;
    
    std::atomic<bool> stop_{false};
    std::atomic<size_t> active_tasks_{0};
    std::atomic<size_t> total_tasks_processed_{0};
};

// Global thread pool instance
class GlobalThreadPool {
public:
    static ThreadPool& instance() {
        static ThreadPool pool;
        return pool;
    }
    
    template<typename F, typename... Args>
    static auto submit(F&& f, Args&&... args) {
        return instance().submit(std::forward<F>(f), std::forward<Args>(args)...);
    }
};

// RAII helper for parallel execution
template<typename F>
class ParallelExecutor {
public:
    ParallelExecutor(ThreadPool& pool, size_t num_tasks)
        : pool_(pool), futures_() {
        futures_.reserve(num_tasks);
    }
    
    template<typename... Args>
    void add_task(F&& f, Args&&... args) {
        futures_.push_back(pool_.submit(std::forward<F>(f), std::forward<Args>(args)...));
    }
    
    void wait() {
        for (auto& future : futures_) {
            future.wait();
        }
    }
    
    auto get_results() {
        using result_type = typename decltype(futures_)::value_type::value_type;
        std::vector<result_type> results;
        results.reserve(futures_.size());
        
        for (auto& future : futures_) {
            results.push_back(future.get());
        }
        
        return results;
    }
    
private:
    ThreadPool& pool_;
    std::vector<std::future<decltype(std::declval<F>()())>> futures_;
};

// Thread pool implementation
inline ThreadPool::ThreadPool(size_t num_threads) {
    workers_.reserve(num_threads);
    
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] { worker_thread(); });
    }
}

inline ThreadPool::~ThreadPool() {
    stop();
}

inline void ThreadPool::worker_thread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            
            if (stop_ && tasks_.empty()) {
                return;
            }
            
            task = std::move(tasks_.front());
            tasks_.pop();
            active_tasks_++;
        }
        
        task();
        
        active_tasks_--;
        total_tasks_processed_++;
        finished_condition_.notify_all();
    }
}

inline void ThreadPool::wait_all() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    finished_condition_.wait(lock, [this] {
        return tasks_.empty() && active_tasks_ == 0;
    });
}

inline void ThreadPool::stop() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    
    condition_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

} // namespace arbitrage