#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p) : AudioProcessorEditor (p)
{
    const std::array<std::pair<const char*, const char*>, 4> defs {{
        { ids::inGain,     "Input Gain"        },
        { ids::lowEnd,     "Low-End Collision" },
        { ids::saturation, "Saturation"        },
        { ids::limiter,    "Brickwall Limiter" }
    }};

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        k.label.setText (defs[i].second, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.attachToComponent (&k.slider, false);
        addAndMakeVisible (k.slider);
        k.attach = std::make_unique<SliderAttachment> (p.apvts, defs[i].first, k.slider);
    }

    setSize (480, 220);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (24);                                   // room for attached labels
    const int w = area.getWidth() / (int) knobs.size();

    for (auto& k : knobs)
        k.slider.setBounds (area.removeFromLeft (w));
}
