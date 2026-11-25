#include "Resampler.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;

double besselI0 (double x)
{
    const double y = 0.25 * x * x;
    double term = 1.0;
    double sum = 1.0;

    for (int k = 1; k < 200; ++k)
    {
        const double kd = static_cast<double> (k);
        term *= y / (kd * kd);
        sum += term;
        if (term <= sum * 1.0e-16)
            break;
    }

    return sum;
}
} // namespace

void Resampler::prepare (int factor, double baseRate, int maxHostFrames)
{
    if (factor < 1)
        factor = 1;

    int stageCount = 0;
    for (int f = factor; f > 1; f >>= 1)
        ++stageCount;

    factor_ = factor;
    maxHostFrames_ = std::max (1, maxHostFrames);

    const std::size_t stageSize = static_cast<std::size_t> (stageCount);
    stages.assign (stageSize, Stage {});
    stageLeft.assign (stageSize, std::vector<float> {});
    stageRight.assign (stageSize, std::vector<float> {});

    const double fp = std::min (20000.0, 0.4535 * baseRate);
    constexpr double stopbandDb = 100.0;
    const double beta = 0.1102 * (stopbandDb - 8.7);
    const double i0Beta = besselI0 (beta);

    latency_ = 0;

    for (int s = 0; s < stageCount; ++s)
    {
        const double inputRate = baseRate * static_cast<double> (1 << (stageCount - s));
        const double fstop = 0.5 * inputRate - fp;
        const double length = (stopbandDb - 8.0)
                              / (2.285 * 2.0 * kPi * ((fstop - fp) / inputRate))
                              + 1.0;

        int taps = static_cast<int> (std::ceil (length));
        if ((taps & 1) == 0)
            ++taps;

        const int delay = (taps - 1) / 2;
        Stage& stage = stages[static_cast<std::size_t> (s)];
        stage.taps = taps;
        stage.delay = delay;

        std::vector<double> coeff (static_cast<std::size_t> (taps));
        double sum = 0.0;

        for (int n = 0; n < taps; ++n)
        {
            const double x = static_cast<double> (n - delay);
            const double sinc = x == 0.0 ? 1.0
                                         : std::sin (0.5 * kPi * x) / (0.5 * kPi * x);
            const double ratio = delay == 0 ? 0.0 : x / static_cast<double> (delay);
            const double window = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - ratio * ratio)))
                                  / i0Beta;
            coeff[static_cast<std::size_t> (n)] = sinc * window;
            sum += sinc * window;
        }

        stage.h.resize (static_cast<std::size_t> (taps));
        for (int n = 0; n < taps; ++n)
            stage.h[static_cast<std::size_t> (n)] =
                static_cast<float> (coeff[static_cast<std::size_t> (n)] / sum);

        stage.decLeft.assign (static_cast<std::size_t> (taps), 0.0f);
        stage.decRight.assign (static_cast<std::size_t> (taps), 0.0f);
        stage.upLeft.assign (static_cast<std::size_t> (delay + 1), 0.0f);
        stage.upRight.assign (static_cast<std::size_t> (delay + 1), 0.0f);

        stageLeft[static_cast<std::size_t> (s)].assign (
            static_cast<std::size_t> ((maxHostFrames_ >> s) + factor_ + 2), 0.0f);
        stageRight[static_cast<std::size_t> (s)].assign (
            static_cast<std::size_t> ((maxHostFrames_ >> s) + factor_ + 2), 0.0f);

        latency_ += (1 << (s + 1)) * delay;
    }

    const int maxBaseFrames = maxHostFrames_ / factor_ + 1;
    baseLeft.assign (static_cast<std::size_t> (maxBaseFrames), 0.0f);
    baseRight.assign (static_cast<std::size_t> (maxBaseFrames), 0.0f);

    const int fifoCapacity = factor_ - 1 + factor_ * maxBaseFrames;
    fifoLeft.assign (static_cast<std::size_t> (fifoCapacity), 0.0f);
    fifoRight.assign (static_cast<std::size_t> (fifoCapacity), 0.0f);

    reset();
}

void Resampler::reset()
{
    for (Stage& stage : stages)
    {
        std::fill (stage.decLeft.begin(), stage.decLeft.end(), 0.0f);
        std::fill (stage.decRight.begin(), stage.decRight.end(), 0.0f);
        std::fill (stage.upLeft.begin(), stage.upLeft.end(), 0.0f);
        std::fill (stage.upRight.begin(), stage.upRight.end(), 0.0f);
        stage.decPos = 0;
        stage.decPhase = 0;
        stage.upPos = 0;
    }

    std::fill (baseLeft.begin(), baseLeft.end(), 0.0f);
    std::fill (baseRight.begin(), baseRight.end(), 0.0f);
    std::fill (fifoLeft.begin(), fifoLeft.end(), 0.0f);
    std::fill (fifoRight.begin(), fifoRight.end(), 0.0f);

    fifoRead_ = 0;
    fifoWrite_ = factor_ - 1;
    fifoCount_ = factor_ - 1;
}

int Resampler::getFactor() const noexcept
{
    return factor_;
}

int Resampler::getLatencySamples() const noexcept
{
    return latency_;
}

