#pragma once

#include "IPlug_include_in_plug_hdr.h"

#include "EffectEngine.h"
#include "Resampler.h"

#include <memory>
#include <vector>

enum EParams
{
    kEffectParam = 0,
    kDepopParam,
    kFirstKnobParam
};

class IMajor2 final : public iplug::Plugin
{
public:
    explicit IMajor2 (const iplug::InstanceInfo& info);

    void OnReset() override;
    void ProcessBlock (iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
    void OnParamChangeUI (int paramIdx, iplug::EParamSource source) override;

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API getParamValueByString (Steinberg::Vst::ParamID tag,
                                                         Steinberg::Vst::TChar* string,
                                                         Steinberg::Vst::ParamValue& valueNormalized) override;

private:
    int currentEffect() const;
    void layoutEditor (iplug::igraphics::IGraphics& graphics);
    void showEffect (iplug::igraphics::IGraphics& graphics);

    std::vector<int> knobBase; // parameter index of each effect's first knob
    std::vector<const KnobDef*> paramKnob; // null for the effect and De-pop parameters

    std::vector<std::unique_ptr<IEngine>> engines;
    std::vector<std::unique_ptr<IEngine>> depopEngines;

    gm2_context context {};
    Resampler resampler;
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
