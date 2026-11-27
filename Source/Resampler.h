#pragma once

#include <vector>

// Runs a fixed-rate effect at hostRate / factor: equiripple linear-phase
// half-band FIR stages down, the effect, the same stages back up. Stereo.
class Resampler
{
public:
    // factor is 1 or a power of two, baseRate = hostRate / factor. Allocates.
    void prepare (int factor, double baseRate, int maxHostFrames);
    void reset();

    int getFactor() const noexcept;
    // Host-rate samples by which down() + up() delay the signal; 0 at factor 1.
    int getLatencySamples() const noexcept;

    // n <= maxHostFrames. Returns the number of base-rate frames now in getBase().
    int down (const float* left, const float* right, int n) noexcept;
    float* getBase (int channel) noexcept;
    // Takes the m base frames from getBase(), processed in place, and writes n host frames.
    void up (int m, float* left, float* right, int n) noexcept;

private:
    struct Stage
    {
        std::vector<float> h;
        int taps = 0;
        int delay = 0;
        std::vector<float> decLeft, decRight;
        std::vector<float> upLeft, upRight;
        int decPos = 0;
        int decPhase = 0;
        int upPos = 0;
    };

    int decimateStage (Stage& stage, const float* left, const float* right, int n,
                       float* outLeft, float* outRight) noexcept;
    void interpolateStage (Stage& stage, const float* left, const float* right, int n,
                           float* outLeft, float* outRight) noexcept;

    std::vector<Stage> stages;
    std::vector<std::vector<float>> stageLeft, stageRight;
    std::vector<float> baseLeft, baseRight;
    std::vector<float> fifoLeft, fifoRight;
    int factor_ = 1;
    int latency_ = 0;
    int maxHostFrames_ = 0;
    int fifoRead_ = 0;
    int fifoWrite_ = 0;
    int fifoCount_ = 0;
};