int Resampler::decimateStage (Stage& stage, const float* left, const float* right, int n,
                              float* outLeft, float* outRight) noexcept
{
    int outCount = 0;

    for (int k = 0; k < n; ++k)
    {
        stage.decLeft[static_cast<std::size_t> (stage.decPos)] = left[k];
        stage.decRight[static_cast<std::size_t> (stage.decPos)] = right[k];

        if (stage.decPhase != 0)
        {
            double sumLeft = 0.0;
            double sumRight = 0.0;
            int index = stage.decPos;

            for (int i = 0; i < stage.taps; ++i)
            {
                const float tap = stage.h[static_cast<std::size_t> (i)];
                sumLeft += static_cast<double> (tap)
                           * static_cast<double> (stage.decLeft[static_cast<std::size_t> (index)]);
                sumRight += static_cast<double> (tap)
                            * static_cast<double> (stage.decRight[static_cast<std::size_t> (index)]);

                if (--index < 0)
                    index = stage.taps - 1;
            }

            outLeft[outCount] = static_cast<float> (sumLeft);
            outRight[outCount] = static_cast<float> (sumRight);
            ++outCount;
        }

        stage.decPhase ^= 1;
        if (++stage.decPos == stage.taps)
            stage.decPos = 0;
    }

    return outCount;
}

void Resampler::interpolateStage (Stage& stage, const float* left, const float* right, int n,
                                  float* outLeft, float* outRight) noexcept
{
    const int historySize = stage.delay + 1;
    int outCount = 0;

    for (int k = 0; k < n; ++k)
    {
        stage.upLeft[static_cast<std::size_t> (stage.upPos)] = left[k];
        stage.upRight[static_cast<std::size_t> (stage.upPos)] = right[k];

        double evenLeft = 0.0;
        double evenRight = 0.0;
        double oddLeft = 0.0;
        double oddRight = 0.0;
        int index = stage.upPos;

        for (int j = 0; j <= stage.delay; ++j)
        {
            const float zl = stage.upLeft[static_cast<std::size_t> (index)];
            const float zr = stage.upRight[static_cast<std::size_t> (index)];
            const double evenTap = static_cast<double> (stage.h[static_cast<std::size_t> (2 * j)]);
            evenLeft += evenTap * static_cast<double> (zl);
            evenRight += evenTap * static_cast<double> (zr);

            if (j < stage.delay)
            {
                const double oddTap =
                    static_cast<double> (stage.h[static_cast<std::size_t> (2 * j + 1)]);
                oddLeft += oddTap * static_cast<double> (zl);
                oddRight += oddTap * static_cast<double> (zr);
            }

            if (--index < 0)
                index = historySize - 1;
        }

        outLeft[outCount] = static_cast<float> (2.0 * evenLeft);
        outRight[outCount] = static_cast<float> (2.0 * evenRight);
        ++outCount;
        outLeft[outCount] = static_cast<float> (2.0 * oddLeft);
        outRight[outCount] = static_cast<float> (2.0 * oddRight);
        ++outCount;

        if (++stage.upPos == historySize)
            stage.upPos = 0;
    }
}

int Resampler::down (const float* left, const float* right, int n) noexcept
{
    if (n <= 0)
        return 0;

    if (stages.empty())
    {
        const int count = std::min (n, static_cast<int> (baseLeft.size()));
        for (int i = 0; i < count; ++i)
        {
            baseLeft[static_cast<std::size_t> (i)] = left[i];
            baseRight[static_cast<std::size_t> (i)] = right[i];
        }
        return count;
    }

    const float* inLeft = left;
    const float* inRight = right;
    int count = n;

    for (std::size_t s = 0; s < stages.size(); ++s)
    {
        float* outLeft = s + 1 == stages.size() ? baseLeft.data() : stageLeft[s].data();
        float* outRight = s + 1 == stages.size() ? baseRight.data() : stageRight[s].data();

        count = decimateStage (stages[s], inLeft, inRight, count, outLeft, outRight);
        inLeft = outLeft;
        inRight = outRight;
    }

    return count;
}

float* Resampler::getBase (int channel) noexcept
{
    return channel == 0 ? baseLeft.data() : baseRight.data();
}

void Resampler::up (int m, float* left, float* right, int n) noexcept
{
    if (n <= 0)
        return;

    if (stages.empty())
    {
        for (int i = 0; i < n; ++i)
        {
            const bool valid = i < m;
            left[i] = valid ? baseLeft[static_cast<std::size_t> (i)] : 0.0f;
            right[i] = valid ? baseRight[static_cast<std::size_t> (i)] : 0.0f;
        }
        return;
    }

    if (m < 0)
        m = 0;

    const float* inLeft = baseLeft.data();
    const float* inRight = baseRight.data();
    int count = m;

    for (int s = static_cast<int> (stages.size()) - 1; s >= 0; --s)
    {
        float* outLeft = stageLeft[static_cast<std::size_t> (s)].data();
        float* outRight = stageRight[static_cast<std::size_t> (s)].data();

        interpolateStage (stages[static_cast<std::size_t> (s)], inLeft, inRight, count,
                          outLeft, outRight);
        count *= 2;
        inLeft = outLeft;
        inRight = outRight;
    }

    const float* finalLeft = stageLeft[0].data();
    const float* finalRight = stageRight[0].data();

    for (int i = 0; i < count; ++i)
    {
        fifoLeft[static_cast<std::size_t> (fifoWrite_)] = finalLeft[i];
        fifoRight[static_cast<std::size_t> (fifoWrite_)] = finalRight[i];

        if (++fifoWrite_ == static_cast<int> (fifoLeft.size()))
            fifoWrite_ = 0;
        ++fifoCount_;
    }

    for (int i = 0; i < n; ++i)
    {
        left[i] = fifoLeft[static_cast<std::size_t> (fifoRead_)];
        right[i] = fifoRight[static_cast<std::size_t> (fifoRead_)];

        if (++fifoRead_ == static_cast<int> (fifoLeft.size()))
            fifoRead_ = 0;
        --fifoCount_;
    }
}
