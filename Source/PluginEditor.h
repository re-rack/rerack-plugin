#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

class GMajor2AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::AudioProcessorValueTreeState::Listener,
                                          private juce::AsyncUpdater
{
public:
    explicit GMajor2AudioProcessorEditor (GMajor2AudioProcessor& processorToUse);
    ~GMajor2AudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void rebuildRows();

    GMajor2AudioProcessor& processor;
    juce::ComboBox effectBox;
    juce::ToggleButton depopButton { "De-pop" };
    std::vector<std::unique_ptr<juce::Component>> rowComponents;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> effectAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> depopAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GMajor2AudioProcessorEditor)
};
