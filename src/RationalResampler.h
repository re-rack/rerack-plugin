#pragma once

#include <vector>

// Runs a fixed-rate device, mono in and stereo out, from a host at another rate: a rational
// polyphase FIR from the host rate down to the device rate and the same prototype back up.
// The round trip plus the device's own latency is a whole number of host samples.
class RationalResampler
{
public:
    // Allocates. deviceLatencyFrames is the device's latency at its own rate, counted in
    // getLatencySamples(). Equal rates convert nothing.
    void prepare (double hostRate, double deviceRate, int maxHostFrames, int deviceLatencyFrames);
    void reset() noexcept;

    // Host samples from an input sample to its output, the device's latency included.
    int getLatencySamples() const noexcept;
    // The most device frames one down() call returns.
    int getMaxDeviceFrames() const noexcept;

    // n <= maxHostFrames. Returns m, the device frames now in getDeviceIn().
    int down (const float* mono, int n) noexcept;
    const float* getDeviceIn() const noexcept;
    float* getDeviceOut (int channel) noexcept;
    // Takes the m device frames written to getDeviceOut() and writes n host frames.
    void up (int m, float* left, float* right, int n) noexcept;

private:
    // In units of the prototype's rate, the least common multiple of both rates.
    int hostStep = 1;
    int deviceStep = 1;
    bool passThrough = true;

    // Each phase's taps, oldest sample first, so a tap window is one forward dot product.
    std::vector<float> downTable, upTable;
    int downTaps = 0;
    int upTaps = 0;

    std::vector<float> hostHistory;
    std::vector<float> deviceHistoryLeft, deviceHistoryRight;
    std::vector<float> deviceIn, deviceOutLeft, deviceOutRight;

    // The next output's newest input, relative to the current call's first input, and its phase.
    int downNext = 0;
    int downPhase = 0;
    int upNext = 0;
    int upPhase = 0;

    int latency = 0;
    int maxHostFrames = 0;
    int maxDeviceFrames = 0;
};
