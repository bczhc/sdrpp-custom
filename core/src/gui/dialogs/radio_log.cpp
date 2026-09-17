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
#include <cctype>
#include <vector>
#include <signal_path/signal_path.h>

namespace radiolog {
    const char* LOG_FILE = "/home/bczhc/Documents/radio-log.txt";

    bool open = false;
    bool justOpened = false;

    bool historyOpen = false;
    bool historyJustOpened = false;
    char historySearchBuf[256] = {0};
    std::vector<std::string> historyLines;
    int historySelected = 0;

    bool isOpen() { return open || historyOpen; }

    char freqBuf[64] = {0};
    char textBuf[2048] = {0};
    char tailerBuf[128] = {0};

    // Formats the tuned frequency (Hz) as a zero-padded 5-digit kHz value,
    // stripping trailing zeros: 7,074,500 Hz -> "07074.5", 7,074,000 Hz -> "07074",
    // 1,234,512 Hz -> "01234.512". Frequency is integer Hz (1 Hz resolution).
    void formatFrequency(uint64_t freqHz, char* out, size_t outSize) {
        uint64_t intKHz = freqHz / 1000;
        uint64_t fracHz = freqHz % 1000; // 0..999 (0.001 kHz)

        if (fracHz == 0) {
            snprintf(out, outSize, "%05llu", (unsigned long long)intKHz);
        }
        else {
            char frac[4];
            snprintf(frac, sizeof(frac), "%03llu", (unsigned long long)fracHz);
            int len = 3;
            while (len > 0 && frac[len - 1] == '0') { len--; }
            frac[len] = '\0';
            snprintf(out, outSize, "%05llu.%s", (unsigned long long)intKHz, frac);
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

    bool containsIgnoreCase(const std::string& hay, const std::string& needle) {
        if (needle.empty()) { return true; }
        auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(),
                              [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
        return it != hay.end();
    }

    // Strips the trailing " (<ts>) (zc)" from a log line, returning the main body.
    std::string extractMainBody(const std::string& line) {
        std::string s = line;
        if (s.size() >= 5 && s.compare(s.size() - 5, 5, " (zc)") == 0) {
            s.erase(s.size() - 5);
        }
        size_t lp = s.rfind('(');
        if (lp != std::string::npos) {
            s.erase(lp);
        }
        return trim(s);
    }

    // Regenerate the tailer timestamp (recording time for a file source, the
    // wall clock otherwise).
    void updateTailer() {
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

        updateTailer();

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

    // Loads all log lines recorded at the current frequency into historyLines.
    void openHistory() {
        historyLines.clear();
        historySelected = 0;
        historySearchBuf[0] = '\0';

        char curFreq[64];
        formatFrequency(gui::freqSelect.frequency, curFreq, sizeof(curFreq));
        std::string curFreqStr(curFreq); // full kHz string

        std::ifstream file(LOG_FILE);
        if (!file.is_open()) {
            flog::error("Radio log: failed to open log file for reading");
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') { line.pop_back(); }
            // Log entry format: "<freq> <note> (<ts>) (zc)", ending with ')'.
            if (line.empty() || line.back() != ')') { continue; }
            if (line.size() <= curFreqStr.size()) { continue; }
            if (line.compare(0, curFreqStr.size(), curFreqStr) != 0) { continue; }
            if (line[curFreqStr.size()] != ' ') { continue; }

            historyLines.push_back(line);
        }

        historyOpen = true;
        historyJustOpened = true;
    }

    void showHistory() {
        if (!historyOpen) { return; }

        ImVec2 dispSize = ImGui::GetIO().DisplaySize;
        ImVec2 center(dispSize.x / 2.0f, dispSize.y / 2.0f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Radio Log History");
        if (ImGui::BeginPopupModal("Radio Log History", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            const float boxWidth = 440.0f * style::uiScale;
            const float listHeight = 300.0f * style::uiScale;

            if (historyJustOpened) {
                ImGui::SetKeyboardFocusHere();
                historyJustOpened = false;
            }
            ImGui::SetNextItemWidth(boxWidth);
            ImGui::InputText("##history_search", historySearchBuf, sizeof(historySearchBuf));

            std::string needle(historySearchBuf);

            std::vector<int> filtered;
            for (size_t i = 0; i < historyLines.size(); i++) {
                if (containsIgnoreCase(historyLines[i], needle)) {
                    filtered.push_back((int)i);
                }
            }

            if (historySelected < 0) { historySelected = 0; }
            if (filtered.empty()) { historySelected = 0; }
            else if (historySelected >= (int)filtered.size()) { historySelected = (int)filtered.size() - 1; }

            bool up = ImGui::IsKeyPressed(ImGuiKey_UpArrow, false);
            bool down = ImGui::IsKeyPressed(ImGuiKey_DownArrow, false);
            if (up && historySelected > 0) { historySelected--; }
            if (down && !filtered.empty() && historySelected < (int)filtered.size() - 1) { historySelected++; }

            ImGui::BeginChild("##history_list", ImVec2(boxWidth, listHeight), true);
            for (size_t fi = 0; fi < filtered.size(); fi++) {
                int lineIdx = filtered[fi];
                bool selected = ((int)fi == historySelected);
                if (ImGui::Selectable(historyLines[lineIdx].c_str(), selected)) {
                    historySelected = (int)fi;
                }
                if (selected) {
                    ImGui::SetScrollHereY();
                }
            }
            ImGui::EndChild();

            bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
            bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false);

            if (enter && !filtered.empty()) {
                std::string mainBody = extractMainBody(historyLines[filtered[historySelected]]);
                snprintf(textBuf, sizeof(textBuf), "%s", mainBody.c_str());
                updateTailer();
                open = true;
                justOpened = true;
                historyOpen = false;
                ImGui::CloseCurrentPopup();
            }
            else if (cancel) {
                historyOpen = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void show() {
        // 'l' opens the log dialog; Shift+'l' opens the history list.
        if (!open && !historyOpen && ImGui::IsKeyPressed(ImGuiKey_L, false) && !ImGui::GetIO().WantTextInput) {
            if (ImGui::GetIO().KeyShift) {
                openHistory();
            }
            else {
                openPopup();
            }
        }

        if (historyOpen) {
            showHistory();
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
