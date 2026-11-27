// The Intellifex LTD's six configurations. Knob names, ranges, pages and value text are the
// firmware's (EPROM parameter names, MCU windows, FUNCTION SELECT pages and display routines);
// defaults are each configuration's factory preset.
#include "IfxCatalog.h"

#include "EffectEngine.h"

#include <ifx.h>

#include <cstddef>

namespace
{
constexpr const char* kDirectHushItems[] = { "Pre Hush", "Post Hush" };
constexpr ChoiceList kDirectHush { kDirectHushItems, 2 };

constexpr const char* kOutInItems[] = { "Out", "In" };
constexpr ChoiceList kOutIn { kOutInItems, 2 };

constexpr const char* kOffOnItems[] = { "Off", "On" };
constexpr ChoiceList kOffOn { kOffOnItems, 2 };

constexpr const char* kMutedActiveItems[] = { "Muted", "Active" };
constexpr ChoiceList kMutedActive { kMutedActiveItems, 2 };

constexpr const char* kMuteTypeItems[] = { "Pre", "Post", "Both" };
constexpr ChoiceList kMuteType { kMuteTypeItems, 3 };

constexpr const char* kDelayTypeItems[] = { "Stereo", "Ping-Pong", "2-Tap" };
constexpr ChoiceList kDelayType { kDelayTypeItems, 3 };

constexpr const char* kReverbTypeItems[] = { "Plate A", "Plate B", "Room A", "Room B",
                                             "Hall A", "Hall B", "Stadium", "Dual" };
constexpr ChoiceList kReverbType { kReverbTypeItems, 8 };

constexpr const char* kDuckerModeItems[] = { "Off", "Dly", "Rev", "Both" };
constexpr ChoiceList kDuckerMode { kDuckerModeItems, 4 };

#define INT(Raw, member, displayName, mx, df, unitValue) \
    KnobDef { #member, displayName, 0, mx, df, KnobKind::Int, KnobUnit::unitValue, nullptr, (uint32_t) offsetof (Raw, member) }

#define LINKED(Raw, member, displayName, mx, df, unitValue, linkIndex)                              \
    KnobDef { #member, displayName, 0, mx, df, KnobKind::Int, KnobUnit::unitValue, nullptr,         \
              (uint32_t) offsetof (Raw, member), linkIndex }

#define CHOICE(Raw, member, displayName, df, list) \
    KnobDef { #member, displayName, 0, (list).count - 1, df, KnobKind::Choice, KnobUnit::Plain, &(list), (uint32_t) offsetof (Raw, member) }

#define DIRECT(Raw, df)                                             \
    INT (Raw, direct_left, "L Dir Level", 58, df, IfxLevel6),       \
    INT (Raw, direct_right, "R Dir Level", 58, df, IfxLevel6),      \
    CHOICE (Raw, direct_hush, "Direct Hush", 0, kDirectHush)

#define REGEN(Raw, left, right)                                     \
    INT (Raw, regen_left, "Regen L", 58, left, IfxLevel0),          \
    INT (Raw, regen_right, "Regen R", 58, right, IfxLevel0)

#define HUSH(Raw)                                                   \
    CHOICE (Raw, hush, "Hush I/O", 1, kOutIn),                      \
    INT (Raw, hush_threshold, "Exp Thresh", 24, 8, IfxThreshold),   \
    INT (Raw, hush_release, "Rel Rate", 9, 5, IfxHushRelease)

#define CHORUS_VOICE(Raw, n, lv, pn, dl, dp, rt)                    \
    INT (Raw, level##n, "Level " #n, 58, lv, IfxLevel0),            \
    INT (Raw, pan##n, "Pan " #n, 100, pn, IfxPan),                  \
    INT (Raw, delay##n, "Delay " #n, 209, dl, IfxVoiceDelay),       \
    INT (Raw, depth##n, "Depth " #n, 100, dp, Plain),               \
    INT (Raw, rate##n, "Rate " #n, 254, rt, Plain)

#define PITCH_VOICE(Raw, n, pt, fn, lv, pn, dl)                     \
    INT (Raw, pitch##n, "Pitch " #n, 180, pt, IfxPitch),            \
    INT (Raw, fine##n, "Fine " #n, 40, fn, IfxFine),                \
    INT (Raw, level##n, "Level " #n, 58, lv, IfxLevel0),            \
    INT (Raw, pan##n, "Pan " #n, 100, pn, IfxPan),                  \
    INT (Raw, delay##n, "Delay " #n, 209, dl, IfxVoiceDelay)

#define DUCKER(Raw, sensitivity, attenuation, release)                              \
    INT (Raw, ducker_sensitivity, "Sensitivity", 24, sensitivity, IfxThreshold),    \
    INT (Raw, ducker_attenuation, "Attenuation", 58, attenuation, IfxLevel0),       \
    INT (Raw, ducker_release, "Release Rate", 19, release, IfxDuckerRelease)

#define REVERB_NET(Raw, pan, mix, decay, damp)                      \
    CHOICE (Raw, rev_input, "Rev Input", 1, kMutedActive),          \
    INT (Raw, dir_in_pan, "Dir In Pan", 100, pan, IfxPan),          \
    INT (Raw, mix_dir_dly, "Mix Dir/Dly", 100, mix, IfxPan),        \
    INT (Raw, reverb_decay, "Reverb Decay", 99, decay, Plain),      \
    INT (Raw, hf_damp, "RV HF Damp", 99, damp, Plain)

// Library knobs standing in for the analog stages: the dry path on as on the hardware, and the
// output pot.
#define ANALOG(Raw)                                                 \
    CHOICE (Raw, analog_direct, "Analog Direct", 1, kOffOn),        \
    INT (Raw, output_level, "Output Level", 100, 100, Percent)

constexpr KnobDef kReverb[] = {
    INT (ifx_reverb_setup_raw, reverb_level, "Reverb Level", 58, 46, IfxLevel6),
    DIRECT (ifx_reverb_setup_raw, 46),
    HUSH (ifx_reverb_setup_raw),
    INT (ifx_reverb_setup_raw, reverb_decay, "Reverb Decay", 99, 81, Plain),
    INT (ifx_reverb_setup_raw, hf_damp, "RV HF Damp", 99, 42, Plain),
    INT (ifx_reverb_setup_raw, low_freq, "Low Freq", 99, 50, Plain),
    CHOICE (ifx_reverb_setup_raw, reverb_type, "Rev Type", 6, kReverbType),
    INT (ifx_reverb_setup_raw, dir_in_pan, "DirIn Pan", 100, 49, IfxPan),
    INT (ifx_reverb_setup_raw, predelay_left, "Predelay L", 209, 157, Milliseconds),
    INT (ifx_reverb_setup_raw, predelay_right, "Predelay R", 209, 209, Milliseconds),
    CHOICE (ifx_reverb_setup_raw, gate, "Gate", 0, kOffOn),
    INT (ifx_reverb_setup_raw, gate_decay, "Gate Decay", 31, 0, Plain),
    INT (ifx_reverb_setup_raw, gate_threshold, "Gate Thresh", 24, 0, IfxThreshold),
    INT (ifx_reverb_setup_raw, gate_hold, "Hold Time", 99, 0, Plain),
    ANALOG (ifx_reverb_setup_raw),
};

constexpr int8_t kDelayTypeKnob = 17;

constexpr KnobDef kDelay[] = {
    INT (ifx_delay_setup_raw, delay_level, "Delay Level", 58, 48, IfxLevel6),
    DIRECT (ifx_delay_setup_raw, 46),
    HUSH (ifx_delay_setup_raw),
    CHOICE (ifx_delay_setup_raw, delay, "Delay", 1, kMutedActive),
    CHOICE (ifx_delay_setup_raw, mute_type, "Mute Type", 0, kMuteType),
    INT (ifx_delay_setup_raw, level1, "Level 1", 58, 58, IfxLevel0),
    INT (ifx_delay_setup_raw, pan1, "Pan 1", 100, 0, IfxPan),
    LINKED (ifx_delay_setup_raw, time1, "Dly Time 1", 150, 75, IfxDelayTime, kDelayTypeKnob),
    INT (ifx_delay_setup_raw, regen1, "Regen 1", 58, 46, IfxLevel0),
    INT (ifx_delay_setup_raw, level2, "Level 2", 58, 58, IfxLevel0),
    INT (ifx_delay_setup_raw, pan2, "Pan 2", 100, 100, IfxPan),
    LINKED (ifx_delay_setup_raw, time2, "Dly Time 2", 150, 50, IfxDelayTime, kDelayTypeKnob),
    INT (ifx_delay_setup_raw, regen2, "Regen 2", 58, 52, IfxLevel0),
    CHOICE (ifx_delay_setup_raw, delay_type, "D Type", 0, kDelayType),
    INT (ifx_delay_setup_raw, hf_damp, "DL HF Damp", 99, 12, Plain),
    CHOICE (ifx_delay_setup_raw, ducker, "Ducker", 1, kOffOn),
    DUCKER (ifx_delay_setup_raw, 20, 19, 8),
    ANALOG (ifx_delay_setup_raw),
};

constexpr KnobDef kChorus8[] = {
    INT (ifx_chorus8_setup_raw, chorus_level, "Chorus Level", 58, 44, IfxLevel6),
    DIRECT (ifx_chorus8_setup_raw, 42),
    REGEN (ifx_chorus8_setup_raw, 0, 3),
    HUSH (ifx_chorus8_setup_raw),
    CHORUS_VOICE (ifx_chorus8_setup_raw, 1, 58, 0, 5, 41, 25),
    CHORUS_VOICE (ifx_chorus8_setup_raw, 2, 58, 0, 20, 41, 21),
    CHORUS_VOICE (ifx_chorus8_setup_raw, 3, 58, 0, 30, 36, 5),
    CHORUS_VOICE (ifx_chorus8_setup_raw, 4, 58, 0, 60, 39, 73),
    CHORUS_VOICE (ifx_chorus8_setup_raw, 5, 58, 100, 30, 40, 38),
    CHORUS_VOICE (ifx_chorus8_setup_raw, 6, 58, 100, 120, 43, 42),
    CHORUS_VOICE (ifx_chorus8_setup_raw, 7, 46, 100, 140, 37, 10),
    CHORUS_VOICE (ifx_chorus8_setup_raw, 8, 34, 100, 160, 32, 35),
    ANALOG (ifx_chorus8_setup_raw),
};

constexpr KnobDef kPitch[] = {
    INT (ifx_pitch_setup_raw, pitch_level, "PShift Level", 58, 46, IfxLevel6),
    DIRECT (ifx_pitch_setup_raw, 46),
    REGEN (ifx_pitch_setup_raw, 0, 0),
    HUSH (ifx_pitch_setup_raw),
    PITCH_VOICE (ifx_pitch_setup_raw, 1, 85, 20, 58, 25, 0),
    PITCH_VOICE (ifx_pitch_setup_raw, 2, 145, 20, 58, 75, 0),
    PITCH_VOICE (ifx_pitch_setup_raw, 3, 105, 20, 58, 25, 0),
    PITCH_VOICE (ifx_pitch_setup_raw, 4, 145, 36, 58, 75, 0),
    ANALOG (ifx_pitch_setup_raw),
};

constexpr KnobDef kChorusReverb[] = {
    INT (ifx_chorus_reverb_setup_raw, effects_level, "Effect Level", 58, 46, IfxLevel6),
    DIRECT (ifx_chorus_reverb_setup_raw, 46),
    INT (ifx_chorus_reverb_setup_raw, chorus_level, "Chorus Level", 58, 58, IfxLevel0),
    INT (ifx_chorus_reverb_setup_raw, delay_level, "Delay Level", 58, 32, IfxLevel0),
    INT (ifx_chorus_reverb_setup_raw, reverb_level, "Reverb Level", 58, 58, IfxLevel0),
    REGEN (ifx_chorus_reverb_setup_raw, 0, 41),
    HUSH (ifx_chorus_reverb_setup_raw),
    CHORUS_VOICE (ifx_chorus_reverb_setup_raw, 1, 58, 0, 0, 36, 12),
    CHORUS_VOICE (ifx_chorus_reverb_setup_raw, 2, 58, 100, 0, 40, 30),
    CHORUS_VOICE (ifx_chorus_reverb_setup_raw, 3, 58, 0, 88, 0, 0),
    CHORUS_VOICE (ifx_chorus_reverb_setup_raw, 4, 58, 100, 176, 0, 0),
    CHOICE (ifx_chorus_reverb_setup_raw, ducker, "Ducker", 0, kDuckerMode),
    DUCKER (ifx_chorus_reverb_setup_raw, 0, 58, 0),
    REVERB_NET (ifx_chorus_reverb_setup_raw, 30, 10, 71, 18),
    ANALOG (ifx_chorus_reverb_setup_raw),
};

constexpr KnobDef kPitchReverb[] = {
    INT (ifx_pitch_reverb_setup_raw, effects_level, "Effect Level", 58, 46, IfxLevel6),
    DIRECT (ifx_pitch_reverb_setup_raw, 46),
    INT (ifx_pitch_reverb_setup_raw, pitch_level, "PShift Level", 58, 58, IfxLevel0),
    INT (ifx_pitch_reverb_setup_raw, delay_level, "Delay Level", 58, 58, IfxLevel0),
    INT (ifx_pitch_reverb_setup_raw, reverb_level, "Reverb Level", 58, 48, IfxLevel0),
    REGEN (ifx_pitch_reverb_setup_raw, 0, 0),
    HUSH (ifx_pitch_reverb_setup_raw),
    PITCH_VOICE (ifx_pitch_reverb_setup_raw, 1, 60, 20, 54, 22, 1),
    PITCH_VOICE (ifx_pitch_reverb_setup_raw, 2, 120, 36, 54, 84, 1),
    INT (ifx_pitch_reverb_setup_raw, level3, "Level 3", 58, 31, IfxLevel0),
    INT (ifx_pitch_reverb_setup_raw, pan3, "Pan 3", 100, 81, IfxPan),
    INT (ifx_pitch_reverb_setup_raw, delay3, "Delay 3", 209, 160, IfxVoiceDelay),
    CHOICE (ifx_pitch_reverb_setup_raw, ducker, "Ducker", 0, kDuckerMode),
    DUCKER (ifx_pitch_reverb_setup_raw, 0, 58, 0),
    REVERB_NET (ifx_pitch_reverb_setup_raw, 50, 50, 66, 18),
    ANALOG (ifx_pitch_reverb_setup_raw),
};

#undef INT
#undef LINKED
#undef CHOICE
#undef DIRECT
#undef REGEN
#undef HUSH
#undef CHORUS_VOICE
#undef PITCH_VOICE
#undef DUCKER
#undef REVERB_NET
#undef ANALOG

constexpr bool sameName (const char* a, const char* b)
{
    while (*a != '\0' && *a == *b)
    {
        ++a;
        ++b;
    }

    return *a == *b;
}

// Index of a knob by field; a missing field fails the build.
template <size_t N>
constexpr int at (const KnobDef (&knobs)[N], const char* field)
{
    for (size_t i = 0; i < N; ++i)
        if (sameName (knobs[i].field, field))
            return (int) i;

    throw "no such knob";
}

static_assert (at (kDelay, "delay_type") == kDelayTypeKnob, "Dly Time follows D Type");

#define PAGE(name, knobs) PageInfo { name, knobs, (int) (sizeof (knobs) / sizeof (knobs[0])) }
#define HUSH_PAGE(table) { at (table, "hush"), at (table, "hush_threshold"), at (table, "hush_release") }
#define CHORUS_VOICE_PAGE(table, n) \
    { at (table, "level" #n), at (table, "pan" #n), at (table, "delay" #n), at (table, "depth" #n), at (table, "rate" #n) }
#define PITCH_VOICE_PAGE(table, n) \
    { at (table, "pitch" #n), at (table, "fine" #n), at (table, "level" #n), at (table, "pan" #n), at (table, "delay" #n) }
#define DUCKER_PAGE(table) \
    { at (table, "ducker"), at (table, "ducker_sensitivity"), at (table, "ducker_attenuation"), at (table, "ducker_release") }
#define REVERB_NET_PAGE(table)                                                                    \
    { at (table, "rev_input"), at (table, "dir_in_pan"), at (table, "mix_dir_dly"),              \
      at (table, "reverb_level"), at (table, "reverb_decay"), at (table, "hf_damp") }

constexpr int kReverbMixer[] = { at (kReverb, "reverb_level"), at (kReverb, "direct_left"),
                                 at (kReverb, "direct_right"), at (kReverb, "direct_hush"),
                                 at (kReverb, "analog_direct"), at (kReverb, "output_level") };
constexpr int kReverbHush[] = HUSH_PAGE (kReverb);
constexpr int kReverbReverb[] = { at (kReverb, "reverb_level"), at (kReverb, "reverb_decay"),
                                  at (kReverb, "hf_damp"), at (kReverb, "low_freq"),
                                  at (kReverb, "reverb_type"), at (kReverb, "dir_in_pan"),
                                  at (kReverb, "predelay_left"), at (kReverb, "predelay_right"),
                                  at (kReverb, "gate"), at (kReverb, "gate_decay"),
                                  at (kReverb, "gate_threshold"), at (kReverb, "gate_hold") };
constexpr PageInfo kReverbPages[] = { PAGE ("Mixer", kReverbMixer), PAGE ("Hush", kReverbHush),
                                      PAGE ("Reverb", kReverbReverb) };

constexpr int kDelayMixer[] = { at (kDelay, "delay_level"), at (kDelay, "direct_left"),
                                at (kDelay, "direct_right"), at (kDelay, "direct_hush"),
                                at (kDelay, "analog_direct"), at (kDelay, "output_level") };
constexpr int kDelayHush[] = HUSH_PAGE (kDelay);
constexpr int kDelayDelay[] = { at (kDelay, "delay"), at (kDelay, "mute_type"), at (kDelay, "level1"),
                                at (kDelay, "pan1"), at (kDelay, "time1"), at (kDelay, "regen1"),
                                at (kDelay, "level2"), at (kDelay, "pan2"), at (kDelay, "time2"),
                                at (kDelay, "regen2"), at (kDelay, "delay_type"), at (kDelay, "hf_damp") };
constexpr int kDelayDucker[] = DUCKER_PAGE (kDelay);
constexpr PageInfo kDelayPages[] = { PAGE ("Mixer", kDelayMixer), PAGE ("Hush", kDelayHush),
                                     PAGE ("Delay", kDelayDelay), PAGE ("Ducker", kDelayDucker) };

constexpr int kChorus8Mixer[] = { at (kChorus8, "chorus_level"), at (kChorus8, "direct_left"),
                                  at (kChorus8, "direct_right"), at (kChorus8, "direct_hush"),
                                  at (kChorus8, "regen_left"), at (kChorus8, "regen_right"),
                                  at (kChorus8, "analog_direct"), at (kChorus8, "output_level") };
constexpr int kChorus8Hush[] = HUSH_PAGE (kChorus8);
constexpr int kChorus8Voice1[] = CHORUS_VOICE_PAGE (kChorus8, 1);
constexpr int kChorus8Voice2[] = CHORUS_VOICE_PAGE (kChorus8, 2);
constexpr int kChorus8Voice3[] = CHORUS_VOICE_PAGE (kChorus8, 3);
constexpr int kChorus8Voice4[] = CHORUS_VOICE_PAGE (kChorus8, 4);
constexpr int kChorus8Voice5[] = CHORUS_VOICE_PAGE (kChorus8, 5);
constexpr int kChorus8Voice6[] = CHORUS_VOICE_PAGE (kChorus8, 6);
constexpr int kChorus8Voice7[] = CHORUS_VOICE_PAGE (kChorus8, 7);
constexpr int kChorus8Voice8[] = CHORUS_VOICE_PAGE (kChorus8, 8);
constexpr PageInfo kChorus8Pages[] = {
    PAGE ("Mixer", kChorus8Mixer),        PAGE ("Hush", kChorus8Hush),
    PAGE ("Voice/Dly 1", kChorus8Voice1), PAGE ("Voice/Dly 2", kChorus8Voice2),
    PAGE ("Voice/Dly 3", kChorus8Voice3), PAGE ("Voice/Dly 4", kChorus8Voice4),
    PAGE ("Voice/Dly 5", kChorus8Voice5), PAGE ("Voice/Dly 6", kChorus8Voice6),
    PAGE ("Voice/Dly 7", kChorus8Voice7), PAGE ("Voice/Dly 8", kChorus8Voice8),
};

constexpr int kPitchMixer[] = { at (kPitch, "pitch_level"), at (kPitch, "direct_left"),
                                at (kPitch, "direct_right"), at (kPitch, "direct_hush"),
                                at (kPitch, "regen_left"), at (kPitch, "regen_right"),
                                at (kPitch, "analog_direct"), at (kPitch, "output_level") };
constexpr int kPitchHush[] = HUSH_PAGE (kPitch);
constexpr int kPitchVoice1[] = PITCH_VOICE_PAGE (kPitch, 1);
constexpr int kPitchVoice2[] = PITCH_VOICE_PAGE (kPitch, 2);
constexpr int kPitchVoice3[] = PITCH_VOICE_PAGE (kPitch, 3);
constexpr int kPitchVoice4[] = PITCH_VOICE_PAGE (kPitch, 4);
constexpr PageInfo kPitchPages[] = { PAGE ("Mixer", kPitchMixer),        PAGE ("Hush", kPitchHush),
                                     PAGE ("Voice/Dly 1", kPitchVoice1), PAGE ("Voice/Dly 2", kPitchVoice2),
                                     PAGE ("Voice/Dly 3", kPitchVoice3), PAGE ("Voice/Dly 4", kPitchVoice4) };

constexpr int kChorusReverbMixer[] = {
    at (kChorusReverb, "effects_level"), at (kChorusReverb, "direct_left"),
    at (kChorusReverb, "direct_right"),  at (kChorusReverb, "direct_hush"),
    at (kChorusReverb, "chorus_level"),  at (kChorusReverb, "delay_level"),
    at (kChorusReverb, "reverb_level"),  at (kChorusReverb, "regen_left"),
    at (kChorusReverb, "regen_right"),   at (kChorusReverb, "analog_direct"),
    at (kChorusReverb, "output_level"),
};
constexpr int kChorusReverbHush[] = HUSH_PAGE (kChorusReverb);
constexpr int kChorusReverbVoice1[] = CHORUS_VOICE_PAGE (kChorusReverb, 1);
constexpr int kChorusReverbVoice2[] = CHORUS_VOICE_PAGE (kChorusReverb, 2);
constexpr int kChorusReverbVoice3[] = CHORUS_VOICE_PAGE (kChorusReverb, 3);
constexpr int kChorusReverbVoice4[] = CHORUS_VOICE_PAGE (kChorusReverb, 4);
constexpr int kChorusReverbDucker[] = DUCKER_PAGE (kChorusReverb);
constexpr int kChorusReverbReverb[] = REVERB_NET_PAGE (kChorusReverb);
constexpr PageInfo kChorusReverbPages[] = {
    PAGE ("Mixer", kChorusReverbMixer),        PAGE ("Hush", kChorusReverbHush),
    PAGE ("Voice/Dly 1", kChorusReverbVoice1), PAGE ("Voice/Dly 2", kChorusReverbVoice2),
    PAGE ("Voice/Dly 3", kChorusReverbVoice3), PAGE ("Voice/Dly 4", kChorusReverbVoice4),
    PAGE ("Ducker", kChorusReverbDucker),      PAGE ("Reverb", kChorusReverbReverb),
};

constexpr int kPitchReverbMixer[] = {
    at (kPitchReverb, "effects_level"), at (kPitchReverb, "direct_left"),
    at (kPitchReverb, "direct_right"),  at (kPitchReverb, "direct_hush"),
    at (kPitchReverb, "pitch_level"),   at (kPitchReverb, "delay_level"),
    at (kPitchReverb, "reverb_level"),  at (kPitchReverb, "regen_left"),
    at (kPitchReverb, "regen_right"),   at (kPitchReverb, "analog_direct"),
    at (kPitchReverb, "output_level"),
};
constexpr int kPitchReverbHush[] = HUSH_PAGE (kPitchReverb);
constexpr int kPitchReverbVoice1[] = PITCH_VOICE_PAGE (kPitchReverb, 1);
constexpr int kPitchReverbVoice2[] = PITCH_VOICE_PAGE (kPitchReverb, 2);
constexpr int kPitchReverbDelay3[] = { at (kPitchReverb, "level3"), at (kPitchReverb, "pan3"),
                                       at (kPitchReverb, "delay3") };
constexpr int kPitchReverbDucker[] = DUCKER_PAGE (kPitchReverb);
constexpr int kPitchReverbReverb[] = REVERB_NET_PAGE (kPitchReverb);
constexpr PageInfo kPitchReverbPages[] = {
    PAGE ("Mixer", kPitchReverbMixer),        PAGE ("Hush", kPitchReverbHush),
    PAGE ("Voice/Dly 1", kPitchReverbVoice1), PAGE ("Voice/Dly 2", kPitchReverbVoice2),
    PAGE ("Delay 3", kPitchReverbDelay3),     PAGE ("Ducker", kPitchReverbDucker),
    PAGE ("Reverb", kPitchReverbReverb),
};

#undef PAGE
#undef HUSH_PAGE
#undef CHORUS_VOICE_PAGE
#undef PITCH_VOICE_PAGE
#undef DUCKER_PAGE
#undef REVERB_NET_PAGE

template <size_t N>
constexpr int knobCount (const KnobDef (&)[N])
{
    static_assert (N <= (size_t) kMaxKnobs, "effect exceeds the processor's knob capacity");
    return (int) N;
}

#define EFFECT(keyValue, nameValue, table, pageTable) \
    EffectInfo { keyValue, nameValue, nullptr, table, knobCount (table), false, pageTable, (int) (sizeof (pageTable) / sizeof (pageTable[0])) }

const EffectInfo kEffects[] = {
    EFFECT ("ifx_reverb", "Reverb", kReverb, kReverbPages),
    EFFECT ("ifx_delay", "Delay & Ducker", kDelay, kDelayPages),
    EFFECT ("ifx_chorus8", "8 Voice Chorus", kChorus8, kChorus8Pages),
    EFFECT ("ifx_pitch", "Pitch Shift & Delay", kPitch, kPitchPages),
    EFFECT ("ifx_chorus_reverb", "Chorus, Delay & Reverb", kChorusReverb, kChorusReverbPages),
    EFFECT ("ifx_pitch_reverb", "Pitch Shift, Delay & Reverb", kPitchReverb, kPitchReverbPages),
};

#undef EFFECT

constexpr int kEffectCount = (int) (sizeof (kEffects) / sizeof (kEffects[0]));

#define IFX_TRAITS(Traits, module)                                                                 \
    struct Traits                                                                                  \
    {                                                                                              \
        using Raw = ifx_##module##_setup_raw;                                                      \
        using Config = ifx_##module##_config;                                                      \
        using State = ifx_##module##_state;                                                        \
        static constexpr bool takesBeat = false;                                                   \
        static int setup (const gm2_context*, const Raw* raw, Config* config)                      \
        {                                                                                          \
            return ifx_##module##_setup_config (raw, config);                                      \
        }                                                                                          \
        static void init (State* state, const Config* config)                                      \
        {                                                                                          \
            ifx_##module##_state_init (state, config);                                             \
        }                                                                                          \
        static void process (State* state, const Config* config, const float* in, const float*,   \
                             float* outL, float* outR, size_t frames)                              \
        {                                                                                          \
            ifx_##module##_process_f32 (state, config, in, outL, outR, frames);                    \
        }                                                                                          \
        static void align (State*, const Config*, double) {}                                       \
    }

IFX_TRAITS (ReverbTraits, reverb);
IFX_TRAITS (DelayTraits, delay);
IFX_TRAITS (Chorus8Traits, chorus8);
IFX_TRAITS (PitchTraits, pitch);
IFX_TRAITS (ChorusReverbTraits, chorus_reverb);
IFX_TRAITS (PitchReverbTraits, pitch_reverb);

#undef IFX_TRAITS
} // namespace

int getIfxEffectCount()
{
    return kEffectCount;
}

const EffectInfo& getIfxEffectInfo (int index)
{
    return kEffects[index < 0 ? 0 : (index >= kEffectCount ? kEffectCount - 1 : index)];
}

std::unique_ptr<IEngine> createIfxEngine (int index)
{
    switch (index)
    {
        case 0: return std::make_unique<Engine<ReverbTraits>> (kEffects[0]);
        case 1: return std::make_unique<Engine<DelayTraits>> (kEffects[1]);
        case 2: return std::make_unique<Engine<Chorus8Traits>> (kEffects[2]);
        case 3: return std::make_unique<Engine<PitchTraits>> (kEffects[3]);
        case 4: return std::make_unique<Engine<ChorusReverbTraits>> (kEffects[4]);
        case 5: return std::make_unique<Engine<PitchReverbTraits>> (kEffects[5]);
        default: break;
    }

    return {};
}
