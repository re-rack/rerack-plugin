#include "Resampler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kStopbandDb = 100.0;
constexpr int kMaxHalfbandOrder = 256;

struct Halfband
{
    std::vector<double> taps;
    double ripple = 0.0;
};

// Equiripple halfband lowpass of 4 * order - 1 taps, flat up to `passband` (a
// fraction of its input rate, below 0.25). H(z) = (z^-(2 order - 1) + G(z^2)) / 2,
// so the Remez exchange designs the 2 * order-tap G against 1 over G's passband,
// and H's passband and stopband ripple are both half of G's error.
Halfband designHalfband (int order, double passband)
{
    const int gridSize = 32 * order + 1;
    const double edge = 4.0 * kPi * passband;

    // G's amplitude is cos (w / 2) P (cos w) with P a polynomial of degree order - 1,
    // so its error against 1 is weight * (desired - P).
    std::vector<double> x ((std::size_t) gridSize), weight ((std::size_t) gridSize),
        desired ((std::size_t) gridSize), error ((std::size_t) gridSize);

    for (int g = 0; g < gridSize; ++g)
    {
        const double w = edge * g / (gridSize - 1);
        x[(std::size_t) g] = std::cos (w);
        weight[(std::size_t) g] = std::cos (0.5 * w);
        desired[(std::size_t) g] = 1.0 / weight[(std::size_t) g];
    }

    std::vector<int> extremal ((std::size_t) order + 1);
    for (int i = 0; i <= order; ++i)
        extremal[(std::size_t) i] = (int) std::lround ((double) i * (gridSize - 1) / order);

    std::vector<double> nodes ((std::size_t) order), values ((std::size_t) order),
        bary ((std::size_t) order);

    const auto polynomial = [&] (double at) {
        double numerator = 0.0;
        double denominator = 0.0;

        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            if (at == nodes[i])
                return values[i];

            const double term = bary[i] / (at - nodes[i]);
            numerator += term * values[i];
            denominator += term;
        }

        return numerator / denominator;
    };

    double maxError = 0.0;

    for (int iteration = 0; iteration < 100; ++iteration)
    {
        double numerator = 0.0;
        double denominator = 0.0;

        for (int i = 0; i <= order; ++i)
        {
            const auto gi = (std::size_t) extremal[(std::size_t) i];
            double a = 1.0;
            for (int j = 0; j <= order; ++j)
                if (j != i)
                    a /= x[gi] - x[(std::size_t) extremal[(std::size_t) j]];

            numerator += a * desired[gi];
            denominator += ((i & 1) != 0 ? -a : a) / weight[gi];
        }

        const double delta = numerator / denominator;

        for (int i = 0; i < order; ++i)
        {
            const auto gi = (std::size_t) extremal[(std::size_t) i];
            nodes[(std::size_t) i] = x[gi];
            values[(std::size_t) i] = desired[gi] - ((i & 1) != 0 ? -delta : delta) / weight[gi];

            double b = 1.0;
            for (int j = 0; j < order; ++j)
                if (j != i)
                    b /= x[gi] - x[(std::size_t) extremal[(std::size_t) j]];

            bary[(std::size_t) i] = b;
        }

        maxError = 0.0;
        for (std::size_t g = 0; g < error.size(); ++g)
        {
            error[g] = weight[g] * (desired[g] - polynomial (x[g]));
            maxError = std::max (maxError, std::abs (error[g]));
        }

        if (maxError - std::abs (delta) <= 1.0e-9 * maxError)
            break;

        std::vector<int> next;
        for (int g = 0; g < gridSize; ++g)
        {
            const double e = error[(std::size_t) g];
            const bool aboveLeft = g == 0 || (e > 0.0 ? e >= error[(std::size_t) g - 1]
                                                      : e <= error[(std::size_t) g - 1]);
            const bool aboveRight = g == gridSize - 1 || (e > 0.0 ? e > error[(std::size_t) g + 1]
                                                                  : e < error[(std::size_t) g + 1]);
            if (! aboveLeft || ! aboveRight)
                continue;

            if (! next.empty() && (e > 0.0) == (error[(std::size_t) next.back()] > 0.0))
            {
                if (std::abs (e) > std::abs (error[(std::size_t) next.back()]))
                    next.back() = g;
            }
            else
            {
                next.push_back (g);
            }
        }

        while ((int) next.size() > order + 1)
        {
            if (std::abs (error[(std::size_t) next.front()]) < std::abs (error[(std::size_t) next.back()]))
                next.erase (next.begin());
            else
                next.pop_back();
        }

        if ((int) next.size() < order + 1)
            break;

        extremal = next;
    }

    // P's Chebyshev coefficients, from its values at the Chebyshev nodes.
    std::vector<double> chebyshev ((std::size_t) order + 1, 0.0);
    for (int m = 0; m < order; ++m)
    {
        double sum = 0.0;
        for (int j = 0; j < order; ++j)
        {
            const double angle = kPi * (j + 0.5) / order;
            sum += polynomial (std::cos (angle)) * std::cos (m * angle);
        }

        chebyshev[(std::size_t) m] = (m == 0 ? 1.0 : 2.0) * sum / order;
    }

    // cos (w / 2) P (cos w) = sum of b[n] cos ((n + 1/2) w), the amplitude of a G
    // with taps g[order - 1 - n] = g[order + n] = b[n] / 2.
    Halfband result;
    result.taps.assign ((std::size_t) (4 * order - 1), 0.0);
    result.taps[(std::size_t) (2 * order - 1)] = 0.5;
    result.ripple = 0.5 * maxError;

    for (int n = 0; n < order; ++n)
    {
        const double b = n == 0 ? chebyshev[0] + 0.5 * chebyshev[1]
                                : 0.5 * (chebyshev[(std::size_t) n] + chebyshev[(std::size_t) n + 1]);
        result.taps[(std::size_t) (2 * (order - 1 - n))] = 0.25 * b;
        result.taps[(std::size_t) (2 * (order + n))] = 0.25 * b;
    }

    return result;
}

// The shortest equiripple halfband that meets kStopbandDb.
std::vector<double> shortestHalfband (double passband)
{
    const double maxRipple = std::pow (10.0, -kStopbandDb / 20.0);
    Halfband design;

    for (int order = 1; order <= kMaxHalfbandOrder; ++order)
    {
        design = designHalfband (order, passband);
        if (design.ripple <= maxRipple)
            break;
    }

    return design.taps;
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

    latency_ = 0;

    for (int s = 0; s < stageCount; ++s)
    {
        const double inputRate = baseRate * static_cast<double> (1 << (stageCount - s));
        const std::vector<double> coeff = shortestHalfband (fp / inputRate);
        const int taps = static_cast<int> (coeff.size());
        const int delay = (taps - 1) / 2;
        Stage& stage = stages[static_cast<std::size_t> (s)];
        stage.taps = taps;
        stage.delay = delay;

        double sum = 0.0;
        for (const double c : coeff)
            sum += c;

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
