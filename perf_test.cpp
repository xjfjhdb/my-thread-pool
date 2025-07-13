#include "ThreadPool.h"
#include <chrono>
#include <iostream>
#include <cmath>
#include <vector>

// 模拟计算密集型任务（增加计算量）
void compute_task() {
    double sum = 0;
    for (int i = 0; i < 10000000; ++i) {
        sum += std::sin(i) * std::cos(i);
    }
}

// 模拟IO密集型任务（增加等待时间）
void io_task() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

int main() {
    std::cout << "===== 线程池性能测试 =====" << std::endl;
    std::cout << "初始线程数: 4, 最大线程数: 16" << std::endl;
    std::cout << "任务总数: 500 (40% 计算任务, 60% IO任务)" << std::endl;
    std::cout << "==========================" << std::endl;
    
    // 创建线程池：4-16个动态线程
    ThreadPool pool(4, 16);
    const int TOTAL_TASKS = 500;
    
    // 启动性能测试
    auto start = std::chrono::high_resolution_clock::now();
    
    // 提交混合任务
    for (int i = 0; i < TOTAL_TASKS; ++i) {
        if (i % 5 < 2) { // 40% 计算任务
            pool.enqueue(50, std::chrono::seconds(10), compute_task);
        } else { // 60% IO任务
            pool.enqueue(30, std::chrono::seconds(10), io_task);
        }
    }
    
    std::cout << "所有任务已提交，开始监控..." << std::endl;
    
    // 监控循环
    int last_completed = 0;
    auto last_time = start;
    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        auto stats = pool.get_stats();
        
        // 计算瞬时QPS
        double instant_qps = 0.0;
        if (auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count()) {
            instant_qps = (stats.completed_tasks - last_completed) * 1000.0 / time_diff;
        }
        
        // 计算平均QPS
        double avg_qps = 0.0;
        if (elapsed > 0) {
            avg_qps = stats.completed_tasks * 1000.0 / elapsed;
        }
        
        // 打印统计信息
        std::cout << "\n===== 线程池性能监控 =====\n";
        std::cout << "运行时间: " << elapsed << "ms\n";
        std::cout << "总任务数: " << stats.total_tasks << " | 已完成: " << stats.completed_tasks 
                  << " | 队列中: " << stats.queue_size << "\n";
        std::cout << "工作线程: " << stats.worker_count << "\n";
        std::cout << "平均耗时: " << stats.avg_time_ms << "ms\n";
        std::cout << "瞬时QPS: " << instant_qps << " | 平均QPS: " << avg_qps << "\n";
        std::cout << "==========================" << std::endl;
        
        // 更新上次状态
        last_completed = stats.completed_tasks;
        last_time = now;
        
        // 检查是否完成所有任务
        if (stats.completed_tasks >= TOTAL_TASKS) {
            std::cout << "\n✅ 所有任务完成! 最终QPS: " << avg_qps << "\n";
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    return 0;
}
