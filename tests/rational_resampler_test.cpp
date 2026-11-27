// The host <-> 39 kHz round trip: its latency is exact and whole, the passband is
// flat and in phase, nothing aliases or images into the other side's band, and the
// result does not depend on how the host cuts its blocks.
#include "RationalResampler.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <random>
#include <vector>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeviceRate = 39000.0;
constexpr int kDeviceLatency = 32;
constexpr int kMaxHostFrames = 512;

int failures = 0;

void check (bool condition, const char* label)
{
    std::printf ("  [%s] %s\n", condition ? "PASS" : "FAIL", label);
    if (! condition)
        ++failures;
}

double toDb (double ratio)
{
    return 20.0 * std::log10 (std::max (ratio, 1e-30));
}

// The stand-in device: what it is fed comes back on both sides kDeviceLatency frames later,
// the right side at half level, unless `source` replaces the input with a generated signal.
struct Device
{
    std::vector<float> line = std::vector<float> (kDeviceLatency, 0.0f);
    int position = 0;
    long frame = 0;
    std::function<float (long)> source;

    void process (const float* in, float* left, float* right, int m)
    {
        for (int i = 0; i < m; ++i, ++frame)
        {
            const float sample = source ? source (frame) : in[i];
            const float out = line[(std::size_t) position];
            line[(std::size_t) position] = sample;
            position = (position + 1) % kDeviceLatency;
            left[i] = out;
            right[i] = 0.5f * out;
        }
    }
};

struct Run
{
    std::vector<float> left, right, device;
    int maxM = 0;
    bool wroteAll = true;
};

Run run (double hostRate, const std::vector<float>& input, const std::vector<int>& blocks,
         std::function<float (long)> source = {})
{
    RationalResampler resampler;
    resampler.prepare (hostRate, kDeviceRate, kMaxHostFrames, kDeviceLatency);

    Device device;
    device.source = std::move (source);

    Run result;
    const float sentinel = std::numeric_limits<float>::quiet_NaN();
    result.left.assign (input.size(), sentinel);
    result.right.assign (input.size(), sentinel);

    const int total = (int) input.size();
    std::size_t blockIndex = 0;

    for (int position = 0; position < total;)
    {
        int n = blocks.empty() ? kMaxHostFrames : blocks[blockIndex++ % blocks.size()];
        n = std::min (n, total - position);

        const int m = resampler.down (input.data() + position, n);
        result.maxM = std::max (result.maxM, m);
        result.device.insert (result.device.end(), resampler.getDeviceIn(),
                              resampler.getDeviceIn() + m);
        device.process (resampler.getDeviceIn(), resampler.getDeviceOut (0),
                        resampler.getDeviceOut (1), m);
        resampler.up (m, result.left.data() + position, result.right.data() + position, n);
        position += n;
    }

    for (std::size_t i = 0; i < input.size(); ++i)
        result.wroteAll = result.wroteAll && ! std::isnan (result.left[i]) && ! std::isnan (result.right[i]);

    return result;
}

std::vector<float> sine (double rate, double frequency, int frames, double amplitude = 0.5)
{
    std::vector<float> signal ((std::size_t) frames);
    for (int i = 0; i < frames; ++i)
        signal[(std::size_t) i] = (float) (amplitude * std::sin (2.0 * kPi * frequency * i / rate));
    return signal;
}

double rms (const std::vector<float>& signal, std::size_t start, std::size_t end)
{
    double sum = 0.0;
    for (std::size_t i = start; i < end; ++i)
        sum += (double) signal[i] * (double) signal[i];
    return std::sqrt (sum / (double) (end - start));
}

// Amplitude of the component at `frequency`, through a Hann window.
double amplitudeAt (const std::vector<float>& signal, std::size_t start, std::size_t end,
                    double frequency, double rate)
{
    double re = 0.0, im = 0.0, weights = 0.0;
    const double length = (double) (end - start);

    for (std::size_t i = start; i < end; ++i)
    {
        const double n = (double) (i - start);
        const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * n / length);
        const double phase = 2.0 * kPi * frequency * (double) i / rate;
        re += w * signal[i] * std::cos (phase);
        im -= w * signal[i] * std::sin (phase);
        weights += w;
    }

    return 2.0 * std::sqrt (re * re + im * im) / weights;
}

std::vector<int> randomBlocks (unsigned seed)
{
    std::mt19937 rng (seed);
    std::uniform_int_distribution<int> distribution (1, kMaxHostFrames);
    std::vector<int> blocks (4096);
    for (int& n : blocks)
        n = distribution (rng);
    return blocks;
}

