#include <iostream>
#include <vector>
#include <chrono>
#include "ThreadPool.h"

int main()
{
    ThreadPool pool(4);
    std::vector<std::future<int>> results;

    for (int i = 0; i < 10; ++i) {
        results.emplace_back(
            pool.enqueue([i] {
                std::cout << "Task " << i << " started" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::cout << "Task " << i << " finished" << std::endl;
                return i * i;
            })
        );
    }

    for (auto &&result : results) {
        std::cout << result.get() << ' ';
    }
    std::cout << std::endl;

    std::cout << "\nMy first thread pool runs successfully!" << std::endl;

    return 0;
}

