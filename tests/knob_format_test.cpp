// Every Int knob's display text reads back as the same raw value, so a value
// typed as shown lands where it was shown.
#include "EffectCatalog.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace
{
int failures = 0;

void check (bool condition, const char* label)
{
    std::printf ("  [%s] %s\n", condition ? "PASS" : "FAIL", label);
    if (! condition)
        ++failures;
}

const KnobDef* findKnob (const char* effectKey, const char* field)
{
    for (int index = 0; index < getEffectCount(); ++index)
    {
        const auto& info = getEffectInfo (index);
        if (std::strcmp (info.key, effectKey) != 0)
            continue;

        for (int knob = 0; knob < info.knobCount; ++knob)
            if (std::strcmp (info.knobs[knob].field, field) == 0)
                return &info.knobs[knob];
    }

    return nullptr;
}

bool shows (const char* effectKey, const char* field, int raw, const char* expected)
{
    const KnobDef* knob = findKnob (effectKey, field);
    return knob != nullptr && formatKnobValue (*knob, raw) == expected;
}

bool reads (const char* effectKey, const char* field, const char* text, int expected)
{
    const KnobDef* knob = findKnob (effectKey, field);
    return knob != nullptr && parseKnobValue (*knob, text) == expected;
}
} // namespace

int main()
{
    long values = 0;
    long mismatches = 0;

    for (int index = 0; index < getEffectCount(); ++index)
    {
        const auto& info = getEffectInfo (index);

        for (int knob = 0; knob < info.knobCount; ++knob)
        {
            const KnobDef& def = info.knobs[knob];
            if (def.kind != KnobKind::Int)
                continue;

            for (int raw = def.min; raw <= def.max; ++raw, ++values)
            {
                const std::string text = formatKnobValue (def, raw);
                if (parseKnobValue (def, text.c_str()) != raw)
                {
                    if (++mismatches <= 10)
                        std::printf ("  %s %s: %d shows \"%s\"\n", info.key, def.field, raw, text.c_str());
                }
            }
        }
    }

    std::printf ("  %ld values\n", values);
    check (mismatches == 0, "every display text reads back as its raw value");

    check (shows ("autowah", "speed", 0, "0.050 Hz"), "slow speeds show three decimals");
    check (shows ("autowah", "speed", 160, "5.00 Hz"), "speeds from 1 Hz show two decimals");
    check (shows ("vintunivibe", "speed_1", 37, "Stop"), "the univibe's lowest speed is Stop");
    check (shows ("tremolo", "hi_cut", 0, "20.0 Hz"), "cuts below 100 Hz show one decimal");
    check (shows ("tremolo", "hi_cut", 58, "15.89 kHz"), "cuts from 1 kHz show kHz");
    check (shows ("eq", "freq_1", 241, "Off"), "the EQ's top frequency is Off");
    check (shows ("gate", "threshold", -61, "Off"), "the gate's lowest threshold is Off");
    check (shows ("achorus", "delay", 140, "14.0 ms"), "advanced delays are tenths of a ms");
    check (shows ("reverb_hall", "decay", 25, "2.5 s"), "reverb decays are tenths of a second");
    check (shows ("wahwah", "out_level", -12, "-12 dB"), "levels are whole dB");

    check (reads ("tremolo", "hi_cut", "1.6 kHz", 38), "kHz is read as thousands of Hz");
    check (reads ("eq", "freq_1", "off", 241), "Off is read case-insensitively");
    check (reads ("vintunivibe", "speed_1", "STOP", 37), "Stop is read case-insensitively");
    check (reads ("wahwah", "mix", "200", 100), "values beyond the range clamp to it");

    std::printf ("%s\n", failures == 0 ? "knob_format_test: PASS" : "knob_format_test: FAIL");
    return failures == 0 ? 0 : 1;
}
