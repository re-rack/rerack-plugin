// The De-pop switch's engines: exactly the effects the library has a variant of
// get one, and each plays its own path while a turn settles and gives the effect's
// output, sample for sample, once it has.
#include "EffectCatalog.h"
#include "EffectEngine.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
constexpr unsigned kRate = 48000;
constexpr int kBlock = 256;
constexpr int kLead = kRate;           // before the turn
constexpr int kSettle = 4 * kRate;     // the variant's hand-back is over within about 2.3 s
constexpr int kCompare = kRate / 2;    // frames that must then be equal
constexpr double kBeatMs = 500.0;
constexpr double kPi = 3.14159265358979323846;

int failures = 0;

void check (bool condition, const char* effect, const char* label)
{
    std::printf ("  [%s] %s: %s\n", condition ? "PASS" : "FAIL", effect, label);
    if (! condition)
        ++failures;
}

struct Signal
{
    unsigned frame = 0;
    unsigned seed = 1;

    void fill (float* left, float* right, int frames)
    {
        for (int i = 0; i < frames; ++i, ++frame)
        {
            seed = seed * 1664525u + 1013904223u;
            const double t = (double) frame / kRate;
            const double noise = ((double) (seed >> 8) / 16777216.0 - 0.5) * 0.01;
            const double tone = 0.2 * std::sin (2.0 * kPi * 82.41 * t)
                                + 0.1 * std::sin (2.0 * kPi * 220.0 * t);
            left[i] = (float) (tone + noise);
            right[i] = (float) (tone - noise);
        }
    }
};

// Both engines through `frames` frames; returns how many output samples differed.
long run (IEngine& exact, IEngine& depop, Signal& signal, int frames)
{
    std::array<float, kBlock> inL {}, inR {}, aL {}, aR {}, bL {}, bR {};
    long differing = 0;

    for (int done = 0; done < frames; done += kBlock)
    {
        signal.fill (inL.data(), inR.data(), kBlock);
        exact.process (inL.data(), inR.data(), aL.data(), aR.data(), kBlock);
        depop.process (inL.data(), inR.data(), bL.data(), bR.data(), kBlock);

        for (int i = 0; i < kBlock; ++i)
            differing += (aL[(size_t) i] != bL[(size_t) i]) + (aR[(size_t) i] != bR[(size_t) i]);
    }

    return differing;
}
} // namespace

int main()
{
    gm2_context context {};
    if (gm2_context_init (&context, kRate) != 0)
        return 1;

    std::array<int, kUnitCount> variants {};

    for (int unit = 0; unit < kUnitCount; ++unit)
    {
        for (int index = 0; index < getEffectCount (unit); ++index)
        {
            const auto& info = getEffectInfo (unit, index);
            auto depop = createDepopEngine (unit, index);
            check ((depop != nullptr) == info.depop, info.key, "has an engine exactly when it has a variant");

            if (depop == nullptr)
                continue;

            ++variants[(size_t) unit];
            auto exact = createEffectEngine (unit, index);
            std::array<int, kMaxKnobs> values {};
            for (int knob = 0; knob < info.knobCount; ++knob)
                values[(size_t) knob] = info.knobs[knob].def;

            exact->prepare (context, values.data(), kBeatMs);
            depop->prepare (context, values.data(), kBeatMs);
            check (exact->isValid() && depop->isValid(), info.key, "sets up at the defaults");

            Signal signal;
            check (run (*exact, *depop, signal, kLead) == 0, info.key, "equal before any turn");

            // Every knob to its maximum in one block, each back to its default one knob a
            // block, then ten fast turns of each knob between 33 % and 66 %.
            long turning = 0;
            for (int knob = 0; knob < info.knobCount; ++knob)
                values[(size_t) knob] = info.knobs[knob].max;
            exact->update (values.data(), kBeatMs);
            depop->update (values.data(), kBeatMs);
            turning += run (*exact, *depop, signal, kBlock * 64);

            for (int knob = 0; knob < info.knobCount; ++knob)
            {
                values[(size_t) knob] = info.knobs[knob].def;
                exact->update (values.data(), kBeatMs);
                depop->update (values.data(), kBeatMs);
                turning += run (*exact, *depop, signal, kBlock);
            }

            for (int knob = 0; knob < info.knobCount; ++knob)
            {
                const auto& def = info.knobs[knob];
                for (int turn = 0; turn < 10; ++turn)
                {
                    const double share = turn % 2 == 0 ? 0.33 : 0.66;
                    values[(size_t) knob] = def.min + (int) std::lround (share * (def.max - def.min));
                    exact->update (values.data(), kBeatMs);
                    depop->update (values.data(), kBeatMs);
                    turning += run (*exact, *depop, signal, kBlock);
                }
                values[(size_t) knob] = def.def;
                exact->update (values.data(), kBeatMs);
                depop->update (values.data(), kBeatMs);
            }

            turning += run (*exact, *depop, signal, kSettle);
            check (turning > 0, info.key, "plays its own path while the turns settle");
            check (run (*exact, *depop, signal, kCompare) == 0, info.key, "equal once settled");
        }
    }

    check (variants[kUnitGMajor2] == 9, "catalog", "nine G Major 2 effects have a variant");
    check (variants[kUnitIntellifex] == 6, "catalog", "every Intellifex effect has a variant");
    std::printf ("%s\n", failures == 0 ? "depop_test: PASS" : "depop_test: FAIL");
    return failures == 0 ? 0 : 1;
}
