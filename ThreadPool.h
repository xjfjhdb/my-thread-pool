#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <type_traits>
#include <sstream>

class ThreadPool {
public:
    ThreadPool(size_t threads);
    
    template<class F, class... Args>
    auto enqueue(int priority, std::chrono::milliseconds timeout, F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    ~ThreadPool();

private:
    struct TaskItem {
        std::function<void()> task;
        int priority;
        std::chrono::milliseconds timeout;
        std::chrono::time_point<std::chrono::steady_clock> start_time;
        
        bool operator<(const TaskItem& other) const {
            return priority < other.priority;
        }
    };

    std::vector<std::thread> workers;
    std::priority_queue<TaskItem> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};

inline ThreadPool::ThreadPool(size_t threads) : stop(false) {
    for(size_t i = 0; i < threads; ++i)
        workers.emplace_back([this] {
            for(;;) {
                TaskItem task_item;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this] { 
                        return this->stop.load() || !this->tasks.empty(); 
                    });
                    
                    if(this->stop.load() && this->tasks.empty())
                        return;
                    
                    task_item = std::move(this->tasks.top());
                    this->tasks.pop();
                }
                
                auto task_start = std::chrono::steady_clock::now();
                
                std::promise<void> completion_promise;
                auto completion_future = completion_promise.get_future();
                std::atomic<bool> task_completed(false);
                std::atomic<bool> timed_out(false);
                
                std::thread task_thread([&] {
                    try {
                        task_item.task();
                        task_completed.store(true);
                        
                        if (!timed_out.load()) {
                            completion_promise.set_value();
                        }
                    } catch (...) {
                        if (!timed_out.load()) {
                            completion_promise.set_exception(std::current_exception());
                        }
                    }
                });
                
                auto status = completion_future.wait_for(task_item.timeout);
                if (status == std::future_status::timeout) {
                    timed_out.store(true);
                    if (!task_completed.load()) {
                        task_thread.detach();
                        
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - task_start).count();
                        
                        // 抛出超时异常
                        try {
                            std::ostringstream oss;
                            oss << "Task timeout after " << elapsed << "ms";
                            throw std::runtime_error(oss.str());
                        } catch (...) {
                            completion_promise.set_exception(std::current_exception());
                        }
                    } else {
                        task_thread.join();
                    }
                } else {
                    task_thread.join();
                }
            }
        });
}

template<class F, class... Args>
auto ThreadPool::enqueue(int priority, std::chrono::milliseconds timeout, F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    auto start_time = std::chrono::steady_clock::now();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        
        if(stop.load())
            throw std::runtime_error("enqueue on stopped ThreadPool");
        
        tasks.push(TaskItem{
            [task]() { (*task)(); },
            priority,
            timeout,
            start_time
        });
    }
    
    condition.notify_one();
    return res;
}

inline ThreadPool::~ThreadPool() {
    stop.store(true);
    condition.notify_all();
    for(std::thread &worker : workers)
        worker.join();
}

#endif

