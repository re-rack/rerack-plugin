#pragma once

#include "IPlug_include_in_plug_hdr.h"

#include "EffectEngine.h"
#include "RationalResampler.h"
#include "Resampler.h"

#include <array>
#include <memory>
#include <vector>

enum EParams
{
    kUnitParam = 0,
    kG2EffectParam,
    kIfxEffectParam,
    kDepopParam,
    kFirstKnobParam
};

class VintageDigital final : public iplug::Plugin
{
public:
    explicit VintageDigital (const iplug::InstanceInfo& info);

    void OnReset() override;
    void ProcessBlock (iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
    void OnParamChangeUI (int paramIdx, iplug::EParamSource source) override;

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API getParamValueByString (Steinberg::Vst::ParamID tag,
                                                         Steinberg::Vst::TChar* string,
                                                         Steinberg::Vst::ParamValue& valueNormalized) override;

private:
    struct KnobParam
    {
        const KnobDef* knob = nullptr; // null for the unit, effect and De-pop parameters
        int linkParam = -1;
        bool linkTarget = false; // another knob's text shows this one's value
    };

    int currentUnit() const;
    int currentEffect (int unit) const;
    int latencyFor (int unit) const;
    int linkedValue (int paramIdx) const;
    void layoutEditor (iplug::igraphics::IGraphics& graphics);
    void showEffect (iplug::igraphics::IGraphics& graphics);

    std::array<std::vector<int>, kUnitCount> knobBase; // parameter index of each effect's first knob
    std::array<std::vector<std::unique_ptr<IEngine>>, kUnitCount> engines;
    std::array<std::vector<std::unique_ptr<IEngine>>, kUnitCount> depopEngines; // null without a variant
    std::vector<KnobParam> paramKnob;
    std::vector<int> ifxPage; // the editor's page per Intellifex effect

    gm2_context context {};
    Resampler resampler;
    RationalResampler ifxResampler;
    double hostSampleRate = 0.0;
    int maxHostFrames = 0;
    int resampleFactor = 1;
    bool passThrough = false;

    const IEngine* currentEngine = nullptr;
    double lastPpq = 0.0;
    bool lastPpqValid = false;
    bool wasPlaying = false;
    bool alignPending = false;
};
