#pragma once

#include "EffectEngine.h"
#include "Resampler.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <memory>
#include <vector>

class GMajor2AudioProcessor final : public juce::AudioProcessor
{
public:
    GMajor2AudioProcessor();
    ~GMajor2AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::vector<std::unique_ptr<IEngine>> engines;
    std::vector<std::vector<std::atomic<float>*>> knobValues;
    std::atomic<float>* effectValue = nullptr;

    gm2_context context {};
    Resampler resampler;
    double hostSampleRate = 44100.0;
    int maxHostFrames = 1;
    int resampleFactor = 1;
    bool passThrough = false;

    int currentEffect = -1;
    double lastPpq = 0.0;
    bool lastPpqValid = false;
    bool wasPlaying = false;
    bool alignPending = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GMajor2AudioProcessor)
};
