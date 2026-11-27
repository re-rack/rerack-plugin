#pragma once

#include <cstdint>
#include <memory>
#include <string>

class IEngine;

enum Unit : int
{
    kUnitGMajor2 = 0,
    kUnitIntellifex,
    kUnitCount
};

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
    GateThreshold,
    IfxLevel6,
    IfxLevel0,
    IfxThreshold,
    IfxHushRelease,
    IfxDuckerRelease,
    IfxPan,
    IfxPitch,
    IfxFine,
    IfxVoiceDelay,
    IfxDelayTime
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
    int8_t link = -1; // knob of the same effect whose value changes this one's text
};

struct PageInfo
{
    const char* name;
    const int* knobs; // indices into EffectInfo::knobs
    int knobCount;
};

struct EffectInfo
{
    const char* key;
    const char* name;
    const char* family; // null lists the effect outside any family submenu
    const KnobDef* knobs;
    int knobCount;
    bool depop; // the library has a de-popped variant of it, gm2_X_alt
    const PageInfo* pages = nullptr; // null shows every knob at once
    int pageCount = 0;
};

constexpr int kMaxKnobs = 64;

const char* getUnitName (int unit);
int getEffectCount (int unit);
const EffectInfo& getEffectInfo (int unit, int index);
std::unique_ptr<IEngine> createEffectEngine (int unit, int index);
std::unique_ptr<IEngine> createDepopEngine (int unit, int index); // null when the effect has no variant

// Display text of an Int knob's raw value, and the raw value nearest to a typed text. `linked`
// is the raw value of the knob's link, when it has one.
std::string formatKnobValue (const KnobDef& knob, int raw, int linked = 0);
int parseKnobValue (const KnobDef& knob, const char* text, int linked = 0);
