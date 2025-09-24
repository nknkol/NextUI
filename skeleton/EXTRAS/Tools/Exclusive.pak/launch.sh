#!/bin/sh

# 1. 定义日志文件和IPC路径
COMPOSITOR_LOG="./compositor_log.txt"
APP_B_LOG="./app_B_log.txt"
APP_C_LOG="./app_C_log.txt"
# --- 新增: App D 和 E 的日志文件 ---
APP_D_LOG="./app_D_log.txt"
APP_E_LOG="./app_E_log.txt"
COMPOSITOR_PID_FILE="/tmp/compositor.pid"

# 2. 进入.pak包所在的目录
cd $(dirname "$0")

# 3. 清理旧的文件
rm -f $COMPOSITOR_LOG $APP_B_LOG $APP_C_LOG $APP_D_LOG $APP_E_LOG $COMPOSITOR_PID_FILE

# 4. 定义清理函数
cleanup() {
    echo "\n--- Cleaning up background processes... ---"
    # 使用 pkill 更可靠，可以杀死所有实例
    pkill compositor.elf
    pkill app_B_overlay.elf
    pkill app_C_exclusive.elf
    pkill app_D_overlay.elf
    pkill app_E_overlay.elf
    rm -f $COMPOSITOR_PID_FILE
}
trap cleanup INT TERM

# 5. 在后台启动合成器和所有叠加层客户端
echo "Starting compositor service... (Log: $COMPOSITOR_LOG)"
./compositor.elf --home-slot 0 > $COMPOSITOR_LOG 2>&1 &
COMPOSITOR_PID=$!
echo $COMPOSITOR_PID > $COMPOSITOR_PID_FILE
echo "Compositor running with PID: $COMPOSITOR_PID"
sleep 1 

echo "Starting overlay client app_B (slot 1)... (Log: $APP_B_LOG)"
./app_B_overlay.elf 1 > $APP_B_LOG 2>&1 &

# --- 新增: 启动 App D 和 E ---
echo "Starting overlay client app_D (slot 2)... (Log: $APP_D_LOG)"
./app_D_overlay.elf 2 > $APP_D_LOG 2>&1 &

echo "Starting overlay client app_E (slot 3)... (Log: $APP_E_LOG)"
./app_E_overlay.elf 3 > $APP_E_LOG 2>&1 &


# 6. 在前台运行主交互程序 (app_C)
echo "Starting main interactive app_C (slot 0)... (Log: $APP_C_LOG)"
echo "--- Press [START] to switch modes, [SELECT] to exit ---"
./app_C_exclusive.elf 0 > $APP_C_LOG 2>&1

# 7. 当 app_C 退出后，执行清理工作
echo "Main app exited. Cleaning up services..."
cleanup

echo "Cleanup complete. Returning to NextUI."
exit 0