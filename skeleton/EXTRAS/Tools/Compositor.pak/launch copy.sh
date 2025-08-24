#!/bin/sh
# Compositor.pak/launch.sh - 混合架构合成器演示工具
# 适配新的直接IPC + framebuffer拦截架构

TOOL_TAG="Compositor"
SDCARD_PATH="/mnt/SDCARD"
SYSTEM_PATH="$SDCARD_PATH/.system"  
LOGS_PATH="$SYSTEM_PATH/logs"
USERDATA_PATH="$SYSTEM_PATH/userdata"
PAK_PATH="$SDCARD_PATH/Tools/$TOOL_TAG.pak"

# 创建必要目录
mkdir -p "$LOGS_PATH"
mkdir -p "$USERDATA_PATH"

# 环境设置
HOME="$USERDATA_PATH"
cd "$PAK_PATH" || exit 1

# 清理函数
cleanup() {
    echo "清理合成器进程..." >> "$LOGS_PATH/$TOOL_TAG.txt"
    killall compositor.elf background.elf overlay.elf 2>/dev/null || true
    rm -f /dev/shm/compositor_fb_shm 2>/dev/null || true
    rm -f /tmp/compositor_exit 2>/dev/null || true
    echo "清理完成" >> "$LOGS_PATH/$TOOL_TAG.txt"
}

# 设置信号处理
trap cleanup EXIT INT TERM

echo "=== 合成器演示启动 $(date) ===" > "$LOGS_PATH/$TOOL_TAG.txt"
echo "工作目录: $(pwd)" >> "$LOGS_PATH/$TOOL_TAG.txt"

# 检查必需文件存在性
REQUIRED_FILES="compositor.elf background.elf overlay.elf libfb_compositor.so"

for file in $REQUIRED_FILES; do
    if [ ! -f "$file" ]; then
        echo "错误: 文件不存在 - $file" >> "$LOGS_PATH/$TOOL_TAG.txt"
        exit 1
    fi
done

echo "所有必需文件检查完成" >> "$LOGS_PATH/$TOOL_TAG.txt"

# 设置framebuffer权限
if [ -c /dev/fb0 ]; then
    chmod 666 /dev/fb0 2>/dev/null && echo "Framebuffer权限已设置" >> "$LOGS_PATH/$TOOL_TAG.txt"
else
    echo "警告: /dev/fb0 不存在" >> "$LOGS_PATH/$TOOL_TAG.txt"
fi

# 清理旧进程和共享内存
cleanup 2>/dev/null
sleep 1

# 清理旧进程和共享内存
cleanup 2>/dev/null
sleep 1

# 设置执行权限
chmod +x compositor.elf background.elf overlay.elf

echo "启动合成器..." >> "$LOGS_PATH/$TOOL_TAG.txt"
./compositor.elf >> "$LOGS_PATH/$TOOL_TAG.txt" 2>&1 &
COMPOSITOR_PID=$!

# 等待合成器初始化
sleep 3

if ! kill -0 $COMPOSITOR_PID 2>/dev/null; then
    echo "错误: 合成器启动失败" >> "$LOGS_PATH/$TOOL_TAG.txt"
    exit 1
fi

echo "合成器已启动 (PID: $COMPOSITOR_PID)" >> "$LOGS_PATH/$TOOL_TAG.txt"

echo "启动背景程序..." >> "$LOGS_PATH/$TOOL_TAG.txt" 
LD_PRELOAD=./libfb_compositor.so ./background.elf >> "$LOGS_PATH/$TOOL_TAG.txt" 2>&1 &
BG_PID=$!
sleep 2

echo "启动叠加程序..." >> "$LOGS_PATH/$TOOL_TAG.txt"
COMPOSITOR_LAYER=1 LD_PRELOAD=./libfb_compositor.so ./overlay.elf >> "$LOGS_PATH/$TOOL_TAG.txt" 2>&1 &
OV_PID=$!

