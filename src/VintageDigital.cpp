#include "VintageDigital.h"

#include "IPlug_include_in_plug_src.h"

#include <ifx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>

#if defined(__SSE__)
 #include <xmmintrin.h>
#endif

using namespace iplug;
using namespace igraphics;

static_assert (std::is_same_v<sample, float>, "the libraries process float; build with SAMPLE_TYPE_FLOAT");
static_assert (kIfxEffectParam == kG2EffectParam + kUnitIntellifex, "one effect parameter per unit, in unit order");

namespace
{
// Decaying feedback tails would otherwise crawl through slow denormal arithmetic.
class ScopedNoDenormals
{
public:
    ScopedNoDenormals() noexcept
    {
#if defined(__SSE__)
        saved = _mm_getcsr();
        _mm_setcsr (saved | 0x8040u); // FTZ | DAZ
#elif defined(__aarch64__)
        __asm__ __volatile__ ("mrs %0, fpcr" : "=r"(saved));
        __asm__ __volatile__ ("msr fpcr, %0" : : "r"(saved | (UINT64_C (1) << 24)));
#endif
    }

    ~ScopedNoDenormals()
    {
#if defined(__SSE__)
        _mm_setcsr (saved);
#elif defined(__aarch64__)
        __asm__ __volatile__ ("msr fpcr, %0" : : "r"(saved));
#endif
    }

