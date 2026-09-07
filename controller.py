import os
import time
import threading
import struct
from evdev import InputDevice, ecodes

# --- 常量配置区 ---
FIFO_PATH = '/home/bczhc/command'
DEVICE_PATH = '/dev/input/by-id/usb-Flydigi_Flydigi_Direwolf_4_Flydigi_Direwolf_4-event-joystick'

THUMBL_VALUES = [0.1323518039, 1.0]
DPAD_Y_VAL = 3.0          
STICK_Y_VAL = 0.01        
STICK_X_FN_VAL = 0.01

BASE_INTERVAL = 0.05      
MAX_STICK_VALUE = 32767   
# ----------------

state = {
    "dir_x": 0,        # 0:停, 1:左, 2:右
    "dir_y": 0,        # 0:停, 1:上, 2:下
    "is_max_x": False, 
    "fn_pressed": False, 
    "running": True,
    "thumbl_idx": 0    
}

def setup_fifo():
    if not os.path.exists(FIFO_PATH):
        os.mkfifo(FIFO_PATH)
    return os.open(FIFO_PATH, os.O_RDWR)

def writer_thread(fifo_fd):
    """负责持续发送的逻辑"""
    while state["running"]:
        active = False
        
        # 1. 处理 X 轴 (左右)
        if state["dir_x"] != 0:
            active = True
            current_interval = BASE_INTERVAL / 2 if state["is_max_x"] else BASE_INTERVAL
            if state["fn_pressed"]:
                val = -STICK_X_FN_VAL if state["dir_x"] == 1 else STICK_X_FN_VAL
                os.write(fifo_fd, struct.pack('<Bf', 0x06, val))
            else:
                # 左推 0x02, 右推 0x01
                os.write(fifo_fd, b'\x02' if state["dir_x"] == 1 else b'\x01')
            time.sleep(current_interval)
        
        # 2. 处理 Y 轴 (上下)
        elif state["dir_y"] != 0:
            active = True
            val = -STICK_Y_VAL if state["dir_y"] == 1 else STICK_Y_VAL
            os.write(fifo_fd, struct.pack('<Bf', 0x07, val))
            time.sleep(BASE_INTERVAL)

        else:
            time.sleep(0.01)

def main():
    fifo_fd = setup_fifo()
    try:
        device = InputDevice(DEVICE_PATH)
        print(f"Listening to: {device.name}")
    except Exception as e:
        print(f"Error: {e}"); return

    t = threading.Thread(target=writer_thread, args=(fifo_fd,), daemon=True)
    t.start()

    try:
        for event in device.read_loop():
            if event.type == ecodes.EV_ABS:
                # D-Pad 保持原样
                if event.code == ecodes.ABS_HAT0X:
                    if event.value == -1: os.write(fifo_fd, b'\x01')
                    elif event.value == 1: os.write(fifo_fd, b'\x02')
                elif event.code == ecodes.ABS_HAT0Y:
                    if event.value == -1: os.write(fifo_fd, struct.pack('<Bf', 0x05, -DPAD_Y_VAL))
                    elif event.value == 1: os.write(fifo_fd, struct.pack('<Bf', 0x05, DPAD_Y_VAL))

                # Left Stick X轴 (左右)
                elif event.code == ecodes.ABS_X:
                    # 只有在 Y 轴没有动作时，才允许进入 X 轴逻辑
                    if state["dir_y"] == 0:
                        val = event.value
                        state["is_max_x"] = abs(val) >= (MAX_STICK_VALUE - 100)
                        if val < -16000: state["dir_x"] = 1 # 左
                        elif val > 16000: state["dir_x"] = 2 # 右
                        else: state["dir_x"] = 0
                    else:
                        state["dir_x"] = 0

                # Left Stick Y轴 (上下)
                elif event.code == ecodes.ABS_Y:
                    # 只有在 X 轴没有动作时，才允许进入 Y 轴逻辑
                    if state["dir_x"] == 0:
                        val = event.value
                        if val < -8000: state["dir_y"] = 1 # 上
                        elif val > 8000: state["dir_y"] = 2 # 下
                        else: state["dir_y"] = 0
                    else:
                        state["dir_y"] = 0

                # Fn 键 (左扳机)
                elif event.code == ecodes.ABS_Z:
                    state["fn_pressed"] = event.value > 0

            elif event.type == ecodes.EV_KEY and event.value == 1:
                if event.code == ecodes.BTN_TL:
                    os.write(fifo_fd, struct.pack('<Bf', 0x03, 1.0))
                    state["thumbl_idx"] = 0
                elif event.code == ecodes.BTN_THUMBL:
                    val = THUMBL_VALUES[state["thumbl_idx"]]
                    os.write(fifo_fd, struct.pack('<Bf', 0x03, val))
                    state["thumbl_idx"] = (state["thumbl_idx"] + 1) % len(THUMBL_VALUES)
                elif event.code == ecodes.BTN_SELECT:
                    os.write(fifo_fd, b'\x04')

    except KeyboardInterrupt:
        state["running"] = False
    finally:
        os.close(fifo_fd)

if __name__ == "__main__":
    main()
