#!/bin/sh

# 1. 定义日志文件和IPC路径
COMPOSITOR_LOG="./compositor_log.txt"
APP_B_LOG="./app_B_log.txt"
APP_C_LOG="./app_C_log.txt"
FIFO_PATH="/tmp/compositor_cmd_fifo"

# 2. 进入.pak包所在的目录
cd $(dirname "$0")

# 3. 清理旧的日志和FIFO文件
rm -f $COMPOSITOR_LOG $APP_B_LOG $APP_C_LOG $FIFO_PATH

# 4. 定义清理函数
cleanup() {
    echo "\n--- Cleaning up background processes... ---"
    pkill compositor.elf
    pkill app_B_overlay.elf
    pkill app_C_exclusive.elf
    rm -f $FIFO_PATH
}
trap cleanup INT TERM

# 5. 在后台启动合成器和叠加层客户端
echo "Starting compositor service... (Log: $COMPOSITOR_LOG)"
./compositor.elf &> $COMPOSITOR_LOG &
COMPOSITOR_PID=$!
sleep 1 

echo "Starting overlay client app_B... (Log: $APP_B_LOG)"
# MODIFIED: 根据您的日志，app_B 使用 slot 1
./app_B_overlay.elf 1 &> $APP_B_LOG &
APP_B_PID=$!

# 6. 在前台运行主交互程序 (app_C)
echo "Starting main interactive app_C... (Log: $APP_C_LOG)"
echo "--- Press [START] to switch modes, [SELECT] to exit ---"
# App C 使用 slot 0
./app_C_exclusive.elf 0 &> $APP_C_LOG

# 7. 当 app_C 退出后，执行清理工作
echo "Main app exited. Cleaning up services..."
kill $COMPOSITOR_PID
kill $APP_B_PID

echo "Cleanup complete. Returning to NextUI."
exit 0