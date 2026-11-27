#include "IMajor2.h"

#include "IPlug_include_in_plug_src.h"

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

static_assert (std::is_same_v<sample, float>, "the library processes float; build with SAMPLE_TYPE_FLOAT");

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
    for (int effect = 0; effect < getEffectCount(); ++effect)
        count += getEffectInfo (effect).knobCount;

    return count;
}
} // namespace

IMajor2::IMajor2 (const InstanceInfo& info)
    : Plugin (info, MakeConfig (parameterCount(), 0))
{
    IParam* effectParam = GetParam (kEffectParam);
    effectParam->InitEnum ("Effect", 0, getEffectCount());
    for (int effect = 0; effect < getEffectCount(); ++effect)
        effectParam->SetDisplayText (effect, getEffectInfo (effect).name);

    GetParam (kDepopParam)->InitBool ("De-pop", true, "", 0, "", "Off", "On");

    const int count = getEffectCount();
    knobBase.reserve ((size_t) count);
    engines.reserve ((size_t) count);
    depopEngines.reserve ((size_t) count);
    paramKnob.assign ((size_t) NParams(), nullptr);

    int param = kFirstKnobParam;

    for (int effect = 0; effect < count; ++effect)
    {
        const auto& effectInfo = getEffectInfo (effect);
        knobBase.push_back (param);
        engines.push_back (createEffectEngine (effect));
        depopEngines.push_back (createDepopEngine (effect));

        for (int knobIndex = 0; knobIndex < effectInfo.knobCount; ++knobIndex, ++param)
        {
            const KnobDef& knob = effectInfo.knobs[knobIndex];
            IParam* knobParam = GetParam (param);
            paramKnob[(size_t) param] = &knob;

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
                knobParam->SetDisplayFunc ([&knob] (double value, WDL_String& display) {
                    display.Set (formatKnobValue (knob, (int) std::lround (value)).c_str());
                });
            }
        }
    }

    mMakeGraphicsFunc = [this] {
        return MakeGraphics (*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                             GetScaleForScreen (PLUG_WIDTH, PLUG_HEIGHT));
    };

    mLayoutFunc = [this] (IGraphics* graphics) { layoutEditor (*graphics); };
}

int IMajor2::currentEffect() const
{
    return std::clamp ((int) std::lround (GetParam (kEffectParam)->Value()), 0, getEffectCount() - 1);
}

void IMajor2::OnReset()
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
    }
    else
    {
        resampler.reset();
    }

    const int latency = resampleFactor > 1 ? resampler.getLatencySamples() : 0;
    if (latency != GetLatency())
        SetLatency (latency);

    currentEngine = nullptr;
    lastPpqValid = false;
    wasPlaying = false;
    alignPending = false;
}

void IMajor2::ProcessBlock (sample** inputs, sample** outputs, int nFrames)
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

    const int effectIndex = currentEffect();
    const auto& info = getEffectInfo (effectIndex);

    std::array<int, kMaxKnobs> values {};
    for (int i = 0; i < info.knobCount; ++i)
    {
        const auto& knob = info.knobs[i];
        const int raw = (int) std::lround (GetParam (knobBase[(size_t) effectIndex] + i)->Value());
        values[(size_t) i] = knob.kind == KnobKind::Choice ? knob.min + raw : raw;
    }

    // Flipping De-pop restarts the effect's state, as switching effects does.
    const auto& depopEngine = depopEngines[(size_t) effectIndex];
    IEngine& engine = GetParam (kDepopParam)->Bool() && depopEngine != nullptr
                          ? *depopEngine
                          : *engines[(size_t) effectIndex];

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

Steinberg::tresult PLUGIN_API IMajor2::initialize (Steinberg::FUnknown* host)
{
    const Steinberg::tresult result = Plugin::initialize (host);
    if (result != Steinberg::kResultOk)
        return result;

    // iPlug2 caps parameter names at 31 characters, too short for "Effect: Knob".
    for (int effect = 0; effect < getEffectCount(); ++effect)
    {
        const auto& info = getEffectInfo (effect);

        for (int knob = 0; knob < info.knobCount; ++knob)
        {
            const auto tag = (Steinberg::Vst::ParamID) (knobBase[(size_t) effect] + knob);
            if (auto* parameter = parameters.getParameter (tag))
            {
                const std::string title = std::string (info.name) + ": " + info.knobs[knob].name;
                Steinberg::UString (parameter->getInfo().title, str16BufferSize (Steinberg::Vst::String128))
                    .fromAscii (title.c_str());
            }
        }
    }

    return result;
}

Steinberg::tresult PLUGIN_API IMajor2::getParamValueByString (Steinberg::Vst::ParamID tag,
                                                              Steinberg::Vst::TChar* string,
                                                              Steinberg::Vst::ParamValue& valueNormalized)
{
    // iPlug2 reads typed values as plain numbers; the knobs need their units, e.g. "2 kHz".
    const KnobDef* knob = tag < paramKnob.size() ? paramKnob[tag] : nullptr;
    if (knob == nullptr || knob->kind != KnobKind::Int)
        return Plugin::getParamValueByString (tag, string, valueNormalized);

    Steinberg::String text (string);
    valueNormalized = GetParam ((int) tag)->ToNormalized (parseKnobValue (*knob, text.text8()));
    return Steinberg::kResultTrue;
}
