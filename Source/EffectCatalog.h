#pragma once

#include <cstdint>
#include <memory>
#include <string>

class IEngine;

enum class KnobKind : uint8_t
{
    Int,
    Choice
};

enum class KnobUnit : uint8_t
{
    Plain,
    Percent,
    Decibels,
    Milliseconds,
    Cents,
    DecibelsPerSecond,
    Speed,
    Cut,
    EqFreq,
    EqWidth,
    AdvDelayMs,
    ReverbDecay,
    UnivibeSeconds,
    GateThreshold
};

struct ChoiceList
{
    const char* const* items;
    int count;
};

struct KnobDef
{
    const char* field;
    const char* name;
    int min;
    int max;
    int def;
    KnobKind kind;
    KnobUnit unit;
    const ChoiceList* choices;
    uint32_t rawOffset;
};

struct EffectInfo
{
    const char* key;
    const char* name;
    const char* family;
    const KnobDef* knobs;
    int knobCount;
    bool depop; // the library has a de-popped variant of it, gm2_X_alt
};

constexpr int kMaxKnobs = 16;

int getEffectCount();
const EffectInfo& getEffectInfo (int index);
std::unique_ptr<IEngine> createEffectEngine (int index);
std::unique_ptr<IEngine> createDepopEngine (int index); // null when the effect has no variant

// Display text of an Int knob's raw value, and the raw value nearest to a typed text.
std::string formatKnobValue (const KnobDef& knob, int raw);
int parseKnobValue (const KnobDef& knob, const char* text);
