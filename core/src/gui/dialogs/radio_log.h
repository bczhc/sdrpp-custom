#pragma once

namespace radiolog {
    void show();
    bool isOpen();
    void openPopup(const char* name = nullptr, double freqHz = -1.0);
}
