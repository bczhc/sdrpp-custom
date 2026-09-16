# sdrpp-custom

A personal fork of [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus).

Most features are built with Claude:deepseek-v4-pro.

## Changes from upstream

- **Baseband noise reduction** — a Log-MMSE denoiser for the baseband (IQ)
  signal, enabled with a "Baseband NR" toggle.
- **Chinese input (IME)** — on Wayland, type Chinese into text fields through
  the system IME, rendered with a merged CJK font.
- **Keyboard controls** — navigate the waterfall, switch modes, and control
  playback from the keyboard.
- **Lock f_c** / **Lock view** — switches that change how scrolling and dragging
  move the center frequency.
- **File source player** — seek bar with live waterfall prefill, plus recording
  time (UTC clock overlay and Shift-hover readout).
- **Radio log** — log the tuned frequency or a bookmark to a text file.
- **Frequency bookmarks** — rename, delete, and log bookmarks from the
  waterfall.
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

## Keyboard controls

| Key | Action |
|---|---|
| `a` / `d` | Shift the spectrum view right / left |
| `w` / `s` | Zoom out / in around the view center (hold `Shift` for fast zoom) |
| `r` / `f` | Lower / raise the FFT floor by 3 dB |
| `c` | Switch the selected VFO to CW |
| `b` | Toggle USB / LSB (falls back to USB) |
| `Shift+C` | Recenter the view on the selected VFO |
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

## File source player

A loaded file source gets a seek bar with a current/total time readout at the
top of the window. Drag it to seek — the waterfall is rebuilt so the new
position lands at the top — and pausing no longer rewinds, so resume continues
from the same spot. The wheel (±1 s, `Shift` ±5 s) and arrow keys fine-tune the
position.

The recording start time is parsed from the filename and shown in the file
source menu. With a valid start time, the waterfall shows a UTC clock overlay in
the top-left corner, and holding `Shift` over the waterfall/FFT shows the
frequency and absolute time (UTC and Beijing) under the cursor.

## Radio log

Press `l` to open a small dialog for logging the tuned frequency. The main box
is pre-filled with the current frequency in kHz (zero-padded to 5 digits), and
a "tailer" holds a timestamp — the recording time for a file source, the wall
clock otherwise. `Ctrl+Enter` appends the line to
`~/Documents/radio-log.txt` and closes; `Esc` cancels.

## Frequency bookmarks

Bookmarks from the frequency manager are drawn as yellow labels above the
waterfall. Left-click one to tune to it; right-click opens the radio log
pre-filled with that bookmark's frequency and name; `Shift`+right-click opens a
rename dialog (with a Delete button).

## SDR++ server on Android (Termux) / Raspberry Pi

There is a separate [`server` branch](https://github.com/bczhc/sdrpp-custom/tree/server)
that builds SDR++ as a headless server binary: no GUI, running inside
[Termux](https://termux.dev) on Android (or on a Raspberry Pi), to serve an
Airspy / AirspyHF to a remote SDR++ client over the network. Build and run
instructions are in that branch's README.
