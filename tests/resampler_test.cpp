#include "Resampler.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace
{
constexpr double kPi = 3.14159265358979323846;

int failures = 0;

void check (bool condition, const char* label)
{
    std::printf ("  [%s] %s\n", condition ? "PASS" : "FAIL", label);
    if (! condition)
        ++failures;
}

struct Buffers
{
    std::vector<float> left;
    std::vector<float> right;
};

void run (Resampler& resampler, const std::vector<float>& inLeft,
          const std::vector<float>& inRight, std::vector<float>& outLeft,
          std::vector<float>& outRight, const std::vector<int>& blocks, int maxHostFrames)
{
    const int total = static_cast<int> (inLeft.size());
    outLeft.assign (inLeft.size(), 0.0f);
    outRight.assign (inRight.size(), 0.0f);

    int position = 0;
    std::size_t blockIndex = 0;

    while (position < total)
    {
        int n = maxHostFrames;
        if (! blocks.empty())
        {
            n = blocks[blockIndex % blocks.size()];
            ++blockIndex;
        }

        n = std::min (n, total - position);

        const int m = resampler.down (inLeft.data() + position, inRight.data() + position, n);
        resampler.up (m, outLeft.data() + position, outRight.data() + position, n);
        position += n;
    }
}

std::vector<int> makeRandomBlocks (int total, int maxHostFrames, unsigned int seed)
{
    std::mt19937 rng (seed);
    std::uniform_int_distribution<int> distribution (1, maxHostFrames);

    std::vector<int> blocks;
    int sum = 0;
    while (sum < total)
    {
        const int n = distribution (rng);
        blocks.push_back (n);
        sum += n;
    }

    return blocks;
}

double rms (const std::vector<float>& signal, int start, int end)
{
    double sum = 0.0;
    for (int i = start; i < end; ++i)
    {
        const double value = static_cast<double> (signal[static_cast<std::size_t> (i)]);
        sum += value * value;
    }

    return std::sqrt (sum / static_cast<double> (end - start));
}

void makeSineMix (int sampleRate, int frames, Buffers& buffers)
{
    buffers.left.resize (static_cast<std::size_t> (frames));
    buffers.right.resize (static_cast<std::size_t> (frames));

    for (int i = 0; i < frames; ++i)
    {
        const double t = static_cast<double> (i) / static_cast<double> (sampleRate);
        buffers.left[static_cast<std::size_t> (i)] =
            static_cast<float> (0.2 * std::sin (2.0 * kPi * 100.0 * t)
                                + 0.2 * std::sin (2.0 * kPi * 1000.0 * t + 0.4)
                                + 0.2 * std::sin (2.0 * kPi * 15000.0 * t + 1.2));
        buffers.right[static_cast<std::size_t> (i)] =
            static_cast<float> (0.2 * std::sin (2.0 * kPi * 100.0 * t + 0.9)
                                + 0.2 * std::sin (2.0 * kPi * 1000.0 * t + 2.1)
                                + 0.2 * std::sin (2.0 * kPi * 15000.0 * t + 0.2));
    }
}

void makeSine (int sampleRate, int frames, double frequency, double amplitude,
               Buffers& buffers)
{
    buffers.left.resize (static_cast<std::size_t> (frames));
    buffers.right.resize (static_cast<std::size_t> (frames));

    for (int i = 0; i < frames; ++i)
    {
        const double t = static_cast<double> (i) / static_cast<double> (sampleRate);
        buffers.left[static_cast<std::size_t> (i)] =
            static_cast<float> (amplitude * std::sin (2.0 * kPi * frequency * t));
        buffers.right[static_cast<std::size_t> (i)] =
            static_cast<float> (amplitude * std::sin (2.0 * kPi * frequency * t + 0.7));
    }
}

void testLatency (int hostRate, int factor, int maxHostFrames)
{
    const int frames = 32768;
    Buffers input;
    makeSineMix (hostRate, frames, input);

    Resampler resampler;
    resampler.prepare (factor, static_cast<double> (hostRate) / static_cast<double> (factor),
                       maxHostFrames);
    const int latency = resampler.getLatencySamples();

    std::vector<float> outLeft;
    std::vector<float> outRight;
    run (resampler, input.left, input.right, outLeft, outRight, std::vector<int> {},
         maxHostFrames);

    const int start = std::max (8192, latency + 4096);
    double error = 0.0;
    double signal = 0.0;
    double wrongError = 0.0;

    for (int i = start; i < frames; ++i)
    {
        const double actualLeft = static_cast<double> (outLeft[static_cast<std::size_t> (i)]);
        const double actualRight = static_cast<double> (outRight[static_cast<std::size_t> (i)]);
        const double expectedLeft =
            static_cast<double> (input.left[static_cast<std::size_t> (i - latency)]);
        const double expectedRight =
            static_cast<double> (input.right[static_cast<std::size_t> (i - latency)]);
        const double wrongLeft =
            static_cast<double> (input.left[static_cast<std::size_t> (i - latency - 1)]);
        const double wrongRight =
            static_cast<double> (input.right[static_cast<std::size_t> (i - latency - 1)]);

        const double dl = actualLeft - expectedLeft;
        const double dr = actualRight - expectedRight;
        error += dl * dl + dr * dr;
        signal += expectedLeft * expectedLeft + expectedRight * expectedRight;

        const double wl = actualLeft - wrongLeft;
        const double wr = actualRight - wrongRight;
        wrongError += wl * wl + wr * wr;
    }

    const double errorDb = 10.0 * std::log10 (error / signal);
    const double wrongDb = 10.0 * std::log10 (wrongError / signal);

    std::printf ("  latency %d, error %.2f dB, off-by-one %.2f dB\n", latency, errorDb,
                 wrongDb);
    check (errorDb < -80.0, "delayed output matches within -80 dB");
    check (wrongDb > -80.0, "off-by-one delay fails the same check");
}

void testPassband (int hostRate, int factor, int maxHostFrames)
{
    const int frames = 32768;
    const double amplitude = 0.5;
    Buffers input;
    makeSine (hostRate, frames, 18000.0, amplitude, input);

    Resampler resampler;
    resampler.prepare (factor, static_cast<double> (hostRate) / static_cast<double> (factor),
                       maxHostFrames);
    const int latency = resampler.getLatencySamples();

    std::vector<float> outLeft;
    std::vector<float> outRight;
    run (resampler, input.left, input.right, outLeft, outRight, std::vector<int> {},
         maxHostFrames);

    const int start = std::max (8192, latency + 4096);
    const double inputRms = rms (input.left, start - latency, frames - latency);
    const double outputRms = rms (outLeft, start, frames);
    const double gainDb = 20.0 * std::log10 (outputRms / inputRms);

    std::printf ("  18 kHz gain %.3f dB\n", gainDb);
    check (std::fabs (gainDb) <= 0.05, "18 kHz passband gain within 0.05 dB");
}

void testAlias (int maxHostFrames)
{
    const int hostRate = 96000;
    const int factor = 2;
    const int frames = 32768;
    const double amplitude = 0.5;

    Buffers input;
    makeSine (hostRate, frames, 30000.0, amplitude, input);

    Resampler resampler;
    resampler.prepare (factor, static_cast<double> (hostRate) / static_cast<double> (factor),
                       maxHostFrames);
    const int latency = resampler.getLatencySamples();

    std::vector<float> outLeft;
    std::vector<float> outRight;
    run (resampler, input.left, input.right, outLeft, outRight, std::vector<int> {},
         maxHostFrames);

    const int start = std::max (8192, latency + 4096);
    double real = 0.0;
    double imaginary = 0.0;
    int count = 0;

    for (int i = start; i < frames; ++i)
    {
        const double phase = 2.0 * kPi * 18000.0 * static_cast<double> (i)
                             / static_cast<double> (hostRate);
        const double value = static_cast<double> (outLeft[static_cast<std::size_t> (i)]);
        real += value * std::cos (phase);
        imaginary += value * std::sin (phase);
        ++count;
    }

    const double aliasAmplitude = 2.0 * std::sqrt (real * real + imaginary * imaginary)
                                  / static_cast<double> (count);
    const double rejectionDb = 20.0 * std::log10 (aliasAmplitude / amplitude);

    std::printf ("  30 kHz alias rejection %.2f dB\n", rejectionDb);
    check (rejectionDb <= -90.0, "30 kHz alias rejected by at least 90 dB");
}

void testBlockIndependence (int hostRate, int factor, int maxHostFrames)
{
    const int frames = 8192;
    Buffers input;
    input.left.resize (static_cast<std::size_t> (frames));
    input.right.resize (static_cast<std::size_t> (frames));

    std::mt19937 rng (0x5A17u + static_cast<unsigned int> (hostRate));
    std::uniform_real_distribution<double> distribution (-0.5, 0.5);

    for (int i = 0; i < frames; ++i)
    {
        input.left[static_cast<std::size_t> (i)] =
            static_cast<float> (distribution (rng));
        input.right[static_cast<std::size_t> (i)] =
            static_cast<float> (distribution (rng));
    }

    const double baseRate = static_cast<double> (hostRate) / static_cast<double> (factor);
    Resampler fixed;
    Resampler random;
    fixed.prepare (factor, baseRate, maxHostFrames);
    random.prepare (factor, baseRate, maxHostFrames);

    std::vector<float> fixedLeft;
    std::vector<float> fixedRight;
    std::vector<float> randomLeft;
    std::vector<float> randomRight;

    run (fixed, input.left, input.right, fixedLeft, fixedRight, std::vector<int> {},
         maxHostFrames);
    const std::vector<int> blocks =
        makeRandomBlocks (frames, maxHostFrames, 0xB10C0u + static_cast<unsigned int> (hostRate));
    run (random, input.left, input.right, randomLeft, randomRight, blocks, maxHostFrames);

    const bool identical =
        std::memcmp (fixedLeft.data(), randomLeft.data(), fixedLeft.size() * sizeof (float)) == 0
        && std::memcmp (fixedRight.data(), randomRight.data(),
                        fixedRight.size() * sizeof (float)) == 0;

    check (identical, "random block sizes are bit-identical to fixed blocks");
}

void testFactorOne (int maxHostFrames)
{
    const int frames = 4096;
    Buffers input;
    input.left.resize (static_cast<std::size_t> (frames));
    input.right.resize (static_cast<std::size_t> (frames));

    std::mt19937 rng (0xF1u);
    std::uniform_real_distribution<double> distribution (-0.5, 0.5);

    for (int i = 0; i < frames; ++i)
    {
        input.left[static_cast<std::size_t> (i)] =
            static_cast<float> (distribution (rng));
        input.right[static_cast<std::size_t> (i)] =
            static_cast<float> (distribution (rng));
    }

    Resampler resampler;
    resampler.prepare (1, 44100.0, maxHostFrames);
    check (resampler.getFactor() == 1, "factor 1 reports factor 1");
    check (resampler.getLatencySamples() == 0, "factor 1 reports zero latency");

    std::vector<float> outLeft;
    std::vector<float> outRight;
    run (resampler, input.left, input.right, outLeft, outRight, std::vector<int> {},
         maxHostFrames);

    check (std::memcmp (input.left.data(), outLeft.data(), input.left.size() * sizeof (float)) == 0,
           "factor 1 left channel is bit-exact");
    check (std::memcmp (input.right.data(), outRight.data(),
                        input.right.size() * sizeof (float)) == 0,
           "factor 1 right channel is bit-exact");

    Resampler inPlace;
    inPlace.prepare (1, 44100.0, maxHostFrames);
    std::vector<float> workLeft = input.left;
    std::vector<float> workRight = input.right;

    int position = 0;
    while (position < frames)
    {
        const int n = std::min (maxHostFrames, frames - position);
        const int m = inPlace.down (workLeft.data() + position, workRight.data() + position, n);
        inPlace.up (m, workLeft.data() + position, workRight.data() + position, n);
        position += n;
    }

    check (std::memcmp (input.left.data(), workLeft.data(),
                        input.left.size() * sizeof (float)) == 0,
           "factor 1 in-place left channel is bit-exact");
    check (std::memcmp (input.right.data(), workRight.data(),
                        input.right.size() * sizeof (float)) == 0,
           "factor 1 in-place right channel is bit-exact");
}
} // namespace

int main()
{
    const int maxHostFrames = 512;

    std::printf ("Factor 1\n");
    testFactorOne (maxHostFrames);

    struct RateCase
    {
        int hostRate;
        int factor;
    };

    const RateCase cases[] = {
        { 88200, 2 },
        { 96000, 2 },
        { 176400, 4 },
        { 192000, 4 },
        { 384000, 8 },
    };

    for (const RateCase& rateCase : cases)
    {
        std::printf ("\nHost %d Hz, factor %d\n", rateCase.hostRate, rateCase.factor);
        testLatency (rateCase.hostRate, rateCase.factor, maxHostFrames);
        testPassband (rateCase.hostRate, rateCase.factor, maxHostFrames);

        if (rateCase.hostRate == 96000)
            testAlias (maxHostFrames);

        testBlockIndependence (rateCase.hostRate, rateCase.factor, maxHostFrames);
    }

    if (failures != 0)
    {
        std::printf ("\n%d check(s) FAILED\n", failures);
        return 1;
    }

    std::printf ("\nAll checks passed\n");
    return 0;
}
