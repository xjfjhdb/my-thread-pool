#include <iostream>
#include <vector>
#include <future>
#include <chrono>
#include <iomanip>
#include <mutex>
#include "ThreadPool.h"

std::mutex cout_mutex;

int main() {
    ThreadPool pool(2);
    auto start_time = std::chrono::steady_clock::now();
    
    // 正常任务（200ms完成）
    auto normal_task = pool.enqueue(100, std::chrono::milliseconds(300), [start_time] {
        {
            std::unique_lock<std::mutex> lock(cout_mutex);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            std::cout << "[" << std::setw(4) << elapsed << "ms] 正常任务开始\n";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        {
            std::unique_lock<std::mutex> lock(cout_mutex);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            std::cout << "[" << std::setw(4) << elapsed << "ms] 正常任务完成\n";
        }
        
        return 1;
    });
    
    // 超时任务（500ms完成）
    auto timeout_task = pool.enqueue(50, std::chrono::milliseconds(300), [start_time] {
        {
            std::unique_lock<std::mutex> lock(cout_mutex);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            std::cout << "[" << std::setw(4) << elapsed << "ms] 超时任务开始\n";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        {
            std::unique_lock<std::mutex> lock(cout_mutex);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            std::cout << "[" << std::setw(4) << elapsed << "ms] 超时任务完成（不应显示）\n";
        }
        
        return 2;
    });
    
    // 获取结果
    try {
        auto normal_result = normal_task.get();
        std::unique_lock<std::mutex> lock(cout_mutex);
        std::cout << "正常任务结果: " << normal_result << std::endl;
    } catch (const std::exception& e) {
        std::unique_lock<std::mutex> lock(cout_mutex);
        std::cout << "正常任务异常: " << e.what() << std::endl;
    }
    
    try {
        auto timeout_result = timeout_task.get();
        std::unique_lock<std::mutex> lock(cout_mutex);
        std::cout << "超时任务结果: " << timeout_result << std::endl;
    } catch (const std::exception& e) {
        std::unique_lock<std::mutex> lock(cout_mutex);
        std::cout << "捕获超时异常: " << e.what() << std::endl;
    }
    
    // 等待足够时间让超时任务完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    return 0;
}

