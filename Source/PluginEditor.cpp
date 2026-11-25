#include "PluginEditor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
constexpr int kMargin = 12;
constexpr int kRowHeight = 28;
constexpr int kGap = 6;
constexpr int kComboHeight = 28;
constexpr int kLabelWidth = 150;
constexpr int kWidth = 480;
constexpr int kRowsHeight = 11 * kRowHeight + 10 * kGap;
constexpr int kHeight = kMargin + kComboHeight + kGap + kRowsHeight + kMargin;
} // namespace

GMajor2AudioProcessorEditor::GMajor2AudioProcessorEditor (GMajor2AudioProcessor& processorToUse)
    : AudioProcessorEditor (&processorToUse), processor (processorToUse)
{
    setResizable (false, false);
    setSize (kWidth, kHeight);

    juce::String previousFamily;

    for (int i = 0; i < getEffectCount(); ++i)
    {
        const auto& info = getEffectInfo (i);

        if (previousFamily != info.family)
        {
            effectBox.addSectionHeading (info.family);
            previousFamily = info.family;
        }

        effectBox.addItem (info.name, i + 1);
    }

    addAndMakeVisible (effectBox);
    effectAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, "effect", effectBox);

    processor.apvts.addParameterListener ("effect", this);
    rebuildRows();
}

GMajor2AudioProcessorEditor::~GMajor2AudioProcessorEditor()
{
    processor.apvts.removeParameterListener ("effect", this);
    cancelPendingUpdate();

    sliderAttachments.clear();
    comboAttachments.clear();
    rowComponents.clear();
    effectAttachment.reset();
}

void GMajor2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void GMajor2AudioProcessorEditor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == "effect")
        triggerAsyncUpdate();
}

void GMajor2AudioProcessorEditor::handleAsyncUpdate()
{
    rebuildRows();
}

void GMajor2AudioProcessorEditor::rebuildRows()
{
    sliderAttachments.clear();
    comboAttachments.clear();
    rowComponents.clear();

    const auto* effectValue = processor.apvts.getRawParameterValue ("effect");
    const int effectIndex = juce::jlimit (0, getEffectCount() - 1,
                                          juce::roundToInt (effectValue->load()));
    const auto& info = getEffectInfo (effectIndex);

    for (int i = 0; i < info.knobCount; ++i)
    {
        const auto& knob = info.knobs[i];
        const juce::String parameterID = juce::String (info.key) + "_" + knob.field;

        auto label = std::make_unique<juce::Label>();
        label->setText (knob.name, juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*label);
        rowComponents.push_back (std::move (label));

        if (knob.kind == KnobKind::Choice)
        {
            auto combo = std::make_unique<juce::ComboBox>();

            for (int choice = 0; choice < knob.choices->count; ++choice)
                combo->addItem (knob.choices->items[choice], choice + 1);

            addAndMakeVisible (*combo);
            comboAttachments.push_back (
                std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                    processor.apvts, parameterID, *combo));
            rowComponents.push_back (std::move (combo));
        }
        else
        {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 96, 24);
            addAndMakeVisible (*slider);
            sliderAttachments.push_back (
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    processor.apvts, parameterID, *slider));
            rowComponents.push_back (std::move (slider));
        }
    }

    resized();
}

void GMajor2AudioProcessorEditor::resized()
{
    effectBox.setBounds (kMargin, kMargin, getWidth() - 2 * kMargin, kComboHeight);

    const int controlX = kMargin + kLabelWidth + 8;
    const int controlWidth = getWidth() - controlX - kMargin;
    int y = kMargin + kComboHeight + kGap;

    for (size_t i = 0; i + 1 < rowComponents.size(); i += 2)
    {
        rowComponents[i]->setBounds (kMargin, y, kLabelWidth, kRowHeight);
        rowComponents[i + 1]->setBounds (controlX, y, controlWidth, kRowHeight);
        y += kRowHeight + kGap;
    }
}
