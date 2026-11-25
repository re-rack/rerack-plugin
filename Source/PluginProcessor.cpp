#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
double knobUnitValue (const KnobDef& knob, int raw)
{
    switch (knob.unit)
    {
        case KnobUnit::Speed: return 0.05 * std::pow (10.0, (double) raw / 80.0);
        case KnobUnit::Cut: return 20.0 * std::pow (10.0, (double) raw / 20.0);
        case KnobUnit::EqFreq: return std::pow (10.0, 1.0 + ((double) raw + 24.0) / 80.0);
        case KnobUnit::EqWidth: return std::pow (2.0, ((double) raw - 10.0) / 3.0);
        case KnobUnit::AdvDelayMs: return (double) raw / 10.0;
        case KnobUnit::ReverbDecay: return (double) raw / 10.0;
        case KnobUnit::UnivibeSeconds: return (double) raw * 0.05;
        case KnobUnit::Plain:
        case KnobUnit::Percent:
        case KnobUnit::Decibels:
        case KnobUnit::Milliseconds:
        case KnobUnit::Cents:
        case KnobUnit::DecibelsPerSecond:
        case KnobUnit::GateThreshold: break;
    }

    return (double) raw;
}

juce::String formatHz (double hz)
{
    if (hz < 100.0)
        return juce::String (hz, 1) + " Hz";

    if (hz < 1000.0)
        return juce::String (juce::roundToInt (hz)) + " Hz";

    return juce::String (hz / 1000.0, 2) + " kHz";
}

juce::String formatKnobValue (const KnobDef& knob, int raw)
{
    switch (knob.unit)
    {
        case KnobUnit::Speed:
            if (knob.min == 37 && raw == 37)
                return "Stop";

            {
                const double hz = knobUnitValue (knob, raw);
                return juce::String (hz, hz < 1.0 ? 3 : 2) + " Hz";
            }

        case KnobUnit::Cut:
            return formatHz (knobUnitValue (knob, raw));

        case KnobUnit::EqFreq:
            if (raw == 241)
                return "Off";

            return formatHz (knobUnitValue (knob, raw));

        case KnobUnit::EqWidth:
            return juce::String (knobUnitValue (knob, raw), 2) + " oct";

        case KnobUnit::AdvDelayMs:
            return juce::String (knobUnitValue (knob, raw), 1) + " ms";

        case KnobUnit::ReverbDecay:
            return juce::String (knobUnitValue (knob, raw), 1) + " s";

        case KnobUnit::UnivibeSeconds:
            return juce::String (knobUnitValue (knob, raw), 2) + " s";

        case KnobUnit::GateThreshold:
            if (raw == -61)
                return "Off";

            return juce::String (raw) + " dB";

        case KnobUnit::Percent: return juce::String (raw) + " %";
        case KnobUnit::Decibels: return juce::String (raw) + " dB";
        case KnobUnit::Milliseconds: return juce::String (raw) + " ms";
        case KnobUnit::Cents: return juce::String (raw) + " cents";
        case KnobUnit::DecibelsPerSecond: return juce::String (raw) + " dB/s";
        case KnobUnit::Plain: break;
    }

    return juce::String (raw);
}

int parseKnobValue (const KnobDef& knob, const juce::String& text)
{
    if (knob.unit == KnobUnit::GateThreshold && text.containsIgnoreCase ("off"))
        return -61;

    if (knob.unit == KnobUnit::EqFreq && text.containsIgnoreCase ("off"))
        return 241;

    if (knob.unit == KnobUnit::Speed && knob.min == 37 && text.containsIgnoreCase ("stop"))
        return 37;

    double target = text.getFloatValue();

    if ((knob.unit == KnobUnit::Cut || knob.unit == KnobUnit::EqFreq)
        && text.toLowerCase().contains ("khz"))
        target *= 1000.0;

    int best = knob.min;
    double bestDistance = std::numeric_limits<double>::max();

    for (int raw = knob.min; raw <= knob.max; ++raw)
    {
        const double distance = std::abs (knobUnitValue (knob, raw) - target);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = raw;
        }
    }

    return best;
}
} // namespace

GMajor2AudioProcessor::GMajor2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    const int count = getEffectCount();
    engines.reserve ((size_t) count);
    knobValues.resize ((size_t) count);

    for (int effect = 0; effect < count; ++effect)
    {
        engines.push_back (createEffectEngine (effect));

        const auto& info = getEffectInfo (effect);
        knobValues[(size_t) effect].resize ((size_t) info.knobCount);

        for (int knob = 0; knob < info.knobCount; ++knob)
            knobValues[(size_t) effect][(size_t) knob]
                = apvts.getRawParameterValue (juce::String (info.key) + "_" + info.knobs[knob].field);
    }

    effectValue = apvts.getRawParameterValue ("effect");
}

