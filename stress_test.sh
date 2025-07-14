#!/bin/bash
# 启动HTTP服务器
./ThreadPool_HTTPServer &
SERVER_PID=$!
echo "Server started with PID: $SERVER_PID"

# 等待服务器启动
sleep 2

# 第一阶段测试：100并发，1000请求
echo -e "\n===== 第一阶段测试: 100并发 ====="
ab -c 100 -n 1000 http://localhost:8080/threadpool

# 第二阶段测试：1000并发，10000请求
echo -e "\n===== 第二阶段测试: 1000并发 ====="
ab -c 1000 -n 10000 http://localhost:8080/threadpool

# 第三阶段测试：5000并发，50000请求
echo -e "\n===== 第三阶段测试: 5000并发 ====="
ab -c 5000 -n 50000 http://localhost:8080/threadpool

# 停止服务器
kill $SERVER_PID
echo "Server stopped"
