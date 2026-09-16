#include <gui/dialogs/radio_log.h>
#include <imgui.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <utils/flog.h>

#include <string>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <signal_path/signal_path.h>

namespace radiolog {
    const char* LOG_FILE = "/home/bczhc/Documents/radio-log.txt";

    bool open = false;
    bool justOpened = false;

    bool isOpen() { return open; }

    char freqBuf[64] = {0};
    char textBuf[2048] = {0};
    char tailerBuf[128] = {0};

    // Formats the tuned frequency (Hz) as a zero-padded 5-digit kHz value,
    // e.g. 7,074,500 Hz -> "07074.5", 7,074,000 Hz -> "07074", 14,270,000 Hz -> "14270".
    void formatFrequency(uint64_t freqHz, char* out, size_t outSize) {
        // Round to the nearest 100 Hz (0.1 kHz) using integer math.
        uint64_t kHz10 = (freqHz + 50) / 100; // kHz * 10
        uint64_t intKHz = kHz10 / 10;
        uint64_t frac = kHz10 % 10;

        if (frac != 0) {
            snprintf(out, outSize, "%05llu.%llu", (unsigned long long)intKHz, (unsigned long long)frac);
        }
        else {
            snprintf(out, outSize, "%05llu", (unsigned long long)intKHz);
        }
    }

    // Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm).
    static long long daysFromCivil(int y, int m, int d) {
        y -= m <= 2;
        int era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (long long)doe - 719468;
    }

    static const char* MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    // Formats an epoch (UTC seconds) as "dMonyy HHMM", e.g. "7Sep26 0522".
    void formatEpochTime(std::time_t epoch, char* out, size_t outSize) {
        std::tm tmv = {};
        gmtime_r(&epoch, &tmv);
        snprintf(out, outSize, "%d%s%02d %02d%02d",
                 tmv.tm_mday, MONTHS[tmv.tm_mon], tmv.tm_year % 100,
                 tmv.tm_hour, tmv.tm_min);
    }

    // Formats the current UTC time as "dMonyy HHMM".
    void formatUtcTime(char* out, size_t outSize) {
        formatEpochTime(std::time(nullptr), out, outSize);
    }

    // Parses "YYYY-MM-DD HH:MM:SS" into UTC epoch seconds. Returns false on invalid input.
    bool parseStartTimeEpoch(const char* str, long long& epoch) {
        int y, mo, d, h, mi, s;
        if (sscanf(str, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) { return false; }
        if (mo < 1 || mo > 12 || d < 1 || d > 31) { return false; }
        if (h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) { return false; }
        epoch = daysFromCivil(y, mo, d) * 86400LL + h * 3600 + mi * 60 + s;
        return true;
    }

    std::string trim(const std::string& s) {
        size_t begin = s.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) { return ""; }
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(begin, end - begin + 1);
    }

