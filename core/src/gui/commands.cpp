//
// Created by bczhc on 08/03/26.
//

#include "commands.h"

std::atomic<int> cmd_spectrum_shift(0);
std::atomic<bool> cmd_panel_toggle(false);
std::atomic<float> cmd_fft_min_change(0.0);
std::atomic<float> cmd_zoom_factor_delta(0.0);
