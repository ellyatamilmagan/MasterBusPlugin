#pragma once
#include "PluginProcessor.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attach;   // destroyed first
    };

    std::array<Knob, 4> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
