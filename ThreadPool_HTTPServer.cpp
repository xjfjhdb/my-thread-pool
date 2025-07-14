#include "ThreadPool.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>

const int PORT = 8080;
const int MAX_CONNECTIONS = 10000;

// 处理HTTP请求
void handle_client(int client_socket) {
    char buffer[4096] = {0};
    read(client_socket, buffer, 4095);
    
    // 解析请求路径 (简化版)
    std::string request(buffer);
    std::string path = "/";
    if (request.find("GET") == 0) {
        size_t start = request.find(' ') + 1;
        size_t end = request.find(' ', start);
        path = request.substr(start, end - start);
    }
    
    // 构造响应
    std::string response;
    if (path == "/threadpool") {
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/plain\r\n"
                   "Connection: close\r\n\r\n"
                   "ThreadPool is working!";
    } else {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Connection: close\r\n\r\n"
                   "Resource not found";
    }
    
    write(client_socket, response.c_str(), response.size());
    close(client_socket);
}

int main() {
    // 创建线程池：初始10线程，最大100线程
    ThreadPool pool(10, 100);
    
    // 创建服务器套接字
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Socket creation failed" << std::endl;
        return 1;
    }
    
    // 设置SO_REUSEADDR
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 绑定端口
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        return 1;
    }
    
    // 开始监听
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        std::cerr << "Listen failed" << std::endl;
        return 1;
    }
    
    std::cout << "Server running on port " << PORT << std::endl;
    
    // 主循环
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }
        
        // 将连接交给线程池处理
        pool.enqueue(50, std::chrono::seconds(5), [client_socket] {
            handle_client(client_socket);
        });
    }
    
    close(server_fd);
    return 0;
}