echo "所有程序已启动:" >> "$LOGS_PATH/$TOOL_TAG.txt"
echo "  合成器: $COMPOSITOR_PID" >> "$LOGS_PATH/$TOOL_TAG.txt"  
echo "  背景程序: $BG_PID" >> "$LOGS_PATH/$TOOL_TAG.txt"
echo "  叠加程序: $OV_PID" >> "$LOGS_PATH/$TOOL_TAG.txt"

# 监控运行状态
counter=0
max_runtime=1800  # 30分钟最大运行时间
stats_interval=60  # 统计间隔

echo "开始监控运行状态..." >> "$LOGS_PATH/$TOOL_TAG.txt"

while kill -0 $COMPOSITOR_PID 2>/dev/null; do
    sleep 5
    counter=$((counter + 5))
    
    # 检查后台进程状态
    if [ $((counter % 30)) -eq 0 ]; then
        bg_status="运行"
        ov_status="运行"
        
        if ! kill -0 $BG_PID 2>/dev/null; then
            bg_status="已停止"
        fi
        
        if ! kill -0 $OV_PID 2>/dev/null; then
            ov_status="已停止"
        fi
        
        echo "进程状态检查 - 背景:$bg_status, 叠加:$ov_status" >> "$LOGS_PATH/$TOOL_TAG.txt"
    fi
    
    # 每分钟记录详细状态
    if [ $((counter % stats_interval)) -eq 0 ]; then
        echo "=== 运行状态报告 (${counter}s) - $(date) ===" >> "$LOGS_PATH/$TOOL_TAG.txt"
        
        # 检查共享内存
        if [ -f "/dev/shm/compositor_fb_shm" ]; then
            shm_size=$(ls -lh /dev/shm/compositor_fb_shm | awk '{print $5}')
            echo "共享内存状态: 正常 ($shm_size)" >> "$LOGS_PATH/$TOOL_TAG.txt"
        else
            echo "共享内存状态: 异常" >> "$LOGS_PATH/$TOOL_TAG.txt"
        fi
        
        # 内存使用情况
        if command -v free >/dev/null 2>&1; then
            mem_info=$(free -m | grep "Mem:" | awk '{printf "使用:%dMB 可用:%dMB", $3, $7}')
            echo "内存状态: $mem_info" >> "$LOGS_PATH/$TOOL_TAG.txt"
        fi
        
        echo "==============================" >> "$LOGS_PATH/$TOOL_TAG.txt"
    fi
    
    # 检查最大运行时间
    if [ $counter -ge $max_runtime ]; then
        echo "达到最大运行时间 (${max_runtime}s)，自动退出" >> "$LOGS_PATH/$TOOL_TAG.txt"
        break
    fi
    
    # 检查手动退出信号
    if [ -f "/tmp/compositor_exit" ]; then
        echo "接收到手动退出信号" >> "$LOGS_PATH/$TOOL_TAG.txt"
        rm -f "/tmp/compositor_exit"
        break
    fi
    
    # 检查framebuffer状态
    if [ $((counter % 300)) -eq 0 ] && [ ! -c /dev/fb0 ]; then
        echo "警告: framebuffer设备丢失" >> "$LOGS_PATH/$TOOL_TAG.txt"
    fi
done

echo "合成器主进程已停止" >> "$LOGS_PATH/$TOOL_TAG.txt"

# 等待子进程结束
if kill -0 $BG_PID 2>/dev/null || kill -0 $OV_PID 2>/dev/null; then
    echo "等待子进程结束..." >> "$LOGS_PATH/$TOOL_TAG.txt"
    sleep 2
    
    # 强制结束仍在运行的进程
    kill $BG_PID $OV_PID 2>/dev/null || true
    sleep 1
    kill -9 $BG_PID $OV_PID 2>/dev/null || true
fi

echo "=== 合成器演示结束 - $(date) ===" >> "$LOGS_PATH/$TOOL_TAG.txt"
echo "总运行时间: ${counter}秒" >> "$LOGS_PATH/$TOOL_TAG.txt"

cleanup

# 生成运行报告
echo "性能报告已保存到: $LOGS_PATH/$TOOL_TAG.txt"