//
// Created by bczhc on 08/03/26.
//

#ifndef SDRPP_COMMANDS_H
#define SDRPP_COMMANDS_H

#include <atomic>

extern std::atomic<int> cmd_spectrum_shift;
extern std::atomic<float> cmd_zoom_factor;
extern std::atomic<bool> cmd_panel_toggle;
extern std::atomic<float> cmd_fft_min_change;
extern std::atomic<float> cmd_volume_delta;
extern std::atomic<float> cmd_zoom_factor_delta;

void setupCommandInputReader();

#endif // SDRPP_COMMANDS_H