    void appendLog(const std::string& line) {
        std::filesystem::path path(LOG_FILE);
        std::error_code ec;
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path(), ec);
        }

        std::ofstream file(path, std::ios::app);
        if (!file.is_open()) {
            flog::error("Radio log: failed to open log file for appending");
            return;
        }
        file << line << "\n";
    }

    void openPopup(const char* name, double freqHz) {
        uint64_t freq = (freqHz >= 0.0) ? (uint64_t)freqHz : gui::freqSelect.frequency;
        formatFrequency(freq, freqBuf, sizeof(freqBuf));

        // Pre-fill the main text with the frequency. Opening from the 'l' key
        // leaves a trailing space to keep typing; opening from a right-click on a
        // frequency marker appends the marker's name instead.
        if (name && name[0]) {
            snprintf(textBuf, sizeof(textBuf), "%s %s", freqBuf, name);
        }
        else {
            snprintf(textBuf, sizeof(textBuf), "%s ", freqBuf);
        }

        // The tailer timestamp: for the file source use the recording time
        // (Start Time + elapsed), otherwise the current wall-clock time.
        const char* startTime = sigpath::sourceManager.getStartTime();
        if (startTime != NULL) {
            long long startEpoch;
            if (parseStartTimeEpoch(startTime, startEpoch)) {
                long long pointEpoch = startEpoch + (long long)std::llround(sigpath::sourceManager.getPosition());
                char tsBuf[32] = {0};
                formatEpochTime((std::time_t)pointEpoch, tsBuf, sizeof(tsBuf));
                snprintf(tailerBuf, sizeof(tailerBuf), "(%s) (zc)", tsBuf);
            }
            else {
                tailerBuf[0] = '\0'; // invalid Start Time -> empty tailer
            }
        }
        else {
            char tsBuf[32] = {0};
            formatUtcTime(tsBuf, sizeof(tsBuf));
            snprintf(tailerBuf, sizeof(tailerBuf), "(%s) (zc)", tsBuf);
        }

        open = true;
        justOpened = true;
    }

    void submit() {
        // The main box is multiline, but the log line must stay a single line:
        // turn any line breaks into spaces before writing.
        std::string mainStr = textBuf;
        std::replace(mainStr.begin(), mainStr.end(), '\n', ' ');
        mainStr = trim(mainStr);

        std::string tailerStr = trim(tailerBuf);
        if (mainStr.empty()) { return; }

        appendLog(mainStr + " " + tailerStr);
    }

    int mainTextCallback(ImGuiInputTextCallbackData* data) {
        // On the first frame, place the cursor at the end of the pre-filled text
        // (ImGui selects all text when focusing via SetKeyboardFocusHere).
        if (data->EventFlag & ImGuiInputTextFlags_CallbackAlways) {
            if (justOpened) {
                data->CursorPos = data->BufTextLen;
                data->SelectionStart = data->BufTextLen;
                data->SelectionEnd = data->BufTextLen;
                justOpened = false;
            }
        }
        return 0;
    }

    void show() {
        // Open on 'l' when no other text field is being edited.
        if (!open && ImGui::IsKeyPressed(ImGuiKey_L, false) && !ImGui::GetIO().WantTextInput) {
            openPopup();
        }

        if (!open) { return; }

        ImVec2 dispSize = ImGui::GetIO().DisplaySize;
        ImVec2 center(dispSize.x / 2.0f, dispSize.y / 2.0f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Radio Log");
        if (ImGui::BeginPopupModal("Radio Log", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Frequency / text");

            const float boxWidth = 320.0f * style::uiScale;

            // Auto-grow the main box with its content (capped so an overly long note scrolls).
            int lineCount = 1;
            for (const char* p = textBuf; *p; ++p) {
                if (*p == '\n') { lineCount++; }
            }
            lineCount = std::min(lineCount, 10);
            float boxHeight = (ImGui::GetTextLineHeight() * lineCount) + (ImGui::GetStyle().FramePadding.y * 2.0f);

            ImGui::PushID("##radiolog_main");
            if (justOpened) { ImGui::SetKeyboardFocusHere(); }
            ImGui::InputTextMultiline("##text", textBuf, sizeof(textBuf), ImVec2(boxWidth, boxHeight),
                                      ImGuiInputTextFlags_CallbackAlways, mainTextCallback);
            ImGui::PopID();

            ImGui::TextUnformatted("Tailer");
            ImGui::SetNextItemWidth(boxWidth);
            ImGui::InputText("##tailer", tailerBuf, sizeof(tailerBuf));

            ImGui::Spacing();
            ImGui::TextUnformatted("Enter: new line    Ctrl+Enter: save & close    Esc: cancel");

            bool ctrlEnter = ImGui::GetIO().KeyCtrl &&
                             (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
            bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false);

            if (ctrlEnter) {
                submit();
                open = false;
                ImGui::CloseCurrentPopup();
            }
            else if (cancel) {
                open = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}
