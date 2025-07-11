#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <mutex>  // 添加互斥锁支持
#include "ThreadPool.h"

// 全局互斥锁用于保护输出
std::mutex cout_mutex;

int main() {
    ThreadPool pool(4);
    std::vector<std::future<int>> results;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 添加任务，显示优先级
    for(int i = 0; i < 10; ++i) {
        int priority = (i % 3 == 0) ? 100 : 50;  // 高优先级任务
        
        results.emplace_back(
            pool.enqueue(priority, [i, priority, start_time] {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                
                // 使用互斥锁保护输出
                {
                    std::unique_lock<std::mutex> lock(cout_mutex);
                    std::cout << "[" << std::setw(4) << elapsed << "ms] "
                              << "任务 " << i << " (优先级:" 
                              << std::setw(3) << priority << ") 开始\n";
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                
                now = std::chrono::steady_clock::now();
                elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                
                // 使用互斥锁保护输出
                {
                    std::unique_lock<std::mutex> lock(cout_mutex);
                    std::cout << "[" << std::setw(4) << elapsed << "ms] "
                              << "任务 " << i << " (优先级:" 
                              << std::setw(3) << priority << ") 结束\n";
                }
                
                return i * i;
            })
        );
    }

    // 获取结果
    std::cout << "\n结果: ";
    for(auto &&result : results) {
        std::cout << result.get() << ' ';
    }
    std::cout << std::endl;

    return 0;
}

