#include "EffectEngine.h"

#include <cstddef>

namespace
{
const char* const kTempoItems[] = { "Ignore", "1", "1/2D", "1/2", "1/2T", "1/4D", "1/4", "1/4T",
                                    "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T", "1/32D",
                                    "1/32", "1/32T" };
const ChoiceList kTempo { kTempoItems, 17 };

const char* const kOffOnItems[] = { "Off", "On" };
const ChoiceList kOffOn { kOffOnItems, 2 };

const char* const kTriangleSineItems[] = { "Triangle", "Sine" };
const ChoiceList kTriangleSine { kTriangleSineItems, 2 };

const char* const kLowHighItems[] = { "Low", "High" };
const ChoiceList kLowHigh { kLowHighItems, 2 };

const char* const kSoftHardItems[] = { "Soft", "Hard" };
const ChoiceList kSoftHard { kSoftHardItems, 2 };

const char* const kDownUpItems[] = { "Down", "Up" };
const ChoiceList kDownUp { kDownUpItems, 2 };

const char* const kResponseItems[] = { "Slow", "Medium", "Fast" };
const ChoiceList kResponse { kResponseItems, 3 };

const char* const kStyleItems[] = { "2nd Order", "4th Order" };
const ChoiceList kStyle { kStyleItems, 2 };

const char* const kUnivibeSelectItems[] = { "Speed 1", "Speed 2" };
const ChoiceList kUnivibeSelect { kUnivibeSelectItems, 2 };

const char* const kOctaveRangeItems[] = { "1 Oct", "2 Oct" };
const ChoiceList kOctaveRange { kOctaveRangeItems, 2 };

const char* const kCompRatioItems[] = { "Off", "1.12:1", "1.25:1", "1.4:1", "1.6:1", "1.8:1",
                                        "2.0:1", "2.5:1", "3.2:1", "4.0:1", "5.6:1", "8.0:1",
                                        "16:1", "32:1", "64:1", "Infinite" };
const ChoiceList kCompRatio { kCompRatioItems, 16 };

const char* const kCompAttackItems[] = { "1 ms", "1.4 ms", "2 ms", "3 ms", "5 ms", "7 ms",
                                         "10 ms", "14 ms", "20 ms", "30 ms", "50 ms", "70 ms" };
const ChoiceList kCompAttack { kCompAttackItems, 12 };

const char* const kCompReleaseItems[] = { "50 ms", "70 ms", "100 ms", "140 ms", "200 ms",
                                          "300 ms", "500 ms", "700 ms", "1000 ms", "1400 ms",
                                          "2000 ms" };
const ChoiceList kCompRelease { kCompReleaseItems, 11 };

const char* const kDynReleaseItems[] = { "20 ms", "30 ms", "50 ms", "70 ms", "100 ms", "140 ms",
                                         "200 ms", "300 ms", "500 ms", "700 ms", "1000 ms" };
const ChoiceList kDynRelease { kDynReleaseItems, 11 };

const char* const kKeyItems[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const ChoiceList kKey { kKeyItems, 12 };

const char* const kScaleItems[] = { "Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian",
                                    "Aeolian", "Locrian", "Pentatonic Minor", "Pentatonic Major",
                                    "Blues", "Diminished Half", "Diminished Whole", "Whole Tone",
                                    "Melodic Minor", "Harmonic Minor", "Super Locrian", "Lydian b7",
                                    "Phrygian Major" };
const ChoiceList kScale { kScaleItems, 18 };

const char* const kReverbShapeItems[] = { "Round", "Curved", "Square" };
const ChoiceList kReverbShape { kReverbShapeItems, 3 };

const char* const kReverbSizeItems[] = { "Box", "Tiny", "Small", "Medium", "Large", "XL", "Grand",
                                         "Huge" };
const ChoiceList kReverbSize { kReverbSizeItems, 8 };

const char* const kHiColorItems[] = { "Wool", "Warm", "Real", "Clear", "Bright", "Crisp", "Glass" };
const ChoiceList kHiColor { kHiColorItems, 7 };

const char* const kLoColorItems[] = { "Thick", "Round", "Real", "Light", "Tight", "Thin", "NoBass" };
const ChoiceList kLoColor { kLoColorItems, 7 };

#define KNOB(rawType, member, fieldName, displayName, mn, mx, df, kindValue, unitValue, choicePtr) \
    { fieldName, displayName, mn, mx, df, kindValue, unitValue, choicePtr,                         \
      (uint32_t) offsetof (rawType, member) }

const KnobDef kEq[] = {
    KNOB (gm2_eq_setup_raw, band[0].freq, "freq_1", "Freq 1", 25, 241, 60, KnobKind::Int, KnobUnit::EqFreq, nullptr),
    KNOB (gm2_eq_setup_raw, band[0].gain, "gain_1", "Gain 1", -12, 12, 6, KnobKind::Int, KnobUnit::Decibels, nullptr),
    KNOB (gm2_eq_setup_raw, band[0].width, "width_1", "Width 1", 3, 16, 10, KnobKind::Int, KnobUnit::EqWidth, nullptr),
    KNOB (gm2_eq_setup_raw, band[1].freq, "freq_2", "Freq 2", 25, 241, 100, KnobKind::Int, KnobUnit::EqFreq, nullptr),
    KNOB (gm2_eq_setup_raw, band[1].gain, "gain_2", "Gain 2", -12, 12, -6, KnobKind::Int, KnobUnit::Decibels, nullptr),
    KNOB (gm2_eq_setup_raw, band[1].width, "width_2", "Width 2", 3, 16, 6, KnobKind::Int, KnobUnit::EqWidth, nullptr),
    KNOB (gm2_eq_setup_raw, band[2].freq, "freq_3", "Freq 3", 25, 241, 180, KnobKind::Int, KnobUnit::EqFreq, nullptr),
    KNOB (gm2_eq_setup_raw, band[2].gain, "gain_3", "Gain 3", -12, 12, 3, KnobKind::Int, KnobUnit::Decibels, nullptr),
    KNOB (gm2_eq_setup_raw, band[2].width, "width_3", "Width 3", 3, 16, 4, KnobKind::Int, KnobUnit::EqWidth, nullptr),
};

const KnobDef kGate[] = {
    KNOB (gm2_gate_setup_raw, mode, "mode", "Mode", 0, 1, 0, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_gate_setup_raw, threshold, "threshold", "Threshold", -61, 0, -20, KnobKind::Int, KnobUnit::GateThreshold, nullptr),
    KNOB (gm2_gate_setup_raw, damping, "damping", "Damping", 0, 90, 90, KnobKind::Int, KnobUnit::Decibels, nullptr),
    KNOB (gm2_gate_setup_raw, release, "release", "Release", 3, 200, 45, KnobKind::Int, KnobUnit::DecibelsPerSecond, nullptr),
};

const KnobDef kWahWah[] = {
    KNOB (gm2_wahwah_setup_raw, sweep, "sweep", "Sweep", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_wahwah_setup_raw, range, "range", "Range", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kLowHigh),
    KNOB (gm2_wahwah_setup_raw, mix, "mix", "Mix", 0, 100, 100, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_wahwah_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kTouchWah[] = {
    KNOB (gm2_touchwah_setup_raw, sense, "sense", "Sense", 0, 10, 5, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_touchwah_setup_raw, response, "response", "Response", 0, 2, 2, KnobKind::Choice, KnobUnit::Plain, &kResponse),
    KNOB (gm2_touchwah_setup_raw, range, "range", "Range", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kLowHigh),
    KNOB (gm2_touchwah_setup_raw, mix, "mix", "Mix", 0, 100, 100, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_touchwah_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kAutoWah[] = {
    KNOB (gm2_autowah_setup_raw, speed, "speed", "Speed", 0, 208, 167, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_autowah_setup_raw, tempo, "tempo", "Tempo", 0, 16, 9, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_autowah_setup_raw, shape, "shape", "Shape", 0, 1, 1, KnobKind::Choice, KnobUnit::Plain, &kTriangleSine),
    KNOB (gm2_autowah_setup_raw, range, "range", "Range", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kLowHigh),
    KNOB (gm2_autowah_setup_raw, mix, "mix", "Mix", 0, 100, 100, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_autowah_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kResFilter[] = {
    KNOB (gm2_resfilter_setup_raw, style, "style", "Style", 0, 1, 1, KnobKind::Choice, KnobUnit::Plain, &kStyle),
    KNOB (gm2_resfilter_setup_raw, hi_cut, "hi_cut", "Hi-Cut", 18, 57, 37, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_resfilter_setup_raw, resonance, "resonance", "Resonance", 0, 100, 100, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_resfilter_setup_raw, mix, "mix", "Mix", 0, 100, 100, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_resfilter_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kTouchRes[] = {
    KNOB (gm2_touchres_setup_raw, style, "style", "Style", 0, 1, 1, KnobKind::Choice, KnobUnit::Plain, &kStyle),
    KNOB (gm2_touchres_setup_raw, sense, "sense", "Sense", 0, 10, 8, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_touchres_setup_raw, response, "response", "Response", 0, 2, 2, KnobKind::Choice, KnobUnit::Plain, &kResponse),
    KNOB (gm2_touchres_setup_raw, freq_max, "freq_max", "Freq Max", 34, 54, 54, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_touchres_setup_raw, mix, "mix", "Mix", 0, 100, 100, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_touchres_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kTremolo[] = {
    KNOB (gm2_tremolo_setup_raw, speed, "speed", "Speed", 0, 208, 167, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_tremolo_setup_raw, depth, "depth", "Depth", 0, 100, 100, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_tremolo_setup_raw, tempo, "tempo", "Tempo", 0, 16, 9, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_tremolo_setup_raw, type, "type", "Type", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kSoftHard),
    KNOB (gm2_tremolo_setup_raw, pulse_width, "pulse_width", "Pulse Width", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_tremolo_setup_raw, hi_cut, "hi_cut", "Hi-Cut", 0, 60, 58, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_tremolo_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kPanner[] = {
    KNOB (gm2_panner_setup_raw, speed, "speed", "Speed", 0, 208, 116, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_panner_setup_raw, width, "width", "Width", 0, 100, 100, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_panner_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_panner_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kComp[] = {
    KNOB (gm2_comp_setup_raw, threshold, "threshold", "Threshold", -40, 0, -20, KnobKind::Int, KnobUnit::Decibels, nullptr),
    KNOB (gm2_comp_setup_raw, ratio, "ratio", "Ratio", 0, 15, 11, KnobKind::Choice, KnobUnit::Plain, &kCompRatio),
    KNOB (gm2_comp_setup_raw, attack, "attack", "Attack", 3, 14, 5, KnobKind::Choice, KnobUnit::Plain, &kCompAttack),
    KNOB (gm2_comp_setup_raw, release, "release", "Release", 13, 23, 15, KnobKind::Choice, KnobUnit::Plain, &kCompRelease),
    KNOB (gm2_comp_setup_raw, gain, "gain", "Gain", -6, 6, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kDetune[] = {
    KNOB (gm2_detune_setup_raw, voice_l, "voice_l", "Voice L", -100, 100, 9, KnobKind::Int, KnobUnit::Cents, nullptr),
    KNOB (gm2_detune_setup_raw, voice_r, "voice_r", "Voice R", -100, 100, -9, KnobKind::Int, KnobUnit::Cents, nullptr),
    KNOB (gm2_detune_setup_raw, delay_l, "delay_l", "Delay L", 0, 50, 10, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_detune_setup_raw, delay_r, "delay_r", "Delay R", 0, 50, 12, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_detune_setup_raw, mix, "mix", "Mix", 0, 100, 25, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_detune_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kWhammy[] = {
    KNOB (gm2_whammy_setup_raw, pitch, "pitch", "Pitch", 0, 100, 0, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_whammy_setup_raw, direction, "direction", "Direction", 0, 1, 1, KnobKind::Choice, KnobUnit::Plain, &kDownUp),
    KNOB (gm2_whammy_setup_raw, range, "range", "Range", 1, 2, 2, KnobKind::Choice, KnobUnit::Plain, &kOctaveRange),
    KNOB (gm2_whammy_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kOctaver[] = {
    KNOB (gm2_octaver_setup_raw, direction, "direction", "Direction", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kDownUp),
    KNOB (gm2_octaver_setup_raw, range, "range", "Range", 1, 2, 1, KnobKind::Choice, KnobUnit::Plain, &kOctaveRange),
    KNOB (gm2_octaver_setup_raw, mix, "mix", "Mix", 0, 100, 20, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_octaver_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kPitch[] = {
    KNOB (gm2_pitch_setup_raw, voice_1, "voice_1", "Voice 1", -2400, 2400, 10, KnobKind::Int, KnobUnit::Cents, nullptr),
    KNOB (gm2_pitch_setup_raw, voice_2, "voice_2", "Voice 2", -2400, 2400, -15, KnobKind::Int, KnobUnit::Cents, nullptr),
    KNOB (gm2_pitch_setup_raw, pan_1, "pan_1", "Pan 1", -50, 50, -50, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_pitch_setup_raw, pan_2, "pan_2", "Pan 2", -50, 50, 50, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_pitch_setup_raw, delay_1, "delay_1", "Delay 1", 0, 350, 20, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_pitch_setup_raw, delay_2, "delay_2", "Delay 2", 0, 350, 10, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_pitch_setup_raw, feedback_1, "feedback_1", "Feedback 1", 0, 100, 0, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_pitch_setup_raw, feedback_2, "feedback_2", "Feedback 2", 0, 100, 0, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_pitch_setup_raw, level_1, "level_1", "Level 1", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
    KNOB (gm2_pitch_setup_raw, level_2, "level_2", "Level 2", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
    KNOB (gm2_pitch_setup_raw, mix, "mix", "Mix", 0, 100, 25, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_pitch_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kIPitch[] = {
    KNOB (gm2_ipitch_setup_raw, voice_1, "voice_1", "Voice 1", -12, 12, 10, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_ipitch_setup_raw, voice_2, "voice_2", "Voice 2", -12, 12, -12, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_ipitch_setup_raw, pan_1, "pan_1", "Pan 1", -50, 50, -50, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_ipitch_setup_raw, pan_2, "pan_2", "Pan 2", -50, 50, 50, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_ipitch_setup_raw, delay_1, "delay_1", "Delay 1", 0, 350, 20, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_ipitch_setup_raw, delay_2, "delay_2", "Delay 2", 0, 350, 10, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_ipitch_setup_raw, key, "key", "Key", 0, 11, 0, KnobKind::Choice, KnobUnit::Plain, &kKey),
    KNOB (gm2_ipitch_setup_raw, scale, "scale", "Scale", 0, 17, 0, KnobKind::Choice, KnobUnit::Plain, &kScale),
    KNOB (gm2_ipitch_setup_raw, level_1, "level_1", "Level 1", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
    KNOB (gm2_ipitch_setup_raw, level_2, "level_2", "Level 2", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
    KNOB (gm2_ipitch_setup_raw, mix, "mix", "Mix", 0, 100, 25, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_ipitch_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kCChorus[] = {
    KNOB (gm2_cchorus_setup_raw, speed, "speed", "Speed", 0, 208, 31, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_cchorus_setup_raw, depth, "depth", "Depth", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_cchorus_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_cchorus_setup_raw, hi_cut, "hi_cut", "Hi-Cut", 0, 60, 58, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_cchorus_setup_raw, mix, "mix", "Mix", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_cchorus_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kAChorus[] = {
    KNOB (gm2_achorus_setup_raw, speed, "speed", "Speed", 0, 208, 31, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_achorus_setup_raw, depth, "depth", "Depth", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_achorus_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_achorus_setup_raw, hi_cut, "hi_cut", "Hi-Cut", 0, 60, 58, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_achorus_setup_raw, delay, "delay", "Delay", 1, 500, 140, KnobKind::Int, KnobUnit::AdvDelayMs, nullptr),
    KNOB (gm2_achorus_setup_raw, golden_ratio, "golden_ratio", "Golden Ratio", 0, 1, 1, KnobKind::Choice, KnobUnit::Plain, &kOffOn),
    KNOB (gm2_achorus_setup_raw, phase_reverse, "phase_reverse", "Phase Reverse", 0, 1, 1, KnobKind::Choice, KnobUnit::Plain, &kOffOn),
    KNOB (gm2_achorus_setup_raw, mix, "mix", "Mix", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_achorus_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kTriChorus[] = {
    KNOB (gm2_trichorus_setup_raw, speed, "speed", "Speed", 0, 208, 31, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_trichorus_setup_raw, depth, "depth", "Depth", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_trichorus_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_trichorus_setup_raw, width, "width", "Width", 1, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_trichorus_setup_raw, phase_reverse, "phase_reverse", "Phase Reverse", 0, 1, 1, KnobKind::Choice, KnobUnit::Plain, &kOffOn),
    KNOB (gm2_trichorus_setup_raw, mix, "mix", "Mix", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_trichorus_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kTriChorusAsym[] = {
    KNOB (gm2_trichorus_asym_setup_raw, speed, "speed", "Speed", 0, 208, 31, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_trichorus_asym_setup_raw, depth, "depth", "Depth", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_trichorus_asym_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_trichorus_asym_setup_raw, width, "width", "Width", 1, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_trichorus_asym_setup_raw, phase_reverse, "phase_reverse", "Phase Reverse", 0, 1, 1, KnobKind::Choice, KnobUnit::Plain, &kOffOn),
    KNOB (gm2_trichorus_asym_setup_raw, mix, "mix", "Mix", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_trichorus_asym_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kCFlanger[] = {
    KNOB (gm2_cflanger_setup_raw, speed, "speed", "Speed", 0, 208, 24, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_cflanger_setup_raw, depth, "depth", "Depth", 0, 100, 25, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_cflanger_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_cflanger_setup_raw, hi_cut, "hi_cut", "Hi-Cut", 0, 60, 58, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_cflanger_setup_raw, feedback, "feedback", "Feedback", -100, 100, -80, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_cflanger_setup_raw, fb_cut, "fb_cut", "Feedback Hi-Cut", 0, 60, 54, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_cflanger_setup_raw, mix, "mix", "Mix", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_cflanger_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kAFlanger[] = {
    KNOB (gm2_aflanger_setup_raw, speed, "speed", "Speed", 0, 208, 17, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_aflanger_setup_raw, depth, "depth", "Depth", 0, 100, 20, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_aflanger_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_aflanger_setup_raw, hi_cut, "hi_cut", "Hi-Cut", 0, 60, 58, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_aflanger_setup_raw, feedback, "feedback", "Feedback", -100, 100, -80, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_aflanger_setup_raw, fb_cut, "fb_cut", "Feedback Hi-Cut", 0, 60, 54, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_aflanger_setup_raw, delay, "delay", "Delay", 1, 500, 45, KnobKind::Int, KnobUnit::AdvDelayMs, nullptr),
    KNOB (gm2_aflanger_setup_raw, golden_ratio, "golden_ratio", "Golden Ratio", 0, 1, 1, KnobKind::Choice, KnobUnit::Plain, &kOffOn),
    KNOB (gm2_aflanger_setup_raw, phase_reverse, "phase_reverse", "Phase Reverse", 0, 1, 1, KnobKind::Choice, KnobUnit::Plain, &kOffOn),
    KNOB (gm2_aflanger_setup_raw, mix, "mix", "Mix", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_aflanger_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kTzFlangerNeg[] = {
    KNOB (gm2_tzflanger_neg_setup_raw, speed, "speed", "Speed", 0, 208, 17, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_tzflanger_neg_setup_raw, depth, "depth", "Depth", 0, 100, 20, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_tzflanger_neg_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_tzflanger_neg_setup_raw, feedback, "feedback", "Feedback", -100, 100, -80, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_tzflanger_neg_setup_raw, fb_cut, "fb_cut", "Feedback Hi-Cut", 0, 60, 45, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_tzflanger_neg_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kTzFlangerPos[] = {
    KNOB (gm2_tzflanger_pos_setup_raw, speed, "speed", "Speed", 0, 208, 17, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_tzflanger_pos_setup_raw, depth, "depth", "Depth", 0, 100, 20, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_tzflanger_pos_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_tzflanger_pos_setup_raw, feedback, "feedback", "Feedback", -100, 100, -80, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_tzflanger_pos_setup_raw, fb_cut, "fb_cut", "Feedback Hi-Cut", 0, 60, 45, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_tzflanger_pos_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kVibrato[] = {
    KNOB (gm2_vibrato_setup_raw, speed, "speed", "Speed", 0, 208, 152, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_vibrato_setup_raw, depth, "depth", "Depth", 0, 100, 7, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_vibrato_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_vibrato_setup_raw, hi_cut, "hi_cut", "Hi-Cut", 0, 60, 58, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_vibrato_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kVintPhaser[] = {
    KNOB (gm2_vintphaser_setup_raw, speed, "speed", "Speed", 0, 208, 56, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_vintphaser_setup_raw, depth, "depth", "Depth", 0, 100, 90, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_vintphaser_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_vintphaser_setup_raw, feedback, "feedback", "Feedback", -100, 100, -90, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_vintphaser_setup_raw, range, "range", "Range", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kLowHigh),
    KNOB (gm2_vintphaser_setup_raw, phase_reverse, "phase_reverse", "Phase Reverse", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kOffOn),
    KNOB (gm2_vintphaser_setup_raw, mix, "mix", "Mix", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_vintphaser_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kSmoothPhaser[] = {
    KNOB (gm2_smoothphaser_setup_raw, speed, "speed", "Speed", 0, 208, 38, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_smoothphaser_setup_raw, depth, "depth", "Depth", 0, 100, 80, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_smoothphaser_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_smoothphaser_setup_raw, feedback, "feedback", "Feedback", -100, 100, -90, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_smoothphaser_setup_raw, range, "range", "Range", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kLowHigh),
    KNOB (gm2_smoothphaser_setup_raw, phase_reverse, "phase_reverse", "Phase Reverse", 0, 1, 1, KnobKind::Choice, KnobUnit::Plain, &kOffOn),
    KNOB (gm2_smoothphaser_setup_raw, mix, "mix", "Mix", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_smoothphaser_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kVintUnivibe[] = {
    KNOB (gm2_vintunivibe_setup_raw, speed_1, "speed_1", "Speed 1", 37, 184, 128, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_vintunivibe_setup_raw, speed_2, "speed_2", "Speed 2", 37, 184, 37, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_vintunivibe_setup_raw, depth, "depth", "Depth", 0, 100, 90, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_vintunivibe_setup_raw, tempo_1, "tempo_1", "Tempo 1", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_vintunivibe_setup_raw, tempo_2, "tempo_2", "Tempo 2", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_vintunivibe_setup_raw, attack, "attack", "Attack", 1, 100, 1, KnobKind::Int, KnobUnit::UnivibeSeconds, nullptr),
    KNOB (gm2_vintunivibe_setup_raw, release, "release", "Release", 1, 100, 1, KnobKind::Int, KnobUnit::UnivibeSeconds, nullptr),
    KNOB (gm2_vintunivibe_setup_raw, select, "select", "Select", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kUnivibeSelect),
    KNOB (gm2_vintunivibe_setup_raw, phase_reverse, "phase_reverse", "Phase Reverse", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kOffOn),
    KNOB (gm2_vintunivibe_setup_raw, mix, "mix", "Mix", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_vintunivibe_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kModUnivibe[] = {
    KNOB (gm2_modunivibe_setup_raw, speed_1, "speed_1", "Speed 1", 37, 184, 128, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_modunivibe_setup_raw, speed_2, "speed_2", "Speed 2", 37, 184, 37, KnobKind::Int, KnobUnit::Speed, nullptr),
    KNOB (gm2_modunivibe_setup_raw, depth, "depth", "Depth", 0, 100, 80, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_modunivibe_setup_raw, tempo_1, "tempo_1", "Tempo 1", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_modunivibe_setup_raw, tempo_2, "tempo_2", "Tempo 2", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_modunivibe_setup_raw, attack, "attack", "Attack", 1, 100, 1, KnobKind::Int, KnobUnit::UnivibeSeconds, nullptr),
    KNOB (gm2_modunivibe_setup_raw, release, "release", "Release", 1, 100, 1, KnobKind::Int, KnobUnit::UnivibeSeconds, nullptr),
    KNOB (gm2_modunivibe_setup_raw, select, "select", "Select", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kUnivibeSelect),
    KNOB (gm2_modunivibe_setup_raw, phase_reverse, "phase_reverse", "Phase Reverse", 0, 1, 0, KnobKind::Choice, KnobUnit::Plain, &kOffOn),
    KNOB (gm2_modunivibe_setup_raw, mix, "mix", "Mix", 0, 100, 50, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_modunivibe_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kPingPong[] = {
    KNOB (gm2_pingpong_setup_raw, delay, "delay", "Delay", 0, 1800, 375, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_pingpong_setup_raw, tempo, "tempo", "Tempo", 0, 16, 8, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_pingpong_setup_raw, width, "width", "Width", 0, 100, 75, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_pingpong_setup_raw, feedback, "feedback", "Feedback", 0, 100, 45, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_pingpong_setup_raw, fb_hi_cut, "fb_hi_cut", "Feedback Hi-Cut", 40, 60, 54, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_pingpong_setup_raw, fb_lo_cut, "fb_lo_cut", "Feedback Lo-Cut", 0, 40, 10, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_pingpong_setup_raw, mix, "mix", "Mix", 0, 100, 15, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_pingpong_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kDynDelay[] = {
    KNOB (gm2_dyndelay_setup_raw, delay, "delay", "Delay", 0, 1800, 300, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_dyndelay_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_dyndelay_setup_raw, feedback, "feedback", "Feedback", 0, 100, 35, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_dyndelay_setup_raw, fb_hi_cut, "fb_hi_cut", "Feedback Hi-Cut", 40, 60, 55, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_dyndelay_setup_raw, fb_lo_cut, "fb_lo_cut", "Feedback Lo-Cut", 0, 40, 8, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_dyndelay_setup_raw, offset, "offset", "Offset", 0, 200, 25, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_dyndelay_setup_raw, sense, "sense", "Sense", -50, 0, -25, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_dyndelay_setup_raw, damp, "damp", "Damp", 0, 100, 18, KnobKind::Int, KnobUnit::Decibels, nullptr),
    KNOB (gm2_dyndelay_setup_raw, release, "release", "Release", 11, 21, 14, KnobKind::Choice, KnobUnit::Plain, &kDynRelease),
    KNOB (gm2_dyndelay_setup_raw, mix, "mix", "Mix", 0, 100, 15, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_dyndelay_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kDualDelay[] = {
    KNOB (gm2_dualdelay_setup_raw, delay_1, "delay_1", "Delay 1", 0, 1800, 280, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_dualdelay_setup_raw, delay_2, "delay_2", "Delay 2", 0, 1800, 420, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_dualdelay_setup_raw, tempo_1, "tempo_1", "Tempo 1", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_dualdelay_setup_raw, tempo_2, "tempo_2", "Tempo 2", 0, 16, 9, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_dualdelay_setup_raw, feedback_1, "feedback_1", "Feedback 1", 0, 100, 35, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_dualdelay_setup_raw, feedback_2, "feedback_2", "Feedback 2", 0, 100, 45, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_dualdelay_setup_raw, fb_hi_cut, "fb_hi_cut", "Feedback Hi-Cut", 40, 60, 53, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_dualdelay_setup_raw, fb_lo_cut, "fb_lo_cut", "Feedback Lo-Cut", 0, 40, 12, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_dualdelay_setup_raw, pan_1, "pan_1", "Pan 1", -50, 50, -30, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_dualdelay_setup_raw, pan_2, "pan_2", "Pan 2", -50, 50, 35, KnobKind::Int, KnobUnit::Plain, nullptr),
    KNOB (gm2_dualdelay_setup_raw, mix, "mix", "Mix", 0, 100, 15, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_dualdelay_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

const KnobDef kRevDelay[] = {
    KNOB (gm2_revdelay_setup_raw, delay, "delay", "Delay", 50, 1000, 160, KnobKind::Int, KnobUnit::Milliseconds, nullptr),
    KNOB (gm2_revdelay_setup_raw, tempo, "tempo", "Tempo", 0, 16, 0, KnobKind::Choice, KnobUnit::Plain, &kTempo),
    KNOB (gm2_revdelay_setup_raw, feedback, "feedback", "Feedback", 0, 100, 30, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_revdelay_setup_raw, fb_hi_cut, "fb_hi_cut", "Feedback Hi-Cut", 40, 60, 54, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_revdelay_setup_raw, fb_lo_cut, "fb_lo_cut", "Feedback Lo-Cut", 0, 40, 10, KnobKind::Int, KnobUnit::Cut, nullptr),
    KNOB (gm2_revdelay_setup_raw, mix, "mix", "Mix", 0, 100, 15, KnobKind::Int, KnobUnit::Percent, nullptr),
    KNOB (gm2_revdelay_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr),
};

#define REVERB_KNOBS(decayDefault, preDelayDefault, shapeDefault, sizeDefault, hiColorDefault, \
                     hiFactorDefault, loColorDefault, loFactorDefault, roomDefault,           \
                     reverbDefault, diffuseDefault, mixDefault)                               \
    KNOB (gm2_reverb_setup_raw, decay, "decay", "Decay", 1, 200, decayDefault, KnobKind::Int, KnobUnit::ReverbDecay, nullptr), \
    KNOB (gm2_reverb_setup_raw, pre_delay, "pre_delay", "Pre-Delay", 0, 100, preDelayDefault, KnobKind::Int, KnobUnit::Milliseconds, nullptr), \
    KNOB (gm2_reverb_setup_raw, shape, "shape", "Shape", 0, 2, shapeDefault, KnobKind::Choice, KnobUnit::Plain, &kReverbShape), \
    KNOB (gm2_reverb_setup_raw, size, "size", "Size", 0, 7, sizeDefault, KnobKind::Choice, KnobUnit::Plain, &kReverbSize), \
    KNOB (gm2_reverb_setup_raw, hi_color, "hi_color", "Hi Color", 0, 6, hiColorDefault, KnobKind::Choice, KnobUnit::Plain, &kHiColor), \
    KNOB (gm2_reverb_setup_raw, hi_factor, "hi_factor", "Hi Factor", -25, 25, hiFactorDefault, KnobKind::Int, KnobUnit::Plain, nullptr), \
    KNOB (gm2_reverb_setup_raw, lo_color, "lo_color", "Lo Color", 0, 6, loColorDefault, KnobKind::Choice, KnobUnit::Plain, &kLoColor), \
    KNOB (gm2_reverb_setup_raw, lo_factor, "lo_factor", "Lo Factor", -25, 25, loFactorDefault, KnobKind::Int, KnobUnit::Plain, nullptr), \
    KNOB (gm2_reverb_setup_raw, room_level, "room_level", "Room Level", -100, 0, roomDefault, KnobKind::Int, KnobUnit::Decibels, nullptr), \
    KNOB (gm2_reverb_setup_raw, reverb_level, "reverb_level", "Reverb Level", -100, 0, reverbDefault, KnobKind::Int, KnobUnit::Decibels, nullptr), \
    KNOB (gm2_reverb_setup_raw, diffuse, "diffuse", "Diffuse", -25, 25, diffuseDefault, KnobKind::Int, KnobUnit::Plain, nullptr), \
    KNOB (gm2_reverb_setup_raw, mix, "mix", "Mix", 0, 100, mixDefault, KnobKind::Int, KnobUnit::Percent, nullptr), \
    KNOB (gm2_reverb_setup_raw, out_level, "out_level", "Out Level", -100, 0, 0, KnobKind::Int, KnobUnit::Decibels, nullptr)

const KnobDef kReverbSpring[] = { REVERB_KNOBS (20, 0, 0, 3, 4, 6, 3, 0, 0, -3, 10, 20) };
const KnobDef kReverbHall[] = { REVERB_KNOBS (25, 50, 0, 4, 3, 0, 4, 9, 0, -6, 0, 25) };
const KnobDef kReverbRoom[] = { REVERB_KNOBS (12, 40, 0, 3, 2, 0, 4, 9, 0, -6, 0, 25) };
const KnobDef kReverbPlate[] = { REVERB_KNOBS (15, 5, 1, 3, 1, 5, 5, 6, 0, -6, 10, 16) };

template <size_t N>
constexpr int knobCount (const KnobDef (&)[N])
{
    static_assert (N <= (size_t) kMaxKnobs, "effect exceeds the processor's knob capacity");
    return (int) N;
}

#define EFFECT(keyValue, nameValue, familyValue, table) \
    { keyValue, nameValue, familyValue, table, knobCount (table) }

const EffectInfo kEffects[] = {
    EFFECT ("eq", "Three-Band EQ", "Gate & EQ", kEq),
    EFFECT ("gate", "Noise Gate", "Gate & EQ", kGate),
    EFFECT ("wahwah", "Wah-Wah", "Filter", kWahWah),
    EFFECT ("touchwah", "Touch Wah", "Filter", kTouchWah),
    EFFECT ("autowah", "Auto Wah", "Filter", kAutoWah),
    EFFECT ("resfilter", "Resonance Filter", "Filter", kResFilter),
    EFFECT ("touchres", "Touch Resonance Filter", "Filter", kTouchRes),
    EFFECT ("tremolo", "Tremolo", "Filter", kTremolo),
    EFFECT ("panner", "Panner", "Filter", kPanner),
    EFFECT ("comp", "Compressor", "Compressor", kComp),
    EFFECT ("detune", "Detune", "Pitch", kDetune),
    EFFECT ("whammy", "Whammy", "Pitch", kWhammy),
    EFFECT ("octaver", "Octaver", "Pitch", kOctaver),
    EFFECT ("pitch", "Pitch Shifter", "Pitch", kPitch),
    EFFECT ("ipitch", "Intelligent Pitch Shifter", "Pitch", kIPitch),
    EFFECT ("cchorus", "Classic Chorus", "Modulation", kCChorus),
    EFFECT ("achorus", "Advanced Chorus", "Modulation", kAChorus),
    EFFECT ("trichorus", "Tri-Chorus", "Modulation", kTriChorus),
    EFFECT ("trichorus_asym", "Tri-Chorus Asymmetric", "Modulation", kTriChorusAsym),
    EFFECT ("cflanger", "Classic Flanger", "Modulation", kCFlanger),
    EFFECT ("aflanger", "Advanced Flanger", "Modulation", kAFlanger),
    EFFECT ("tzflanger_neg", "Through-Zero Flanger Negative", "Modulation", kTzFlangerNeg),
    EFFECT ("tzflanger_pos", "Through-Zero Flanger Positive", "Modulation", kTzFlangerPos),
    EFFECT ("vibrato", "Vibrato", "Modulation", kVibrato),
    EFFECT ("vintphaser", "Vintage Phaser", "Modulation", kVintPhaser),
    EFFECT ("smoothphaser", "Smooth Phaser", "Modulation", kSmoothPhaser),
    EFFECT ("vintunivibe", "Vintage Univibe", "Modulation", kVintUnivibe),
    EFFECT ("modunivibe", "Modern Univibe", "Modulation", kModUnivibe),
    EFFECT ("pingpong", "Ping Pong Delay", "Delay", kPingPong),
    EFFECT ("dyndelay", "Dynamic Delay", "Delay", kDynDelay),
    EFFECT ("dualdelay", "Dual Delay", "Delay", kDualDelay),
    EFFECT ("revdelay", "Reverse Delay", "Delay", kRevDelay),
    EFFECT ("reverb_spring", "Spring Reverb", "Reverb", kReverbSpring),
    EFFECT ("reverb_hall", "Hall Reverb", "Reverb", kReverbHall),
    EFFECT ("reverb_room", "Room Reverb", "Reverb", kReverbRoom),
    EFFECT ("reverb_plate", "Plate Reverb", "Reverb", kReverbPlate),
};

#undef EFFECT

constexpr int kEffectCount = (int) (sizeof (kEffects) / sizeof (kEffects[0]));

#define GM2_TRAITS_MEMBERS(Traits, module)                                                         \
    using Raw = gm2_##module##_setup_raw;                                                          \
    using Config = gm2_##module##_config;                                                          \
    using State = gm2_##module##_state;                                                            \
    static int setup (const gm2_context* context, const Raw* raw, Config* config)                  \
    {                                                                                              \
        return gm2_##module##_setup_config (context, raw, config);                                 \
    }                                                                                              \
    static void init (State* state, const Config* config)                                          \
    {                                                                                              \
        gm2_##module##_state_init (state, config);                                                 \
    }                                                                                              \
    static void process (State* state, const Config* config, const float* inL, const float* inR,  \
                         float* outL, float* outR, size_t frames)                                  \
    {                                                                                              \
        gm2_##module##_process_f32 (state, config, inL, inR, outL, outR, frames);                  \
    }

#define GM2_TRAITS(Traits, module)                                                                 \
    struct Traits                                                                                  \
    {                                                                                              \
        GM2_TRAITS_MEMBERS (Traits, module)                                                        \
        static constexpr bool takesBeat = false;                                                   \
        static void align (State*, const Config*, double) {}                                       \
    }

#define GM2_TRAITS_BEAT(Traits, module)                                                            \
    struct Traits                                                                                  \
    {                                                                                              \
        GM2_TRAITS_MEMBERS (Traits, module)                                                        \
        static constexpr bool takesBeat = true;                                                    \
        static constexpr size_t beatOffset = offsetof (Raw, beat_ms);                              \
        static void align (State*, const Config*, double) {}                                       \
    }

#define GM2_TRAITS_LFO_BEAT(Traits, module)                                                        \
    struct Traits                                                                                  \
    {                                                                                              \
        GM2_TRAITS_MEMBERS (Traits, module)                                                        \
        static constexpr bool takesBeat = true;                                                    \
        static constexpr size_t beatOffset = offsetof (Raw, beat_ms);                              \
        static void align (State* state, const Config* config, double beats)                       \
        {                                                                                          \
            gm2_##module##_align (state, config, beats);                                           \
        }                                                                                          \
    }

GM2_TRAITS (EqTraits, eq);
GM2_TRAITS (GateTraits, gate);
GM2_TRAITS (WahWahTraits, wahwah);
GM2_TRAITS (TouchWahTraits, touchwah);
GM2_TRAITS_LFO_BEAT (AutoWahTraits, autowah);
GM2_TRAITS (ResFilterTraits, resfilter);
GM2_TRAITS (TouchResTraits, touchres);
GM2_TRAITS_LFO_BEAT (TremoloTraits, tremolo);
GM2_TRAITS_LFO_BEAT (PannerTraits, panner);
GM2_TRAITS (CompTraits, comp);
GM2_TRAITS (DetuneTraits, detune);
GM2_TRAITS (WhammyTraits, whammy);
GM2_TRAITS (OctaverTraits, octaver);
GM2_TRAITS (PitchTraits, pitch);
GM2_TRAITS (IPitchTraits, ipitch);
GM2_TRAITS_LFO_BEAT (CChorusTraits, cchorus);
GM2_TRAITS_LFO_BEAT (AChorusTraits, achorus);
GM2_TRAITS_LFO_BEAT (TriChorusTraits, trichorus);
GM2_TRAITS_LFO_BEAT (TriChorusAsymTraits, trichorus_asym);
GM2_TRAITS_LFO_BEAT (CFlangerTraits, cflanger);
GM2_TRAITS_LFO_BEAT (AFlangerTraits, aflanger);
GM2_TRAITS_LFO_BEAT (TzFlangerNegTraits, tzflanger_neg);
GM2_TRAITS_LFO_BEAT (TzFlangerPosTraits, tzflanger_pos);
GM2_TRAITS_LFO_BEAT (VibratoTraits, vibrato);
GM2_TRAITS_LFO_BEAT (VintPhaserTraits, vintphaser);
GM2_TRAITS_LFO_BEAT (SmoothPhaserTraits, smoothphaser);
GM2_TRAITS_LFO_BEAT (VintUnivibeTraits, vintunivibe);
GM2_TRAITS_LFO_BEAT (ModUnivibeTraits, modunivibe);
GM2_TRAITS_BEAT (PingPongTraits, pingpong);
GM2_TRAITS_BEAT (DynDelayTraits, dyndelay);
GM2_TRAITS_BEAT (DualDelayTraits, dualdelay);
GM2_TRAITS_BEAT (RevDelayTraits, revdelay);
GM2_TRAITS (ReverbTraits, reverb);

#undef GM2_TRAITS_MEMBERS
#undef GM2_TRAITS
#undef GM2_TRAITS_BEAT
#undef GM2_TRAITS_LFO_BEAT
#undef REVERB_KNOBS
#undef KNOB
} // namespace

int getEffectCount()
{
    return kEffectCount;
}

const EffectInfo& getEffectInfo (int index)
{
    return kEffects[index < 0 ? 0 : (index >= kEffectCount ? kEffectCount - 1 : index)];
}

std::unique_ptr<IEngine> createEffectEngine (int index)
{
    switch (index)
    {
        case 0: return std::make_unique<Engine<EqTraits>> (kEffects[0]);
        case 1: return std::make_unique<Engine<GateTraits>> (kEffects[1]);
        case 2: return std::make_unique<Engine<WahWahTraits>> (kEffects[2]);
        case 3: return std::make_unique<Engine<TouchWahTraits>> (kEffects[3]);
        case 4: return std::make_unique<Engine<AutoWahTraits>> (kEffects[4]);
        case 5: return std::make_unique<Engine<ResFilterTraits>> (kEffects[5]);
        case 6: return std::make_unique<Engine<TouchResTraits>> (kEffects[6]);
        case 7: return std::make_unique<Engine<TremoloTraits>> (kEffects[7]);
        case 8: return std::make_unique<Engine<PannerTraits>> (kEffects[8]);
        case 9: return std::make_unique<Engine<CompTraits>> (kEffects[9]);
        case 10: return std::make_unique<Engine<DetuneTraits>> (kEffects[10]);
        case 11: return std::make_unique<Engine<WhammyTraits>> (kEffects[11]);
        case 12: return std::make_unique<Engine<OctaverTraits>> (kEffects[12]);
        case 13: return std::make_unique<Engine<PitchTraits>> (kEffects[13]);
        case 14: return std::make_unique<Engine<IPitchTraits>> (kEffects[14]);
        case 15: return std::make_unique<Engine<CChorusTraits>> (kEffects[15]);
        case 16: return std::make_unique<Engine<AChorusTraits>> (kEffects[16]);
        case 17: return std::make_unique<Engine<TriChorusTraits>> (kEffects[17]);
        case 18: return std::make_unique<Engine<TriChorusAsymTraits>> (kEffects[18]);
        case 19: return std::make_unique<Engine<CFlangerTraits>> (kEffects[19]);
        case 20: return std::make_unique<Engine<AFlangerTraits>> (kEffects[20]);
        case 21: return std::make_unique<Engine<TzFlangerNegTraits>> (kEffects[21]);
        case 22: return std::make_unique<Engine<TzFlangerPosTraits>> (kEffects[22]);
        case 23: return std::make_unique<Engine<VibratoTraits>> (kEffects[23]);
        case 24: return std::make_unique<Engine<VintPhaserTraits>> (kEffects[24]);
        case 25: return std::make_unique<Engine<SmoothPhaserTraits>> (kEffects[25]);
        case 26: return std::make_unique<Engine<VintUnivibeTraits>> (kEffects[26]);
        case 27: return std::make_unique<Engine<ModUnivibeTraits>> (kEffects[27]);
        case 28: return std::make_unique<Engine<PingPongTraits>> (kEffects[28]);
        case 29: return std::make_unique<Engine<DynDelayTraits>> (kEffects[29]);
        case 30: return std::make_unique<Engine<DualDelayTraits>> (kEffects[30]);
        case 31: return std::make_unique<Engine<RevDelayTraits>> (kEffects[31]);
        case 32: return std::make_unique<Engine<ReverbTraits>> (kEffects[32]);
        case 33: return std::make_unique<Engine<ReverbTraits>> (kEffects[33]);
        case 34: return std::make_unique<Engine<ReverbTraits>> (kEffects[34]);
        case 35: return std::make_unique<Engine<ReverbTraits>> (kEffects[35]);
        default: break;
    }

    return {};
}
