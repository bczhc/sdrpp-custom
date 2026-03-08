//
// Created by bczhc on 08/03/26.
//

#include "commands.h"
#include "utils/flog.h"

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <sys/stat.h>

std::atomic<int> cmd_spectrum_shift(0);
std::atomic<float> cmd_zoom_factor(1.0f);
std::atomic<bool> cmd_panel_toggle(false);
std::atomic<float> cmd_fft_min_change(0.0);
std::atomic<float> cmd_volume_delta; // 0x06
std::atomic<float> cmd_zoom_factor_delta; // 0x07

void commandInputWorker() {
    const char* pipePath = "./command";

    // 1. 检查文件是否存在
    struct stat st;
    if (stat(pipePath, &st) != 0) {
        return;
    }

    // 2. 打开 Pipe (以只读模式阻塞打开)
    int fd = open(pipePath, O_RDONLY);
    if (fd == -1) return;

    flog::info("Command pipe detected at ./command. Starting input reader thread.");

    uint8_t cmd;
    while (read(fd, &cmd, 1) > 0) {
        flog::info("Cmd read: {}", cmd);
        switch (cmd) {
        case 0x01: // 左移
            cmd_spectrum_shift.store(-1);
            break;

        case 0x02: // 右移
            cmd_spectrum_shift.store(1);
            break;

        case 0x03: { // 缩放
            float factor;
            // 继续读取 4 字节的 float (Little Endian)
            if (read(fd, &factor, 4) == 4) {
                cmd_zoom_factor.store(factor);
            }
            break;
        }

        case 0x04: // Panel 切换
            // 这里可以用取反或者设为 true，由 UI 逻辑处理后置为 false
            cmd_panel_toggle.store(true);
            break;

        case 0x05: // fft min change
            float deltaFftMin;
            if (read(fd, &deltaFftMin, 4) == 4) {
                cmd_fft_min_change.store(deltaFftMin);
            }
            break;

        case 0x06:
            // volume
            float deltaVolume;
            if (read(fd, &deltaVolume, 4) == 4) {
                cmd_volume_delta.store(deltaVolume);
            }
            break;

        case 0x07:
            // zoom delta
            float deltaZoom;
            if (read(fd, &deltaZoom, 4) == 4) {
                cmd_zoom_factor_delta.store(deltaZoom);
            }
            break;

        default:
            // 忽略未知指令
            break;
        }
    }

    close(fd);
    flog::info("Command pipe closed.");
}

void setupCommandInputReader() {
    // 启动脱离主线程的异步线程
    std::thread worker(commandInputWorker);
    worker.detach();
}