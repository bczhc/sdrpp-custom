#define NOMINMAX
#include <imgui.h>
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/tuner.h>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <thread>
#include <atomic>
#include <cstring>
#include <cerrno>

SDRPP_MOD_INFO{
    /* Name:            */ "raw_iq_source",
    /* Description:     */ "Raw f32le IQ pipe source module for SDR++",
    /* Author:          */ "bczhc",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class RawIqSourceModule : public ModuleManager::Instance {
public:
    RawIqSourceModule(std::string name) {
        this->name = name;

        if (core::args["server"].b()) { return; }

        config.acquire();
        strncpy(path, ((std::string)config.conf["path"]).c_str(), sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        sampleRate = config.conf["sampleRate"];
        config.release();

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("Raw IQ", &handler);
    }

    ~RawIqSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("Raw IQ");
    }

    void postInit() {}

    void enable() { enabled = true; }

    void disable() { enabled = false; }

    bool isEnabled() { return enabled; }

private:
    static void menuSelected(void* ctx) {
        RawIqSourceModule* _this = (RawIqSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        // Set the center to 0 Hz directly (baseband IQ). Do NOT go through
        // tuner::tune/IQ_ONLY, which sets centerFreqMoved and persists "0" to the
        // config, retuning other sources to 0 Hz later.
        gui::waterfall.setCenterFrequency(0.0);
        sigpath::iqFrontEnd.setBuffering(false);
        gui::waterfall.centerFrequencyLocked = true;
        flog::info("RawIqSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        RawIqSourceModule* _this = (RawIqSourceModule*)ctx;
        sigpath::iqFrontEnd.setBuffering(true);
        gui::waterfall.centerFrequencyLocked = false;
        flog::info("RawIqSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        RawIqSourceModule* _this = (RawIqSourceModule*)ctx;
        if (_this->running) { return; }
        _this->shouldStop = false;
        _this->running = true;
        _this->workerThread = std::thread(worker, _this);
        flog::info("RawIqSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        RawIqSourceModule* _this = (RawIqSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->shouldStop = true;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();
        _this->shouldStop = false;
        _this->running = false;
        flog::info("RawIqSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        RawIqSourceModule* _this = (RawIqSourceModule*)ctx;
        // The center frequency is pinned to 0 Hz for a baseband IQ pipe; the
        // tuned frequency becomes the VFO's offset from that center. Clear
        // centerFreqMoved so main_window doesn't persist "0" to the config.
        if (freq != 0.0) {
            gui::waterfall.setCenterFrequency(0.0);
            gui::waterfall.centerFreqMoved = false;
            if (!gui::waterfall.selectedVFO.empty()) {
                sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, freq);
            }
        }
    }

    static void menuHandler(void* ctx) {
        RawIqSourceModule* _this = (RawIqSourceModule*)ctx;
        bool changed = false;

        ImGui::SetNextItemWidth(240);
        changed |= ImGui::InputText(("Path##raw_iq_path_" + _this->name).c_str(), _this->path, sizeof(_this->path));

        ImGui::SetNextItemWidth(240);
        changed |= ImGui::InputInt(("Sample Rate##raw_iq_sr_" + _this->name).c_str(), &_this->sampleRate);
        if (_this->sampleRate < 1) { _this->sampleRate = 1; }

        if (changed) {
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["path"] = _this->path;
            config.conf["sampleRate"] = _this->sampleRate;
            config.release(true);
        }
    }

    static void worker(void* ctx) {
        RawIqSourceModule* _this = (RawIqSourceModule*)ctx;

        int blockSize = std::max(std::min(_this->sampleRate / 200, STREAM_BUFFER_SIZE), 1);

        int fd = -1;
        while (!_this->shouldStop) {
            // Open the pipe if not already open. Non-blocking: ENXIO means no
            // writer has connected yet, so retry after a short delay.
            if (fd < 0) {
                fd = open(_this->path, O_RDONLY | O_NONBLOCK);
                if (fd < 0) {
                    if (errno == ENXIO || errno == ENOENT) {
                        usleep(50000);
                        continue;
                    }
                    flog::error("RawIqSourceModule '{0}': failed to open '{1}': {2}", _this->name, _this->path, strerror(errno));
                    break;
                }
            }

            // Wait for data, with a timeout so stop() is still detected.
            struct pollfd pfd = { fd, POLLIN, 0 };
            int pr = poll(&pfd, 1, 100);
            if (pr < 0) {
                if (errno == EINTR) { continue; }
                flog::error("RawIqSourceModule '{0}': poll error: {1}", _this->name, strerror(errno));
                break;
            }
            if (pr == 0) { continue; } // timeout

            // The pipe carries interleaved f32le IQ: [I, Q, I, Q, ...], which is
            // exactly the memory layout of dsp::complex_t.
            ssize_t n = read(fd, _this->stream.writeBuf, blockSize * sizeof(dsp::complex_t));
            if (n < 0) {
                if (errno == EAGAIN || errno == EINTR) { continue; }
                flog::error("RawIqSourceModule '{0}': read error: {1}", _this->name, strerror(errno));
                break;
            }
            if (n == 0) {
                // The writer closed the pipe: close and wait for a new one.
                close(fd);
                fd = -1;
                continue;
            }

            int samples = (int)(n / sizeof(dsp::complex_t));
            if (samples == 0) { continue; }

            if (!_this->stream.swap(samples)) { break; }
        }

        if (fd >= 0) { close(fd); }
        _this->running = false;
    }

    std::string name;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    std::atomic<bool> shouldStop = false;
    bool running = false;
    bool enabled = true;
    int sampleRate = 1000000;
    char path[1024] = {0};
    std::thread workerThread;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["path"] = "";
    def["sampleRate"] = 1000000;
    config.setPath(core::args["root"].s() + "/raw_iq_source_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    return new RawIqSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (RawIqSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
