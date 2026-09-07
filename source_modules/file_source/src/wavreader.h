#pragma once

#pragma once
#include <stdint.h>
#include <string.h>
#include <fstream>
#include <mutex>

#define WAV_SIGNATURE       "RIFF"
#define WAV_TYPE            "WAVE"
#define WAV_FORMAT_MARK     "fmt "
#define WAV_DATA_MARK       "data"
#define WAV_SAMPLE_TYPE_PCM 1

class WavReader {
public:
    WavReader(std::string path) {
        file = std::ifstream(path.c_str(), std::ios::binary);
        file.read((char*)&hdr, sizeof(WavHeader_t));
        valid = false;
        if (memcmp(hdr.signature, "RIFF", 4) != 0) { return; }
        if (memcmp(hdr.fileType, "WAVE", 4) != 0) { return; }
        valid = true;
    }

    uint16_t getBitDepth() {
        return hdr.bitDepth;
    }

    uint16_t getChannelCount() {
        return hdr.channelCount;
    }

    uint32_t getSampleRate() {
        return hdr.sampleRate;
    }

    bool isValid() {
        return valid;
    }

    void readSamples(void* data, size_t size) {
        std::lock_guard<std::mutex> lck(mtx);
        char* _data = (char*)data;
        file.read(_data, size);
        int read = file.gcount();
        if (read < size) {
            file.clear();
            file.seekg(sizeof(WavHeader_t));
            file.read(&_data[read], size - read);
        }
    }

    void rewind() {
        std::lock_guard<std::mutex> lck(mtx);
        file.clear();
        file.seekg(sizeof(WavHeader_t));
    }

    // Seek to a position expressed in seconds from the start of the file.
    void seek(double seconds) {
        std::lock_guard<std::mutex> lck(mtx);
        double rate = (double)hdr.sampleRate;
        uint64_t frameBytes = (uint64_t)hdr.bytesPerSample;
        if (rate <= 0.0 || frameBytes == 0) { return; }

        // Convert seconds to a whole number of IQ frames (one frame = one
        // complex sample = hdr.bytesPerSample bytes). Seeking to a byte offset
        // that isn't a multiple of frameBytes would swap the I/Q channels.
        double frames = seconds * rate;
        if (frames < 0.0) { frames = 0.0; }
        uint64_t frame = (uint64_t)frames;
        uint64_t maxFrame = (uint64_t)hdr.dataSize / frameBytes;
        if (frame > maxFrame) { frame = maxFrame; }

        file.clear();
        file.seekg(sizeof(WavHeader_t) + (std::streamoff)(frame * frameBytes));
    }

    // Current playback position in seconds.
    double getPosition() {
        std::lock_guard<std::mutex> lck(mtx);
        double rate = (double)hdr.sampleRate;
        double frameBytes = (double)hdr.bytesPerSample;
        if (rate <= 0.0 || frameBytes <= 0.0) { return 0.0; }
        std::streamoff off = file.tellg() - (std::streamoff)sizeof(WavHeader_t);
        if (off < 0) { off = 0; }
        return (double)off / (rate * frameBytes);
    }

    // Total duration of the file in seconds.
    double getDuration() {
        double rate = (double)hdr.sampleRate;
        double frameBytes = (double)hdr.bytesPerSample;
        if (rate <= 0.0 || frameBytes <= 0.0) { return 0.0; }
        return (double)hdr.dataSize / (rate * frameBytes);
    }

    void close() {
        file.close();
    }

private:
    struct WavHeader_t {
        char signature[4];           // "RIFF"
        uint32_t fileSize;           // data bytes + sizeof(WavHeader_t) - 8
        char fileType[4];            // "WAVE"
        char formatMarker[4];        // "fmt "
        uint32_t formatHeaderLength; // Always 16
        uint16_t sampleType;         // PCM (1)
        uint16_t channelCount;
        uint32_t sampleRate;
        uint32_t bytesPerSecond;
        uint16_t bytesPerSample;
        uint16_t bitDepth;
        char dataMarker[4]; // "data"
        uint32_t dataSize;
    };

    bool valid = false;
    std::ifstream file;
    std::mutex mtx;
    WavHeader_t hdr;
};