void checkRate (double hostRate)
{
    char label[160];
    std::printf ("host %.0f Hz\n", hostRate);

    RationalResampler probe;
    probe.prepare (hostRate, kDeviceRate, kMaxHostFrames, kDeviceLatency);
    const int latency = probe.getLatencySamples();
    const double lower = std::min (hostRate, kDeviceRate);
    const int frames = (int) hostRate;
    const auto settled = (std::size_t) (2 * latency + 2048);
    std::printf ("  latency %d samples\n", latency);

    {
        std::vector<float> impulse ((std::size_t) (4 * latency + 4096), 0.0f);
        const int at = 1000;
        impulse[(std::size_t) at] = 1.0f;
        const Run out = run (hostRate, impulse, {});

        const auto peak = std::max_element (out.left.begin(), out.left.end(),
                                            [] (float a, float b) { return std::abs (a) < std::abs (b); });
        const int peakAt = (int) (peak - out.left.begin());

        double asymmetry = 0.0;
        for (int d = 1; d < latency / 2; ++d)
            asymmetry = std::max (asymmetry, (double) std::abs (out.left[(std::size_t) (peakAt - d)]
                                                                - out.left[(std::size_t) (peakAt + d)]));

        std::snprintf (label, sizeof (label), "an impulse peaks at the reported latency (%d)", peakAt - at);
        check (peakAt - at == latency, label);
        std::snprintf (label, sizeof (label), "the impulse response is symmetric (%.1f dB)", toDb (asymmetry / *peak));
        check (asymmetry / *peak < 1e-4, label);
    }

    for (const double frequency : { 100.0, 1000.0, 10000.0, 0.44 * lower })
    {
        if (frequency > 0.44 * lower)
            continue;

        const std::vector<float> input = sine (hostRate, frequency, frames);
        const Run out = run (hostRate, input, {});

        std::vector<float> residual (input.size(), 0.0f);
        std::vector<float> reference (input.size(), 0.0f);
        for (std::size_t i = settled; i < input.size(); ++i)
        {
            reference[i] = input[i - (std::size_t) latency];
            residual[i] = out.left[i] - reference[i];
        }

        const double gainDb = toDb (rms (out.left, settled, input.size()) / rms (reference, settled, input.size()));
        const double errorDb = toDb (rms (residual, settled, input.size()) / rms (reference, settled, input.size()));
        std::snprintf (label, sizeof (label), "%.0f Hz: gain %+.5f dB, residual %.1f dB", frequency, gainDb, errorDb);
        check (std::abs (gainDb) <= 0.01 && errorDb <= -80.0, label);
    }

    if (hostRate > kDeviceRate)
    {
        double worst = 0.0;
        for (double frequency = 0.505 * kDeviceRate; frequency < 0.5 * hostRate; frequency += 0.04 * hostRate)
        {
            const Run out = run (hostRate, sine (hostRate, frequency, frames), {});
            worst = std::max (worst, rms (out.device, out.device.size() / 4, out.device.size()) / (0.5 / std::sqrt (2.0)));
        }

        std::snprintf (label, sizeof (label), "tones above 19.5 kHz do not alias into the device (%.1f dB)", toDb (worst));
        check (worst <= std::pow (10.0, -100.0 / 20.0), label);
    }
    else if (hostRate < kDeviceRate)
    {
        const double frequency = 0.4 * hostRate;
        const Run out = run (hostRate, sine (hostRate, frequency, frames), {});
        const std::size_t start = out.device.size() / 4;
        const double image = std::max (amplitudeAt (out.device, start, out.device.size(), hostRate - frequency, kDeviceRate),
                                       amplitudeAt (out.device, start, out.device.size(), hostRate + frequency, kDeviceRate));
        std::snprintf (label, sizeof (label), "upsampling to the device leaves no images (%.1f dB)", toDb (image / 0.5));
        check (image / 0.5 <= std::pow (10.0, -100.0 / 20.0), label);
    }

    if (hostRate != kDeviceRate)
    {
        const std::vector<float> silence ((std::size_t) frames, 0.0f);
        const double tone = hostRate > kDeviceRate ? 0.4 * kDeviceRate : 0.505 * hostRate + 0.3 * (0.5 * kDeviceRate - 0.505 * hostRate);
        const auto source = [tone] (long frame) { return (float) (0.5 * std::sin (2.0 * kPi * tone * (double) frame / kDeviceRate)); };
        const Run out = run (hostRate, silence, {}, source);

        double worst = 0.0;
        if (hostRate > kDeviceRate)
        {
            for (int r = 1; r * kDeviceRate - tone < 0.5 * hostRate; ++r)
                for (const double image : { r * kDeviceRate - tone, r * kDeviceRate + tone })
                    if (image < 0.5 * hostRate)
                        worst = std::max (worst, amplitudeAt (out.left, settled, out.left.size(), image, hostRate));
        }
        else
        {
            worst = rms (out.left, settled, out.left.size()) * std::sqrt (2.0);
        }

        std::snprintf (label, sizeof (label), "a %.0f Hz device tone images or aliases %.1f dB on the host side", tone, toDb (worst / 0.5));
        check (worst / 0.5 <= std::pow (10.0, -100.0 / 20.0), label);
    }

    {
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
        std::vector<float> input ((std::size_t) frames);
        for (float& sample : input)
            sample = noise (rng);

        const Run fixed = run (hostRate, input, {});
        const Run varied = run (hostRate, input, randomBlocks (11));
        check (fixed.left == varied.left && fixed.right == varied.right, "random block sizes give the same output");
        check (fixed.wroteAll && varied.wroteAll, "up() writes every host frame");
        check (std::max (fixed.maxM, varied.maxM) <= probe.getMaxDeviceFrames(), "down() stays within getMaxDeviceFrames()");

        const double expected = (double) frames * kDeviceRate / hostRate;
        check (std::abs ((double) varied.device.size() - expected) <= 2.0, "device frames follow the rate ratio");
    }
}
} // namespace

int main()
{
    for (const double rate : { 22050.0, 32000.0, 39000.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, 47999.0 })
        checkRate (rate);

    {
        std::printf ("pass-through\n");
        std::vector<float> input (4096);
        for (std::size_t i = 0; i < input.size(); ++i)
            input[i] = (float) i;

        const Run out = run (kDeviceRate, input, randomBlocks (3));
        bool delayed = true;
        for (std::size_t i = 0; i < input.size(); ++i)
            delayed = delayed && out.left[i] == (i < (std::size_t) kDeviceLatency ? 0.0f : input[i - kDeviceLatency]);
        check (delayed, "39 kHz passes through, only the device's latency later");
    }

    std::printf ("%s\n", failures == 0 ? "rational_resampler_test: PASS" : "rational_resampler_test: FAIL");
    return failures == 0 ? 0 : 1;
}