    ScopedNoDenormals (const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator= (const ScopedNoDenormals&) = delete;

private:
#if defined(__aarch64__)
    uint64_t saved = 0;
#else
    unsigned saved = 0;
#endif
};

int parameterCount()
{
    int count = kFirstKnobParam;
    for (int unit = 0; unit < kUnitCount; ++unit)
        for (int effect = 0; effect < getEffectCount (unit); ++effect)
            count += getEffectInfo (unit, effect).knobCount;

    return count;
}
} // namespace

VintageDigital::VintageDigital (const InstanceInfo& info)
    : Plugin (info, MakeConfig (parameterCount(), 0))
{
    IParam* unitParam = GetParam (kUnitParam);
    unitParam->InitEnum ("Unit", kUnitGMajor2, kUnitCount);

    paramKnob.assign ((size_t) NParams(), KnobParam {});
    int param = kFirstKnobParam;

    for (int unit = 0; unit < kUnitCount; ++unit)
    {
        unitParam->SetDisplayText (unit, getUnitName (unit));

        const int count = getEffectCount (unit);
        IParam* effectParam = GetParam (kG2EffectParam + unit);
        effectParam->InitEnum ((std::string (getUnitName (unit)) + " Effect").c_str(), 0, count);

        knobBase[(size_t) unit].reserve ((size_t) count);
        engines[(size_t) unit].reserve ((size_t) count);
        depopEngines[(size_t) unit].reserve ((size_t) count);

        for (int effect = 0; effect < count; ++effect)
        {
            const auto& effectInfo = getEffectInfo (unit, effect);
            effectParam->SetDisplayText (effect, effectInfo.name);
            knobBase[(size_t) unit].push_back (param);
            engines[(size_t) unit].push_back (createEffectEngine (unit, effect));
            depopEngines[(size_t) unit].push_back (createDepopEngine (unit, effect));

            for (int knobIndex = 0; knobIndex < effectInfo.knobCount; ++knobIndex, ++param)
            {
                const KnobDef& knob = effectInfo.knobs[knobIndex];
                IParam* knobParam = GetParam (param);
                const int linkParam = knob.link >= 0 ? knobBase[(size_t) unit].back() + knob.link : -1;
                paramKnob[(size_t) param].knob = &knob;
                paramKnob[(size_t) param].linkParam = linkParam;
                if (linkParam >= 0)
                    paramKnob[(size_t) linkParam].linkTarget = true;

                if (knob.kind == KnobKind::Choice)
                {
                    knobParam->InitEnum (knob.name, knob.def - knob.min, knob.choices->count, "", 0,
                                         effectInfo.name);

                    for (int choice = 0; choice < knob.choices->count; ++choice)
                        knobParam->SetDisplayText (choice, knob.choices->items[choice]);
                }
                else
                {
                    knobParam->InitInt (knob.name, knob.def, knob.min, knob.max, "", 0, effectInfo.name);
                    knobParam->SetDisplayFunc ([this, &knob, linkParam] (double value, WDL_String& display) {
                        display.Set (formatKnobValue (knob, (int) std::lround (value), linkedValue (linkParam)).c_str());
                    });
                }
            }
        }
    }

    GetParam (kDepopParam)->InitBool ("De-pop", true, "", 0, "", "Off", "On");
    ifxPage.assign ((size_t) getEffectCount (kUnitIntellifex), 0);

    mMakeGraphicsFunc = [this] {
        return MakeGraphics (*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                             GetScaleForScreen (PLUG_WIDTH, PLUG_HEIGHT));
    };

    mLayoutFunc = [this] (IGraphics* graphics) { layoutEditor (*graphics); };
}

int VintageDigital::currentUnit() const
{
    return std::clamp ((int) std::lround (GetParam (kUnitParam)->Value()), 0, kUnitCount - 1);
}

int VintageDigital::currentEffect (int unit) const
{
    return std::clamp ((int) std::lround (GetParam (kG2EffectParam + unit)->Value()), 0,
                       getEffectCount (unit) - 1);
}

int VintageDigital::latencyFor (int unit) const
{
    if (unit == kUnitIntellifex)
        return ifxResampler.getLatencySamples();

    return resampleFactor > 1 ? resampler.getLatencySamples() : 0;
}

int VintageDigital::linkedValue (int paramIdx) const
{
    if (paramIdx < 0)
        return 0;

    const KnobDef& knob = *paramKnob[(size_t) paramIdx].knob;
    const int raw = (int) std::lround (GetParam (paramIdx)->Value());
    return knob.kind == KnobKind::Choice ? knob.min + raw : raw;
}

void VintageDigital::OnReset()
{
    const double rate = GetSampleRate() > 0.0 ? GetSampleRate() : 44100.0;
    const int frames = std::max (1, GetBlockSize());

    // VST3 hosts also reset through setProcessing (false), which must not allocate.
    if (rate != hostSampleRate || frames != maxHostFrames)
    {
        hostSampleRate = rate;
        maxHostFrames = frames;
        passThrough = true;
        resampleFactor = 0;

        for (int factor = 1; factor <= 64; factor <<= 1)
        {
            const auto baseRate = (unsigned) std::lround (hostSampleRate / (double) factor);
            if (baseRate == 0)
                continue;

            gm2_context candidate {};
            if (gm2_context_init (&candidate, baseRate) == 0)
            {
                context = candidate;
                resampleFactor = factor;
                passThrough = false;
                break;
            }
        }

        if (resampleFactor > 1)
            resampler.prepare (resampleFactor, (double) context.sample_rate, maxHostFrames);

        ifxResampler.prepare (hostSampleRate, IFX_DEVICE_RATE, maxHostFrames, (int) IFX_BLOCK_FRAMES);
    }
    else
    {
        resampler.reset();
        ifxResampler.reset();
    }

    const int latency = latencyFor (currentUnit());
    if (latency != GetLatency())
        SetLatency (latency);

    currentEngine = nullptr;
    lastPpqValid = false;
    wasPlaying = false;
    alignPending = false;
}

void VintageDigital::ProcessBlock (sample** inputs, sample** outputs, int nFrames)
{
    const ScopedNoDenormals noDenormals;

    if (nFrames <= 0)
        return;

    float* left = outputs[0];
    float* right = outputs[1];
    const size_t bytes = (size_t) nFrames * sizeof (float);

    if (NInChansConnected() == 0)
    {
        std::memset (left, 0, bytes);
        std::memset (right, 0, bytes);
        return;
    }

    std::memmove (left, inputs[0], bytes);
    std::memmove (right, IsChannelConnected (ERoute::kInput, 1) ? inputs[1] : inputs[0], bytes);

    const double bpm = GetTempo();
    const double ppq = GetPPQPos();
    const bool playing = GetTransportIsRunning();
    const double beatMs = bpm > 0.0 ? 60000.0 / bpm : std::numeric_limits<double>::quiet_NaN();

    const int unit = currentUnit();
    const int effectIndex = currentEffect (unit);
    const auto& info = getEffectInfo (unit, effectIndex);
    const int base = knobBase[(size_t) unit][(size_t) effectIndex];

    std::array<int, kMaxKnobs> values {};
    for (int i = 0; i < info.knobCount; ++i)
    {
        const auto& knob = info.knobs[i];
        const int raw = (int) std::lround (GetParam (base + i)->Value());
        values[(size_t) i] = knob.kind == KnobKind::Choice ? knob.min + raw : raw;
    }

    // Flipping De-pop restarts the effect's state, as switching effects does.
    IEngine* depopEngine = depopEngines[(size_t) unit][(size_t) effectIndex].get();
    IEngine& engine = GetParam (kDepopParam)->Bool() && depopEngine != nullptr
                          ? *depopEngine
                          : *engines[(size_t) unit][(size_t) effectIndex];

    if (&engine != currentEngine)
    {
        currentEngine = &engine;
        engine.prepare (context, values.data(), beatMs);
        alignPending = true;
        lastPpqValid = false;
    }
    else
    {
        engine.update (values.data(), beatMs);
    }

    if (unit == kUnitIntellifex)
    {
        // The device's single A/D sees the sum of its two inputs.
        for (int i = 0; i < nFrames; ++i)
            left[i] = 0.5f * (left[i] + right[i]);

        for (int position = 0; position < nFrames;)
        {
            const int chunk = std::min (nFrames - position, maxHostFrames);
            const int deviceFrames = ifxResampler.down (left + position, chunk);
            engine.process (ifxResampler.getDeviceIn(), nullptr, ifxResampler.getDeviceOut (0),
                            ifxResampler.getDeviceOut (1), deviceFrames);
            ifxResampler.up (deviceFrames, left + position, right + position, chunk);
            position += chunk;
        }

        return;
    }

    if (engine.isValid() && playing && std::isfinite (ppq))
    {
        double alignedPpq = ppq;

        if (resampleFactor > 1)
            alignedPpq -= (resampler.getLatencySamples() * 0.5) * bpm / (60.0 * hostSampleRate);

        bool doAlign = alignPending || ! wasPlaying || ! lastPpqValid;

        if (! doAlign && lastPpqValid)
        {
            const double expected = lastPpq + (double) nFrames * bpm / (60.0 * hostSampleRate);
            doAlign = std::abs (alignedPpq - expected) > 0.01;
        }

        if (doAlign)
            engine.align (alignedPpq);

        lastPpq = alignedPpq;
        lastPpqValid = true;
        alignPending = false;
    }
    else
    {
        lastPpqValid = false;
    }

    wasPlaying = playing;

    if (passThrough)
        return;

    // An invalid engine still runs through the resampler, so dry audio keeps the reported latency.

    if (resampleFactor == 1)
    {
        engine.process (left, right, left, right, nFrames);
        return;
    }

    for (int position = 0; position < nFrames;)
    {
        const int chunk = std::min (nFrames - position, maxHostFrames);
        const int baseFrames = resampler.down (left + position, right + position, chunk);

        if (baseFrames > 0)
            engine.process (resampler.getBase (0), resampler.getBase (1), resampler.getBase (0),
                            resampler.getBase (1), baseFrames);

        resampler.up (baseFrames, left + position, right + position, chunk);
        position += chunk;
    }
}

Steinberg::tresult PLUGIN_API VintageDigital::initialize (Steinberg::FUnknown* host)
{
    const Steinberg::tresult result = Plugin::initialize (host);
    if (result != Steinberg::kResultOk)
        return result;

    // iPlug2 caps parameter names at 31 characters, too short for "Effect: Knob".
    for (int unit = 0; unit < kUnitCount; ++unit)
    {
        for (int effect = 0; effect < getEffectCount (unit); ++effect)
        {
            const auto& info = getEffectInfo (unit, effect);

            for (int knob = 0; knob < info.knobCount; ++knob)
            {
                const auto tag = (Steinberg::Vst::ParamID) (knobBase[(size_t) unit][(size_t) effect] + knob);
                if (auto* parameter = parameters.getParameter (tag))
                {
                    const std::string title = std::string (info.name) + ": " + info.knobs[knob].name;
                    Steinberg::UString (parameter->getInfo().title, str16BufferSize (Steinberg::Vst::String128))
                        .fromAscii (title.c_str());
                }
            }
        }
    }

    return result;
}

Steinberg::tresult PLUGIN_API VintageDigital::getParamValueByString (Steinberg::Vst::ParamID tag,
                                                                     Steinberg::Vst::TChar* string,
                                                                     Steinberg::Vst::ParamValue& valueNormalized)
{
    // iPlug2 reads typed values as plain numbers; the knobs need their units, e.g. "2 kHz".
    const KnobDef* knob = tag < paramKnob.size() ? paramKnob[tag].knob : nullptr;
    if (knob == nullptr || knob->kind != KnobKind::Int)
        return Plugin::getParamValueByString (tag, string, valueNormalized);

    Steinberg::String text (string);
    const int raw = parseKnobValue (*knob, text.text8(), linkedValue (paramKnob[tag].linkParam));
    valueNormalized = GetParam ((int) tag)->ToNormalized (raw);
    return Steinberg::kResultTrue;
}
