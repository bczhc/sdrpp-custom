# sdrpp-custom

A personal fork of [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus).

## Changes from upstream

- **Baseband noise reduction** — a Log-MMSE denoiser for the baseband (IQ)
  signal, enabled with a "Baseband NR" toggle.
- **Chinese input (IME)** — on Wayland, type Chinese into text fields through
  the system IME, rendered with a merged CJK font.
- **Gamepad control** — drive the waterfall with a gaming controller via
  `controller.py`.
- **Keyboard controls** — navigate the waterfall and control playback from the
  keyboard.
- **Lock f_c** — keep the center frequency fixed while navigating the spectrum.
- **Lock view** — pin the view in place and move the center frequency instead.
- **File source seeking** — seek through a loaded file with a slider and time
  readout.
- **Radio log** — press `l` to log the tuned frequency to a text file.
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

## Chinese input (IME)

On Wayland you can type Chinese into text fields through the system IME
(fcitx5 / ibus). SDR++ builds a vendored GLFW fork that implements the Wayland
text-input protocol and toggles the IME while a text field is focused. Chinese
characters are rendered by merging Source Han Sans CN into the UI font.

The CJK font is loaded from
`/usr/share/fonts/adobe-source-han-sans/SourceHanSansCN-Regular.otf`; if it is
missing, text still goes in but shows as `?`.

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

## Keyboard controls

| Key | Action |
|---|---|
| `a` / `d` | Shift the spectrum view right / left |
| `w` / `s` | Zoom out / in (hold `Shift` for fast zoom) |
| `r` / `f` | Lower / raise the FFT floor by 3 dB |
| `x` | Toggle between the current zoom and the minimum zoom |
| `Space` | Play / pause |
| `Esc` | Toggle the menu panel |

`x` remembers the current zoom level and zooms out to the minimum; pressing it
again restores the remembered zoom. These shortcuts are ignored while a text
field is focused, so they don't interfere with typing.

Built-in keybindings (from upstream SDR++):

| Context | Key | Action |
|---|---|---|
| Over the waterfall / FFT | `←` / `→` | Tune the selected VFO by one snap interval |
| Over the waterfall / FFT | scroll | Tune the VFO |
| Over the waterfall / FFT | `Shift` + scroll | Coarse tuning (×10) |
| Over the waterfall / FFT | `Alt` + scroll | Fine tuning (×0.1) |
| Over the waterfall / FFT | `PageUp` / `PageDown` | Cycle through VFOs |
| Over the frequency scale | `←` / `→` | Pan the spectrum view |
| Frequency box (hover a digit) | `↑` / `↓` | Increment / decrement the digit |
| Frequency box (hover a digit) | `←` / `Backspace` / `→` | Move the cursor |
| Frequency box (hover a digit) | `0`–`9` | Type digits |
| Frequency box (hover a digit) | `Delete` / `Enter` | Zero the remaining digits |
| Frequency box | `Ctrl+C` / `Ctrl+Insert` | Copy frequency (`Cmd+C` on macOS) |
| Frequency box | `Ctrl+V` / `Shift+Insert` | Paste frequency |

## Lock f_c and Lock view

Both switches sit in the waterfall controls, next to the zoom slider.

- **Lock f_c** — when on, scrolling or dragging the frequency scale never
  retunes the center frequency, even at the edge of the spectrum. The view just
  stops there.
- **Lock view** — when on, scrolling or dragging retunes the center frequency
  directly instead of panning the view. The VFO stays fixed on screen while the
  spectrum scrolls behind it.

## File source seeking

A loaded file source gets a seek bar with a current/total time readout at the
top of the window. Drag to seek — the waterfall is rebuilt so the new position
lands at the top — and pausing no longer rewinds, so resume continues from the
same spot.

## Radio log

Press `l` to open a small dialog for logging the tuned frequency. The main box
is pre-filled with the current frequency in kHz (zero-padded to 5 digits), and
a "tailer" holds a UTC timestamp. `Ctrl+Enter` appends the line to
`~/Documents/radio-log.txt` and closes; `Esc` cancels.

## SDR++ server on Android (Termux) / Raspberry Pi

There is a separate [`server` branch](https://github.com/bczhc/sdrpp-custom/tree/server)
that builds SDR++ as a headless server binary: no GUI, running inside
[Termux](https://termux.dev) on Android (or on a Raspberry Pi), to serve an
Airspy / AirspyHF to a remote SDR++ client over the network. Build and run
instructions are in that branch's README.
