# sdrpp-custom

A personal fork of [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus).

## Changes from upstream

- **Baseband noise reduction** — a Log-MMSE denoiser for the baseband (IQ)
  signal, enabled with a "Baseband NR" toggle.
- **Gamepad control** — drive the waterfall with a gaming controller via
  `controller.py`.
- **Lock f_c** — keep the center frequency fixed while navigating the spectrum.
- **Lock view** — pin the view in place and move the center frequency instead.
- **Extra UI scales** — added 133% and 150% to the UI scale menu.

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

## Baseband noise reduction

The `noise_reduction_logmmse` module adds a **Baseband NR** toggle that
denoises the IQ signal before demodulation. The noise profile is re-learned
each time you retune, and the filter turns itself off if the CPU can't keep up
in real time.

Enable the module in the module manager, then flip the "Baseband NR" switch.

## Controlling SDR++ with a gamepad

`controller.py` reads a gaming controller and turns it into the commands that
drive SDR++.

1. `pip install evdev`
2. Edit the two paths at the top of `controller.py` for your machine (the
   controller device and where the FIFO should live).
3. Start SDR++ from the directory where the FIFO should appear.
4. Run `python controller.py`.

Default mapping (Flydigi Direwolf 4; edit the script to remap):

| Control | Action |
|---|---|
| D-Pad ← / → | Shift the spectrum view left / right |
| D-Pad ↑ / ↓ | Adjust the FFT floor |
| Left stick ← / → | Pan the spectrum view |
| Left stick ↑ / ↓ | Zoom out / in |
| Fn (left trigger) + left stick ← / → | Volume down / up |
| Left bumper | Zoom to maximum |
| Left stick click | Cycle zoom presets |
| Select / Back | Toggle the menu |

## Lock f_c and Lock view

Both switches sit in the waterfall controls, next to the zoom slider.

- **Lock f_c** — when on, scrolling or dragging the frequency scale never
  retunes the center frequency, even at the edge of the spectrum. The view just
  stops there.
- **Lock view** — when on, scrolling or dragging retunes the center frequency
  directly instead of panning the view. The VFO stays fixed on screen while the
  spectrum scrolls behind it.