juce::AudioProcessorValueTreeState::ParameterLayout GMajor2AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray effectNames;
    for (int i = 0; i < getEffectCount(); ++i)
        effectNames.add (getEffectInfo (i).name);

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "effect", 1 },
                                                             "Effect", effectNames, 0));

    for (int effect = 0; effect < getEffectCount(); ++effect)
    {
        const auto& info = getEffectInfo (effect);

        for (int knobIndex = 0; knobIndex < info.knobCount; ++knobIndex)
        {
            const auto knob = info.knobs[knobIndex];
            const juce::String id = juce::String (info.key) + "_" + knob.field;
            const juce::String name = juce::String (info.name) + ": " + knob.name;

            if (knob.kind == KnobKind::Choice)
            {
                juce::StringArray choices;
                for (int i = 0; i < knob.choices->count; ++i)
                    choices.add (knob.choices->items[i]);

                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { id, 1 }, name, choices, knob.def - knob.min));
            }
            else
            {
                auto attributes = juce::AudioParameterIntAttributes()
                                      .withStringFromValueFunction (
                                          [knob] (int value, int) { return formatKnobValue (knob, value); })
                                      .withValueFromStringFunction (
                                          [knob] (const juce::String& text) { return parseKnobValue (knob, text); });

                layout.add (std::make_unique<juce::AudioParameterInt> (
                    juce::ParameterID { id, 1 }, name, knob.min, knob.max, knob.def, attributes));
            }
        }
    }

    return layout;
}

void GMajor2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxHostFrames = std::max (1, samplesPerBlock);

    passThrough = false;
    resampleFactor = 1;
    bool found = false;

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
            found = true;
            break;
        }
    }

    if (! found)
    {
        passThrough = true;
        resampleFactor = 0;
    }

    if (resampleFactor > 1)
    {
        resampler.prepare (resampleFactor, (double) context.sample_rate, maxHostFrames);
        setLatencySamples (resampler.getLatencySamples());
    }
    else
    {
        setLatencySamples (0);
    }

    currentEffect = -1;
    lastPpqValid = false;
    wasPlaying = false;
    alignPending = false;
}

void GMajor2AudioProcessor::releaseResources()
{
    resampler.reset();
}

bool GMajor2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto input = layouts.getMainInputChannelSet();
    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void GMajor2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    if (getTotalNumInputChannels() == 0)
    {
        buffer.clear();
        return;
    }

    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    double bpm = 0.0;
    double ppq = std::numeric_limits<double>::quiet_NaN();
    bool playing = false;

    if (auto* head = getPlayHead())
    {
        const auto position = head->getPosition();
        if (position.hasValue())
        {
            if (const auto tempo = position->getBpm(); tempo.hasValue())
                bpm = *tempo;

            if (const auto ppqPosition = position->getPpqPosition(); ppqPosition.hasValue())
                ppq = *ppqPosition;

            playing = position->getIsPlaying();
        }
    }

    const double beatMs = bpm > 0.0 ? 60000.0 / bpm : std::numeric_limits<double>::quiet_NaN();

    const int effectIndex = juce::jlimit (0, getEffectCount() - 1,
                                          juce::roundToInt (effectValue->load()));
    const auto& info = getEffectInfo (effectIndex);

    std::array<int, kMaxKnobs> values {};
    for (int i = 0; i < info.knobCount; ++i)
    {
        const auto& knob = info.knobs[i];
        const float raw = knobValues[(size_t) effectIndex][(size_t) i]->load();
        values[(size_t) i] = knob.kind == KnobKind::Choice
                                 ? knob.min + juce::roundToInt (raw)
                                 : juce::roundToInt (raw);
    }

    auto& engine = engines[(size_t) effectIndex];

    if (effectIndex != currentEffect)
    {
        currentEffect = effectIndex;
        engine->prepare (context, values.data(), beatMs);
        alignPending = true;
        lastPpqValid = false;
    }
    else
    {
        engine->update (values.data(), beatMs);
    }

    if (engine->isValid() && playing && std::isfinite (ppq))
    {
        double alignedPpq = ppq;

        if (resampleFactor > 1)
            alignedPpq -= (resampler.getLatencySamples() * 0.5) * bpm / (60.0 * hostSampleRate);

        bool doAlign = alignPending || ! wasPlaying || ! lastPpqValid;

        if (! doAlign && lastPpqValid)
        {
            const double expected = lastPpq + (double) numSamples * bpm / (60.0 * hostSampleRate);
            doAlign = std::abs (alignedPpq - expected) > 0.01;
        }

        if (doAlign)
            engine->align (alignedPpq);

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

    float* left = buffer.getWritePointer (0);
    float* right = buffer.getWritePointer (1);

    if (resampleFactor == 1)
    {
        engine->process (left, right, left, right, numSamples);
        return;
    }

    const int chunkMax = maxHostFrames > 0 ? maxHostFrames : numSamples;

    for (int position = 0; position < numSamples;)
    {
        const int chunk = std::min (numSamples - position, chunkMax);
        const int baseFrames = resampler.down (left + position, right + position, chunk);

        if (baseFrames > 0)
            engine->process (resampler.getBase (0), resampler.getBase (1), resampler.getBase (0),
                             resampler.getBase (1), baseFrames);

        resampler.up (baseFrames, left + position, right + position, chunk);
        position += chunk;
    }
}

juce::AudioProcessorEditor* GMajor2AudioProcessor::createEditor()
{
    return new GMajor2AudioProcessorEditor (*this);
}

void GMajor2AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GMajor2AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GMajor2AudioProcessor();
}
