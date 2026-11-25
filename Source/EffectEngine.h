#pragma once

#include "EffectCatalog.h"

#include <gm2.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>

class IEngine
{
public:
    virtual ~IEngine() = default;

    virtual void prepare (const gm2_context& context, const int* values, double beatMs) = 0;
    virtual void update (const int* values, double beatMs) = 0;
    virtual void process (const float* inL, const float* inR, float* outL, float* outR, int frames) = 0;
    virtual void align (double beats) = 0;
    virtual bool isValid() const = 0;
};

template <typename M>
class Engine final : public IEngine
{
public:
    explicit Engine (const EffectInfo& effectInfo)
        : info (effectInfo), state (std::make_unique<typename M::State>())
    {
    }

    void prepare (const gm2_context& contextToUse, const int* values, double beatMs) override
    {
        if (preparedRate != contextToUse.sample_rate)
        {
            valid = false;
            preparedRate = contextToUse.sample_rate;
        }

        context = &contextToUse;
        fillRaw (values, beatMs);

        typename M::Config next {};
        if (M::setup (context, &raw, &next) == 0)
        {
            config = next;
            M::init (state.get(), &config);
            valid = true;
        }
        else if (valid)
        {
            M::init (state.get(), &config);
        }

        applySnapshot (values, beatMs);
    }

    void update (const int* values, double beatMs) override
    {
        if (matchesSnapshot (values, beatMs))
            return;

        fillRaw (values, beatMs);

        typename M::Config next {};
        if (M::setup (context, &raw, &next) == 0)
        {
            config = next;
            valid = true;
        }

        applySnapshot (values, beatMs);
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int frames) override
    {
        if (valid && frames > 0)
            M::process (state.get(), &config, inL, inR, outL, outR, (size_t) frames);
    }

    void align (double beats) override
    {
        if (valid)
            M::align (state.get(), &config, beats);
    }

    bool isValid() const override
    {
        return valid;
    }

private:
    void fillRaw (const int* values, double beatMs)
    {
        raw = typename M::Raw {};

        for (int i = 0; i < info.knobCount; ++i)
        {
            auto* target = reinterpret_cast<int*> (reinterpret_cast<char*> (&raw) + info.knobs[i].rawOffset);
            *target = values[i];
        }

        if constexpr (M::takesBeat)
            *reinterpret_cast<double*> (reinterpret_cast<char*> (&raw) + M::beatOffset) = beatMs;
    }

    bool matchesSnapshot (const int* values, double beatMs) const
    {
        if (snapshotCount != info.knobCount
            || ! std::equal (values, values + info.knobCount, snapshot.begin()))
            return false;

        if constexpr (M::takesBeat)
        {
            if (std::isnan (beatMs) && std::isnan (snapshotBeat))
                return true;

            return std::memcmp (&beatMs, &snapshotBeat, sizeof (beatMs)) == 0;
        }

        return true;
    }

    void applySnapshot (const int* values, double beatMs)
    {
        std::copy (values, values + info.knobCount, snapshot.begin());
        snapshotCount = info.knobCount;
        snapshotBeat = beatMs;
    }

    const EffectInfo& info;
    const gm2_context* context = nullptr;
    unsigned preparedRate = 0;
    typename M::Raw raw {};
    typename M::Config config {};
    std::unique_ptr<typename M::State> state;
    std::array<int, 16> snapshot {};
    int snapshotCount = -1;
    double snapshotBeat = std::numeric_limits<double>::quiet_NaN();
    bool valid = false;
};
