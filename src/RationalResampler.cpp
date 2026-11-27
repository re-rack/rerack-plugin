#include "RationalResampler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kStopbandDb = 100.0;
constexpr double kPassband = 0.45; // of the lower rate
constexpr double kStopband = 0.5;
constexpr long kMaxPhases = 1024;

double besselI0 (double x)
{
    double sum = 1.0;
    double term = 1.0;

    for (int k = 1; term > 1e-12 * sum; ++k)
    {
        const double factor = x / (2.0 * k);
        term *= factor * factor;
        sum += term;
    }

    return sum;
}

// Kaiser-windowed sinc lowpass of `taps` taps at `rate`, unity DC gain.
std::vector<double> designLowpass (int taps, double cutoff, double rate)
{
    const double beta = 0.1102 * (kStopbandDb - 8.7);
    const double centre = 0.5 * (taps - 1);
    const double fc = 2.0 * cutoff / rate;
    const double norm = besselI0 (beta);

    std::vector<double> h ((std::size_t) taps);
    double sum = 0.0;

    for (int n = 0; n < taps; ++n)
    {
        const double x = n - centre;
        const double sinc = x == 0.0 ? fc : std::sin (kPi * fc * x) / (kPi * x);
        const double r = x / centre;
        const double window = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / norm;
        h[(std::size_t) n] = sinc * window;
        sum += h[(std::size_t) n];
    }

    for (double& tap : h)
        tap /= sum;

    return h;
}

// Polyphase table of `phases` rows: row p holds gain * h[p + t * phases], newest tap last.
std::vector<float> polyphase (const std::vector<double>& h, int phases, int taps, double gain)
{
    std::vector<float> table ((std::size_t) phases * (std::size_t) taps, 0.0f);

    for (int p = 0; p < phases; ++p)
    {
        for (int t = 0; t < taps; ++t)
        {
            const std::size_t n = (std::size_t) p + (std::size_t) t * (std::size_t) phases;
            if (n < h.size())
                table[(std::size_t) p * (std::size_t) taps + (std::size_t) (taps - 1 - t)] =
                    (float) (gain * h[n]);
        }
    }

    return table;
}

float dot (const float* a, const float* b, int n) noexcept
{
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
    int i = 0;

    for (; i + 4 <= n; i += 4)
    {
        s0 += a[i] * b[i];
        s1 += a[i + 1] * b[i + 1];
        s2 += a[i + 2] * b[i + 2];
        s3 += a[i + 3] * b[i + 3];
    }

    for (; i < n; ++i)
        s0 += a[i] * b[i];

    return (s0 + s1) + (s2 + s3);
}

void dot2 (const float* a, const float* left, const float* right, int n, float& outLeft,
           float& outRight) noexcept
{
    float l0 = 0.0f, l1 = 0.0f, r0 = 0.0f, r1 = 0.0f;
    int i = 0;

    for (; i + 2 <= n; i += 2)
    {
        l0 += a[i] * left[i];
        r0 += a[i] * right[i];
        l1 += a[i + 1] * left[i + 1];
        r1 += a[i + 1] * right[i + 1];
    }

    for (; i < n; ++i)
    {
        l0 += a[i] * left[i];
        r0 += a[i] * right[i];
    }

    outLeft = l0 + l1;
    outRight = r0 + r1;
}
} // namespace

void RationalResampler::prepare (double hostRate, double deviceRate, int maxHostFramesToUse,
                                 int deviceLatencyFrames)
{
    long host = std::max (1L, std::lround (hostRate));
    const long device = std::max (1L, std::lround (deviceRate));
    long g = std::gcd (host, device);

    // shortcut: an odd host rate is moved to the nearest 100 Hz, which only shifts the emulated
    // device clock by a fraction of a percent, well within the hardware's own clock tolerance.
    if (host / g > kMaxPhases || device / g > kMaxPhases)
    {
        host = std::max (100L, (host + 50) / 100 * 100);
        g = std::gcd (host, device);
    }

    maxHostFrames = std::max (1, maxHostFramesToUse);
    passThrough = host == device;
    hostStep = (int) (device / g);
    deviceStep = (int) (host / g);

    if (passThrough)
    {
        downTaps = upTaps = 1;
        downTable.assign (1, 1.0f);
        upTable.assign (1, 1.0f);
        maxDeviceFrames = maxHostFrames;
        latency = deviceLatencyFrames;
    }
    else
    {
        const double rate = (double) host * (double) hostStep;
        const double lower = (double) std::min (host, device);
        const double transition = (kStopband - kPassband) * lower;
        const double cutoff = 0.5 * (kPassband + kStopband) * lower;

        long order = (long) std::ceil ((kStopbandDb - 7.95) * rate / (14.36 * transition));

        // The two linear-phase passes delay by `order` prototype samples; with the device's
        // latency that has to land on a host sample for the reported latency to be exact.
        const long deviceDelay = (long) deviceLatencyFrames * deviceStep;
        while ((order + deviceDelay) % hostStep != 0)
            ++order;

        const int taps = (int) order + 1;
        const std::vector<double> h = designLowpass (taps, cutoff, rate);

        downTaps = (taps + hostStep - 1) / hostStep;
        upTaps = (taps + deviceStep - 1) / deviceStep;
        downTable = polyphase (h, hostStep, downTaps, (double) hostStep);
        upTable = polyphase (h, deviceStep, upTaps, (double) deviceStep);

        maxDeviceFrames =
            (int) (((int64_t) maxHostFrames * hostStep + deviceStep - 1) / deviceStep) + 1;
        latency = (int) ((order + deviceDelay) / hostStep);
    }

    hostHistory.assign ((std::size_t) (downTaps - 1 + maxHostFrames), 0.0f);
    deviceHistoryLeft.assign ((std::size_t) (upTaps + maxDeviceFrames), 0.0f);
    deviceHistoryRight.assign ((std::size_t) (upTaps + maxDeviceFrames), 0.0f);
    deviceIn.assign ((std::size_t) maxDeviceFrames, 0.0f);
    deviceOutLeft.assign ((std::size_t) maxDeviceFrames, 0.0f);
    deviceOutRight.assign ((std::size_t) maxDeviceFrames, 0.0f);

    reset();
}

