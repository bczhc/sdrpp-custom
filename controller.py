import os
import time
import threading
import struct
from evdev import InputDevice, ecodes

# --- 常量配置区 ---
FIFO_PATH = './command'
DEVICE_PATH = '/dev/input/by-id/usb-Flydigi_Flydigi_Direwolf_4_Flydigi_Direwolf_4-event-joystick'

THUMBL_VALUES = [0.017517, 1.0]
DPAD_Y_VAL = 3.0          
STICK_Y_VAL = 0.01        
STICK_X_FN_VAL = 0.1      

BASE_INTERVAL = 0.05      
MAX_STICK_VALUE = 32767   
# ----------------

state = {
    "dir_x": 0,        # 0:停, 1:左, 2:右
    "dir_y": 0,        # 0:停, 1:上, 2:下
    "is_max_x": False, # 仅 X 轴触发倍速
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
        
        # 1. 处理 X 轴 (左右) - 优先级最高
        if state["dir_x"] != 0:
            active = True
            # 倍速逻辑：仅在 X 轴推到底时 interval 减半
            current_interval = BASE_INTERVAL / 2 if state["is_max_x"] else BASE_INTERVAL
            
            if state["fn_pressed"]:
                # Fn 按下：左推负，右推正 (0x06)
                val = -STICK_X_FN_VAL if state["dir_x"] == 1 else STICK_X_FN_VAL
                os.write(fifo_fd, struct.pack('<Bf', 0x06, val))
            else:
                # Fn 未按下：左推 0x02, 右推 0x01 (注意这里已按要求反转)
                os.write(fifo_fd, b'\x02' if state["dir_x"] == 1 else b'\x01')
            
            time.sleep(current_interval)
        
        # 2. 处理 Y 轴 (上下) - 仅在 X 轴静止时工作，固定倍速
        elif state["dir_y"] != 0:
            active = True
            val = -STICK_Y_VAL if state["dir_y"] == 1 else STICK_Y_VAL
            os.write(fifo_fd, struct.pack('<Bf', 0x07, val))
            time.sleep(BASE_INTERVAL) # Y 轴固定 1 倍速

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

                # Left Stick X轴 (左右) - 包含反向逻辑与倍速判定
                elif event.code == ecodes.ABS_X:
                    val = event.value
                    state["is_max_x"] = abs(val) >= (MAX_STICK_VALUE - 100)
                    if val < -8000: 
                        state["dir_x"] = 1 # 左推状态
                        state["dir_y"] = 0 
                    elif val > 8000: 
                        state["dir_x"] = 2 # 右推状态
                        state["dir_y"] = 0 
                    else: 
                        state["dir_x"] = 0

                # Left Stick Y轴 (上下)
                elif event.code == ecodes.ABS_Y:
                    if state["dir_x"] == 0:
                        if event.value < -8000: state["dir_y"] = 1
                        elif event.value > 8000: state["dir_y"] = 2
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
