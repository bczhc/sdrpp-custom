#pragma once
#include <vector>

namespace radiolog {
    extern const char* LOG_FILE;

    // Parses every log line matching "^([\d\.]+)\s+.*\)$" and returns the first
    // capture group (the frequency in kHz) converted to Hz.
    std::vector<double> getLoggedFrequenciesHz();

    void show();
    bool isOpen();
    void openPopup(const char* name = nullptr, double freqHz = -1.0);
}
