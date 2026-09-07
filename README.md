# sdrpp-custom

My custom SDR++.

## Building

```shell
export CMAKE_POLICY_VERSION_MINIMUM=3.5

mkdir -p build
cd build

# Or it's also okay just simply do 'cmake .. -G Ninja'.
cmake .. -G Ninja \
-DOPT_BUILD_DISCORD_PRESENCE=OFF \
-DOPT_BUILD_AIRSPY_SOURCE=ON \
-DOPT_BUILD_AIRSPYHF_SOURCE=ON \
-DOPT_BUILD_BLADERF_SOURCE=OFF \
-DOPT_BUILD_FILE_SOURCE=ON \
-DOPT_BUILD_HACKRF_SOURCE=OFF \
-DOPT_BUILD_HERMES_SOURCE=OFF \
-DOPT_BUILD_LIMESDR_SOURCE=OFF \
-DOPT_BUILD_NETWORK_SOURCE=OFF \
-DOPT_BUILD_PERSEUS_SOURCE=OFF \
-DOPT_BUILD_PLUTOSDR_SOURCE=OFF \
-DOPT_BUILD_RFSPACE_SOURCE=OFF \
-DOPT_BUILD_RTL_SDR_SOURCE=OFF \
-DOPT_BUILD_RTL_TCP_SOURCE=OFF \
-DOPT_BUILD_SDRPLAY_SOURCE=OFF \
-DOPT_BUILD_SDRPP_SERVER_SOURCE=ON \
-DOPT_BUILD_SOAPY_SOURCE=OFF \
-DOPT_BUILD_SPECTRAN_SOURCE=OFF \
-DOPT_BUILD_SPECTRAN_HTTP_SOURCE=OFF \
-DOPT_BUILD_SPYSERVER_SOURCE=OFF \
-DOPT_BUILD_USRP_SOURCE=OFF

ninja
# Installing
sudo ninja install
```

## Controlling SDR++ with a gaming controller

SDR++ can be driven remotely through a named pipe (FIFO) at `./command`
(relative to the directory SDR++ is launched from). A background thread opens
this FIFO and reads a small binary protocol, so any external program can
control the waterfall without touching the GUI. `controller.py` translates a
gaming controller's inputs into this protocol, letting you drive SDR++ with a
gamepad.

### Setup

1. Install the Python dependency:

   ```shell
   pip install evdev
   ```

2. Edit the constants at the top of `controller.py` to match your hardware:

   - `FIFO_PATH` — the FIFO location. Must be `./command` relative to the
     directory SDR++ is started from (SDR++ watches `./command`).
   - `DEVICE_PATH` — the input device for your controller. Find it with
     `ls /dev/input/by-id/` or `python -m evdev.evtest`.

3. Start SDR++ (from the directory where the FIFO should live).

4. Run `python controller.py`.

5. Play.

### Controller mapping

Default mapping for a Flydigi Direwolf 4 controller. The `state`/constants at
the top of `controller.py` let you re-map any of this to other buttons or
axes.

| Control | Command | Effect |
|---|---|---|
| D-Pad ← | `0x01` | Shift spectrum view left |
| D-Pad → | `0x02` | Shift spectrum view right |
| D-Pad ↑ | `0x05` + `-3.0` | Decrease FFT min by 3 dB (widen dynamic range) |
| D-Pad ↓ | `0x05` + `+3.0` | Increase FFT min by 3 dB (raise the floor) |
| Left stick ← | `0x02` | Shift spectrum view right |
| Left stick → | `0x01` | Shift spectrum view left |
| Left stick ↑ | `0x07` + `-0.01` | Zoom out |
| Left stick ↓ | `0x07` + `+0.01` | Zoom in |
| Fn (left trigger) + Left stick ← | `0x06` + `-0.01` | Volume down |
| Fn (left trigger) + Left stick → | `0x06` + `+0.01` | Volume up |
| Left bumper (LB) | `0x03` + `1.0` | Zoom to maximum |
| Left stick click (L3) | `0x03` + preset | Cycle zoom presets (`0.132` → `1.0`) |
| Select / Back | `0x04` | Toggle the menu panel |

Notes:

- The left stick's X axis is intentionally mapped in the opposite direction to
  the D-Pad (stick-left sends `0x02`, stick-right sends `0x01`) — it acts as a
  drag/pan control rather than a direction pad.
- Left stick X repeats twice as fast at full deflection.
- The **Lock f_c** button in the UI keeps the center frequency fixed while the
  scale view is being navigated, so shifting the view with the D-Pad / stick
  never retunes the VFO.

### FIFO command protocol

Each message is a single command byte, optionally followed by a 4-byte
little-endian `float` payload. SDR++ reads the FIFO as a byte stream and
reopens it once a second when it is not yet present (or after it is closed).

| Opcode | Payload | Effect |
|---|---|---|
| `0x01` | — | Shift spectrum view left |
| `0x02` | — | Shift spectrum view right |
| `0x03` | `float` (0..1) | Set zoom factor (0 = widest, 1 = max zoom) |
| `0x04` | — | Toggle menu panel |
| `0x05` | `float` | Add delta to FFT min (dB) |
| `0x06` | `float` | Add delta to volume (0..1) |
| `0x07` | `float` | Add delta to zoom factor (0..1) |

A writer should open the FIFO in read-write mode (`os.open(path, os.O_RDWR)`)
so the open doesn't block waiting for a reader; `controller.py` does this
automatically.
