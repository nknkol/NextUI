#!/bin/sh

# 定义FIFO路径，与protocol.h中一致
FIFO_PATH="/tmp/compositor_cmd_fifo"
# 进入脚本所在目录
cd $(dirname "$0")

# --- 清理函数 ---
# 确保在脚本退出时能杀掉所有后台进程
cleanup() {
    echo "--- Cleaning up and shutting down... ---"
    # 如果PID存在，则kill掉
    [ -n "$COMPOSITOR_PID" ] && kill $COMPOSITOR_PID 2>/dev/null
    [ -n "$APP_A_PID" ] && kill $APP_A_PID 2>/dev/null
    [ -n "$APP_B_PID" ] && kill $APP_B_PID 2>/dev/null
    rm -f $FIFO_PATH
    echo "--- Cleanup complete. ---"
}

# 设置trap，当脚本被中断(Ctrl+C)或正常退出时，调用cleanup函数
trap cleanup EXIT

# --- 辅助函数 ---
# 用于向合成器发送命令
send_cmd() {
    if [ ! -p "$FIFO_PATH" ]; then
        echo "Error: Compositor command FIFO not found at $FIFO_PATH"
        exit 1
    fi
    echo "Sending command: $1"
    echo "$1" > "$FIFO_PATH"
}

# --- 1. 初始化 ---
echo "--- Starting Compositor... ---"
# 在后台启动合成器，并将日志输出到compositor_log.txt
./compositor.elf &> ./compositor_log.txt &
COMPOSITOR_PID=$!
# 等待1秒，确保合成器已成功创建FIFO
sleep 1

# --- 2. 启动客户端应用 ---
echo "--- Starting client applications... ---"
# 启动App A，请求使用slot 0
./app_A.elf 0 &
APP_A_PID=$!
# 启动App B，请求使用slot 1
./app_B_overlay.elf 1 &
APP_B_PID=$!

# 等待应用注册
sleep 2

# --- 3. 开始测试场景 ---

# 场景一: App A (slot 0) 作为主显示
echo "\n--- (SCENARIO 1) Activating App A (Slot 0). Screen should show App A. ---"
send_cmd "SET_ACTIVE 0"
sleep 5 # 等待5秒观察效果

# 场景二: 切换到 App B (slot 1) 作为主显示
echo "\n--- (SCENARIO 2) Switching to App B (Slot 1). Screen should now show App B. ---"
send_cmd "SET_ACTIVE 1"
sleep 5 # 等待5秒观察效果

# 场景三: 将 App B (slot 1) 作为叠加层，显示在 App A (slot 0) 之上
echo "\n--- (SCENARIO 3) Activating App A again, with App B as an overlay. ---"
send_cmd "SET_ACTIVE 0"
sleep 1 # 等待切换完成
send_cmd "SET_OVERLAY 1"
echo "--- Screen should now show App A in the background and App B (with transparency) on top. ---"
sleep 5 # 等待5秒观察效果

# 场景四: 清除叠加层，只显示 App A
echo "\n--- (SCENARIO 4) Clearing the overlay. Screen should return to showing only App A. ---"
send_cmd "CLEAR_OVERLAY"
sleep 5 # 等待5秒观察效果

# --- 4. 测试结束 ---
echo "\n--- Test sequence finished. Shutting down. ---"

# cleanup函数将在这里被自动调用
exit 0