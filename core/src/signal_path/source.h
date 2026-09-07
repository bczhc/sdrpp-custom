#pragma once
#include <string>
#include <vector>
#include <map>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <utils/event.h>

class SourceManager {
public:
    SourceManager();

    struct SourceHandler {
        SourceHandler() {
            stream = NULL;
            menuHandler = NULL;
            selectHandler = NULL;
            deselectHandler = NULL;
            startHandler = NULL;
            stopHandler = NULL;
            tuneHandler = NULL;
            seekHandler = NULL;
            getPositionHandler = NULL;
            getDurationHandler = NULL;
            readSamplesHandler = NULL;
            ctx = NULL;
        }

        dsp::stream<dsp::complex_t>* stream;
        void (*menuHandler)(void* ctx);
        void (*selectHandler)(void* ctx);
        void (*deselectHandler)(void* ctx);
        void (*startHandler)(void* ctx);
        void (*stopHandler)(void* ctx);
        void (*tuneHandler)(double freq, void* ctx);
        void (*seekHandler)(double seconds, void* ctx);
        double (*getPositionHandler)(void* ctx);
        double (*getDurationHandler)(void* ctx);
        int (*readSamplesHandler)(double seconds, dsp::complex_t* out, int count, void* ctx);
        void* ctx;
    };

    enum TuningMode {
        NORMAL,
        PANADAPTER
    };

    void registerSource(std::string name, SourceHandler* handler);
    void unregisterSource(std::string name);
    void selectSource(std::string name);
    void showSelectedMenu();
    void start();
    void stop();
    void tune(double freq);
    void seek(double seconds);
    double getPosition();
    double getDuration();
    int readSamples(double seconds, dsp::complex_t* out, int count);
    void setTuningOffset(double offset);
    void setTuningMode(TuningMode mode);
    void setPanadapterIF(double freq);

    std::vector<std::string> getSourceNames();

    Event<std::string> onSourceRegistered;
    Event<std::string> onSourceUnregister;
    Event<std::string> onSourceUnregistered;
    Event<double> onRetune;

private:
    std::map<std::string, SourceHandler*> sources;
    std::string selectedName;
    SourceHandler* selectedHandler = NULL;
    double tuneOffset;
    double currentFreq;
    double ifFreq = 0.0;
    TuningMode tuneMode = TuningMode::NORMAL;
    dsp::stream<dsp::complex_t> nullSource;
};