void RationalResampler::reset() noexcept
{
    std::fill (hostHistory.begin(), hostHistory.end(), 0.0f);
    std::fill (deviceHistoryLeft.begin(), deviceHistoryLeft.end(), 0.0f);
    std::fill (deviceHistoryRight.begin(), deviceHistoryRight.end(), 0.0f);
    std::fill (deviceIn.begin(), deviceIn.end(), 0.0f);
    std::fill (deviceOutLeft.begin(), deviceOutLeft.end(), 0.0f);
    std::fill (deviceOutRight.begin(), deviceOutRight.end(), 0.0f);

    downNext = 0;
    downPhase = 0;
    upNext = 0;
    upPhase = 0;
}

int RationalResampler::getLatencySamples() const noexcept
{
    return latency;
}

int RationalResampler::getMaxDeviceFrames() const noexcept
{
    return maxDeviceFrames;
}

int RationalResampler::down (const float* mono, int n) noexcept
{
    if (n <= 0)
        return 0;

    if (passThrough)
    {
        std::copy (mono, mono + n, deviceIn.begin());
        return n;
    }

    float* history = hostHistory.data();
    std::copy (mono, mono + n, history + downTaps - 1);

    int m = 0;
    while (downNext < n)
    {
        deviceIn[(std::size_t) m++] =
            dot (downTable.data() + (std::size_t) downPhase * (std::size_t) downTaps,
                 history + downNext, downTaps);

        downPhase += deviceStep;
        downNext += downPhase / hostStep;
        downPhase %= hostStep;
    }

    downNext -= n;
    std::copy (history + n, history + n + downTaps - 1, history);
    return m;
}

const float* RationalResampler::getDeviceIn() const noexcept
{
    return deviceIn.data();
}

float* RationalResampler::getDeviceOut (int channel) noexcept
{
    return channel == 0 ? deviceOutLeft.data() : deviceOutRight.data();
}

void RationalResampler::up (int m, float* left, float* right, int n) noexcept
{
    if (n <= 0)
        return;

    if (passThrough)
    {
        for (int i = 0; i < n; ++i)
        {
            const bool valid = i < m;
            left[i] = valid ? deviceOutLeft[(std::size_t) i] : 0.0f;
            right[i] = valid ? deviceOutRight[(std::size_t) i] : 0.0f;
        }
        return;
    }

    // A call's first host frame can need the previous call's newest device frame (upNext is
    // then -1), so the history keeps upTaps old frames, one more than a tap window.
    m = std::max (0, m);
    float* historyLeft = deviceHistoryLeft.data();
    float* historyRight = deviceHistoryRight.data();
    std::copy (deviceOutLeft.begin(), deviceOutLeft.begin() + m, historyLeft + upTaps);
    std::copy (deviceOutRight.begin(), deviceOutRight.begin() + m, historyRight + upTaps);

    for (int j = 0; j < n; ++j)
    {
        dot2 (upTable.data() + (std::size_t) upPhase * (std::size_t) upTaps,
              historyLeft + 1 + upNext, historyRight + 1 + upNext, upTaps, left[j], right[j]);

        upPhase += hostStep;
        upNext += upPhase / deviceStep;
        upPhase %= deviceStep;
    }

    upNext -= m;
    std::copy (historyLeft + m, historyLeft + m + upTaps, historyLeft);
    std::copy (historyRight + m, historyRight + m + upTaps, historyRight);
}
