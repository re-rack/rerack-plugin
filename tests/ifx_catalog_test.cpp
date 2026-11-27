// The Intellifex catalog holds the library's own fixture knob tables, so every range is the
// firmware's window and every default the configuration's factory preset; its pages cover every
// knob and fit the editor, and every effect plays at its defaults.
#include "EffectCatalog.h"
#include "EffectEngine.h"

#include <ifx.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C"
{
unsigned ifx_fixture_knob_count (const char* name);
int ifx_fixture_knob (const char* name, unsigned index, int* min, int* max, int* def);
}

namespace
{
constexpr int kEditorRows = 13;
constexpr int kFrames = 39000;
constexpr double kPi = 3.14159265358979323846;

int failures = 0;

void check (bool condition, const char* effect, const char* label)
{
    std::printf ("  [%s] %s: %s\n", condition ? "PASS" : "FAIL", effect, label);
    if (! condition)
        ++failures;
}

bool matchesFixture (const EffectInfo& info, const char* fixture)
{
    const unsigned count = ifx_fixture_knob_count (fixture);
    if (count == 0 || (int) count + 2 != info.knobCount)
        return false;

    for (unsigned i = 0; i < count; ++i)
    {
        int min = 0, max = 0, def = 0;
        const KnobDef& knob = info.knobs[i];
        if (ifx_fixture_knob (fixture, i, &min, &max, &def) != 0 || knob.min != min || knob.max != max
            || knob.def != def)
        {
            std::printf ("  knob %u %s: %d..%d (%d), the fixture has %d..%d (%d)\n", i, knob.field,
                         knob.min, knob.max, knob.def, min, max, def);
            return false;
        }
    }

    return std::strcmp (info.knobs[count].field, "analog_direct") == 0
           && std::strcmp (info.knobs[count + 1].field, "output_level") == 0;
}

bool pagesCoverKnobs (const EffectInfo& info, bool& fit)
{
    std::vector<bool> shown ((size_t) info.knobCount, false);
    fit = info.pageCount > 0;

    for (int p = 0; p < info.pageCount; ++p)
    {
        fit = fit && info.pages[p].knobCount <= kEditorRows;

        for (int i = 0; i < info.pages[p].knobCount; ++i)
        {
            const int knob = info.pages[p].knobs[i];
            if (knob < 0 || knob >= info.knobCount)
                return false;
            shown[(size_t) knob] = true;
        }
    }

    for (const bool knobShown : shown)
        if (! knobShown)
            return false;

    return true;
}
} // namespace

int main()
{
    gm2_context context {};

    for (int index = 0; index < getEffectCount (kUnitIntellifex); ++index)
    {
        const auto& info = getEffectInfo (kUnitIntellifex, index);
        const std::string fixture = std::string (info.key).substr (4); // past "ifx_"

        check (matchesFixture (info, fixture.c_str()), info.key,
               "knob ranges and defaults are the fixture's, then Analog Direct and Output Level");

        bool fit = false;
        check (pagesCoverKnobs (info, fit), info.key, "every knob is on a page");
        check (fit, info.key, "every page fits the editor");

        std::array<int, kMaxKnobs> values {};
        for (int knob = 0; knob < info.knobCount; ++knob)
            values[(size_t) knob] = info.knobs[knob].def;

        auto engine = createEffectEngine (kUnitIntellifex, index);
        engine->prepare (context, values.data(), 500.0);
        check (engine->isValid(), info.key, "sets up at the defaults");

        std::vector<float> in ((size_t) kFrames), left ((size_t) kFrames), right ((size_t) kFrames);
        for (int i = 0; i < kFrames; ++i)
            in[(size_t) i] = (float) (0.25 * std::sin (2.0 * kPi * 220.0 * i / kFrames) * (i % 9750 < 4000));

        engine->process (in.data(), nullptr, left.data(), right.data(), kFrames);

        double energy = 0.0;
        bool finite = true;
        for (int i = 0; i < kFrames; ++i)
        {
            finite = finite && std::isfinite (left[(size_t) i]) && std::isfinite (right[(size_t) i]);
            energy += (double) left[(size_t) i] * left[(size_t) i] + (double) right[(size_t) i] * right[(size_t) i];
        }

        check (finite && energy > 1.0, info.key, "plays a test signal at its defaults");
    }

    std::printf ("%s\n", failures == 0 ? "ifx_catalog_test: PASS" : "ifx_catalog_test: FAIL");
    return failures == 0 ? 0 : 1;
}
