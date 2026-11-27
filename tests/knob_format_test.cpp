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
    for (int unit = 0; unit < kUnitCount; ++unit)
    {
        for (int index = 0; index < getEffectCount (unit); ++index)
        {
            const auto& info = getEffectInfo (unit, index);
            if (std::strcmp (info.key, effectKey) != 0)
                continue;

            for (int knob = 0; knob < info.knobCount; ++knob)
                if (std::strcmp (info.knobs[knob].field, field) == 0)
                    return &info.knobs[knob];
        }
    }

    return nullptr;
}

bool shows (const char* effectKey, const char* field, int raw, const char* expected, int linked = 0)
{
    const KnobDef* knob = findKnob (effectKey, field);
    return knob != nullptr && formatKnobValue (*knob, raw, linked) == expected;
}

bool reads (const char* effectKey, const char* field, const char* text, int expected, int linked = 0)
{
    const KnobDef* knob = findKnob (effectKey, field);
    return knob != nullptr && parseKnobValue (*knob, text, linked) == expected;
}
} // namespace

int main()
{
    long values = 0;
    long mismatches = 0;

    for (int unit = 0; unit < kUnitCount; ++unit)
    {
        for (int index = 0; index < getEffectCount (unit); ++index)
        {
            const auto& info = getEffectInfo (unit, index);

            for (int knob = 0; knob < info.knobCount; ++knob)
            {
                const KnobDef& def = info.knobs[knob];
                if (def.kind != KnobKind::Int)
                    continue;

                const KnobDef* link = def.link >= 0 ? &info.knobs[def.link] : nullptr;
                const int linkedMax = link != nullptr ? link->max : 0;

                for (int linked = 0; linked <= linkedMax; ++linked)
                {
                    for (int raw = def.min; raw <= def.max; ++raw, ++values)
                    {
                        const std::string text = formatKnobValue (def, raw, linked);
                        if (parseKnobValue (def, text.c_str(), linked) != raw)
                        {
                            if (++mismatches <= 10)
                                std::printf ("  %s %s: %d shows \"%s\"\n", info.key, def.field, raw, text.c_str());
                        }
                    }
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

    check (shows ("ifx_reverb", "reverb_level", 46, "0.0 dB")
               && shows ("ifx_reverb", "reverb_level", 58, "+6.0 dB")
               && shows ("ifx_reverb", "reverb_level", 18, "-14 dB")
               && shows ("ifx_reverb", "reverb_level", 19, "-13.5 dB"),
           "Intellifex +6 dB levels read as the device shows them");
    check (shows ("ifx_delay", "level1", 0, "-inf dB") && shows ("ifx_delay", "level1", 1, "-48 dB")
               && shows ("ifx_delay", "level1", 58, "0.0 dB"),
           "Intellifex 0 dB levels read as the device shows them");
    check (shows ("ifx_reverb", "hush_threshold", 8, "-68 dB"), "thresholds fall in 3 dB steps from -92 dB");
    check (shows ("ifx_reverb", "hush_release", 9, "800 ms"), "Hush release is the device's table");
    check (shows ("ifx_delay", "ducker_release", 19, "9.0 s"), "the ducker release is the device's table");
    check (shows ("ifx_delay", "pan1", 25, "<25") && shows ("ifx_delay", "pan1", 50, "<50>")
               && shows ("ifx_delay", "pan1", 75, "75>"),
           "pans point to the louder side");
    check (shows ("ifx_pitch", "pitch1", 85, "-700 cents") && shows ("ifx_pitch", "pitch1", 180, "+1200 cents")
               && shows ("ifx_pitch", "pitch1", 120, "0 cents"),
           "pitch is in 20-cent steps around 120");
    check (shows ("ifx_pitch", "fine4", 36, "+16 cents"), "fine pitch is in cents around 20");
    check (shows ("ifx_chorus8", "delay1", 5, "10 ms"), "voice delays are 2 ms steps");
    check (shows ("ifx_delay", "time1", 75, "375 ms", 0) && shows ("ifx_delay", "time1", 75, "750 ms", 2),
           "delay times double in 2-Tap");

    check (reads ("ifx_delay", "pan1", "<25", 25) && reads ("ifx_delay", "pan2", "75>", 75), "pans read with arrows");
    check (reads ("ifx_delay", "level1", "-INF", 0), "-inf reads case-insensitively");
    check (reads ("ifx_delay", "time1", "750 ms", 75, 2), "a 2-Tap delay time reads in 10 ms steps");

    std::printf ("%s\n", failures == 0 ? "knob_format_test: PASS" : "knob_format_test: FAIL");
    return failures == 0 ? 0 : 1;
}
