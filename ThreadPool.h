// ThreadPool.h 完整修复代码

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <iostream>

class ThreadPool {
public:
    ThreadPool(size_t min_threads, size_t max_threads = 0);
    
    template<class F, class... Args>
    auto enqueue(int priority, std::chrono::milliseconds timeout, F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    ~ThreadPool();
    
    struct Stats {
        size_t total_tasks;
        size_t completed_tasks;
        double avg_time_ms;
        size_t queue_size;
        size_t worker_count;
    };
    
    Stats get_stats() const;
    size_t worker_count() const { return workers.size(); }

private:
    struct TaskItem {
        std::function<void()> task;
        int priority;
        std::chrono::milliseconds timeout;
        
        bool operator<(const TaskItem& other) const {
            return priority < other.priority;
        }
    };

    struct Worker {
        std::thread thread;
        std::atomic<bool> should_stop{false};
    };
    
    std::vector<std::unique_ptr<Worker>> workers;
    std::priority_queue<TaskItem> tasks;
    
    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    
    // 统计相关
    std::atomic<size_t> total_tasks_{0};
    std::atomic<size_t> completed_tasks_{0};
    std::atomic<long long> total_time_ns_{0};
    
    // 线程调整相关
    size_t min_threads_;
    size_t max_threads_;
    std::thread adjust_thread;
    
    void add_worker();
    void adjust_workers();
    void worker_loop(Worker* worker);
};

inline ThreadPool::ThreadPool(size_t min_threads, size_t max_threads)
    : stop(false), min_threads_(min_threads), 
      max_threads_(max_threads ? max_threads : min_threads * 4) {
    
    // 创建初始线程
    for(size_t i = 0; i < min_threads_; ++i) {
        add_worker();
    }
    
    // 启动线程调整器（200ms检查一次）
    adjust_thread = std::thread([this] {
        while (!stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (!stop.load()) adjust_workers();
        }
    });
}

inline void ThreadPool::add_worker() {
    auto worker = std::make_unique<Worker>();
    worker->thread = std::thread(&ThreadPool::worker_loop, this, worker.get());
    workers.push_back(std::move(worker));
}

inline void ThreadPool::worker_loop(Worker* worker) {
    while (!worker->should_stop.load()) {
        TaskItem task_item;
        bool has_task = false;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this, worker] {
                return stop.load() || worker->should_stop.load() || !tasks.empty();
            });
            
            if (worker->should_stop.load() || stop.load()) {
                return;
            }
            
            if (!tasks.empty()) {
                task_item = std::move(tasks.top());
                tasks.pop();
                has_task = true;
            }
        }
        
        if (has_task) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // 执行任务（带超时控制）
            std::promise<void> completion_promise;
            auto completion_future = completion_promise.get_future();
            std::atomic<bool> task_completed(false);
            
            std::thread task_thread([&] {
                try {
                    task_item.task();
                    task_completed.store(true);
                    completion_promise.set_value();
                } catch (...) {
                    completion_promise.set_exception(std::current_exception());
                }
            });
            
            auto status = completion_future.wait_for(task_item.timeout);
            if (status == std::future_status::timeout) {
                if (!task_completed.load()) {
                    task_thread.detach();
                }
            } else {
                if (task_thread.joinable()) task_thread.join();
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = end - start;
            total_time_ns_ += duration.count();
            completed_tasks_++;
        }
    }
}

inline void ThreadPool::adjust_workers() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    if (stop.load()) return;
    
    // 激进扩容策略：队列长度/2 + 4
    size_t ideal_threads = std::min(tasks.size() / 2 + 4, max_threads_);
    ideal_threads = std::clamp(ideal_threads, min_threads_, max_threads_);
    
    size_t current_threads = workers.size();
    
    if (ideal_threads > current_threads) {
        size_t to_add = ideal_threads - current_threads;
        std::cout << "[动态扩容] 增加 " << to_add << " 个工作线程 (当前: " 
                  << current_threads << " -> 目标: " << ideal_threads << ")\n";
        for (size_t i = 0; i < to_add; i++) {
            add_worker();
        }
    } else if (ideal_threads < current_threads) {
        size_t to_remove = current_threads - ideal_threads;
        std::cout << "[动态缩容] 减少 " << to_remove << " 个工作线程 (当前: " 
                  << current_threads << " -> 目标: " << ideal_threads << ")\n";
        
        size_t removed = 0;
        for (auto it = workers.rbegin(); it != workers.rend() && removed < to_remove; ++it) {
            if (!(*it)->should_stop.load()) {
                (*it)->should_stop.store(true);
                removed++;
            }
        }
        condition.notify_all();
    }
}

template<class F, class... Args>
auto ThreadPool::enqueue(int priority, std::chrono::milliseconds timeout, F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop.load()) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        
        tasks.push(TaskItem{
            [task]() { (*task)(); },
            priority,
            timeout
        });
        
        total_tasks_++;
    }
    
    condition.notify_one();
    return res;
}

inline ThreadPool::Stats ThreadPool::get_stats() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    double avg_ms = 0.0;
    if (completed_tasks_ > 0) {
        avg_ms = (total_time_ns_.load() / 1e6) / completed_tasks_;
    }
    
    return {
        total_tasks_.load(),
        completed_tasks_.load(),
        avg_ms,
        tasks.size(),
        workers.size()
    };
}

inline ThreadPool::~ThreadPool() {
    stop.store(true);
    
    // 标记所有工作线程停止
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        for (auto& worker : workers) {
            worker->should_stop.store(true);
        }
    }
    
    condition.notify_all();
    
    // 等待所有工作线程结束
    for (auto& worker : workers) {
        if (worker->thread.joinable()) {
            worker->thread.join();
        }
    }
    
    if (adjust_thread.joinable()) adjust_thread.join();
    
    // 清空任务队列
    std::priority_queue<TaskItem> empty;
    std::swap(tasks, empty);
}
